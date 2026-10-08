// HUD gauge emitter: FUN_8004FD30 (emitFrame) and FUN_8004FB4C (emitItem). Writes the guest's own
// DR_AREA packets and OT links; FUN_8004EB94 (centred 8x8 glyph row) and FUN_8005019C stay guest.
#include "hud_gauge_emitter.h"
#include "core.h"
#include "core/overrides/guest_jal.h"
#include "core/overrides/native_override_catalog.h"
#include "game.h"
#include "guest_abi.h"
#include "guest_call.h"
#include "guest_ordering_table.h"
#include <cstdint>

namespace {

// -------------------------------------------------------------------------------------------
// Guest addresses / constants (named, not inline hex — CLAUDE.md "no magic constant offsets").
constexpr uint32_t kHudGaugeBase = 0x800BF548u; // DAT_800bf548: the gauge table header + array
constexpr uint32_t kFlagOff = 1u;               // DAT_800bf549: draw-enable flag byte (==1 to draw)
constexpr uint32_t kCountOff = 8u;              // DAT_800bf550: active-item count (signed s16)
constexpr uint32_t kRecordsOff = 12u;           // first record = kHudGaugeBase + kRecordsOff
constexpr uint32_t kRecordStride = 140u;        // 0x8C, per docs/findings/render.md census note

// gen holds the pool-cursor page (r20) and the OT-base page (r19) live across the item's nested
// leaves, which spill them.
constexpr uint32_t kPktPoolBaseReg = tomba2::render::PacketPool::kCursorPage;
constexpr uint32_t kOtBaseReg = tomba2::render::OrderingTable::kBasePointerPage;
constexpr uint32_t kHudOtBucketIndex = 3u; // the fixed near/HUD bucket, never a Z-derived index
constexpr uint32_t kDrawAreaOtWords = 2u;  // DR_AREA's top-left + bottom-right words
constexpr uint32_t kDrawAreaOtTag = kDrawAreaOtWords << 24;
constexpr uint32_t kDrawAreaPacketBytes = 12u; // tag word + 2 data words = 3 words = 12 bytes;
                                               // matches FUN_80081CF8 setting its length byte to 2.

constexpr uint32_t kFunBuildDrawArea = 0x80081CF8u; // FUN_80081cf8(pkt, ushort rect[4]{x,y,w,h}):
                                                    // still-substrate DR_AREA packet-header builder
                                                    // (SetDrawAreaTopLeft/BottomRight word builders
                                                    // at 0x80082240/0x800822D8 are already owned —
                                                    // see game/render/wide_re_gpu_putdrawenv.cpp —
                                                    // but the packet-header assembly leaf itself
                                                    // isn't; called exactly as gen calls it).
constexpr uint32_t kFunSegmentLayout = 0x8004EB94u; // FUN_8004eb94(descAddr, signed16 span): still-
                                                    // substrate per-segment layout leaf.
constexpr uint32_t kFunLabelOrDigits = 0x8005019Cu; // FUN_8005019c(rectAddr, byte, 0, 3): still-
                                                    // substrate digit/label draw leaf.

// jal-site return-address constants, one per call site, in program order (mirrors gen exactly —
// FUN_80081CF8's own callee-saved footprint spills r16/r17/r31, so a wrong/garbage r31 here would
// surface as a genuine SBS diff in that leaf's own stack spill).
constexpr uint32_t kRaFrameTileA = 0x8004FDA8u;
constexpr uint32_t kRaFrameCallItem = 0x8004FDE4u;
constexpr uint32_t kRaFrameTileB = 0x8004FE4Cu;
constexpr uint32_t kRaItemTileA = 0x8004FC08u;
constexpr uint32_t kRaItemSeg1 = 0x8004FC44u;
constexpr uint32_t kRaItemSeg2 = 0x8004FC74u;
constexpr uint32_t kRaItemTileB = 0x8004FCB8u;
constexpr uint32_t kRaItemSegElse = 0x8004FCF4u;
constexpr uint32_t kRaItemFinal = 0x8004FD08u;

// Full-viewport scissor reset (FD30, before the item loop) — dims match the task's "0x140x0xf0".
constexpr uint16_t kViewportX = 0, kViewportW = 0x140, kViewportH = 0xF0;
// Status-panel scissor (FD30, after the item loop) — dims match the task's "0x120x0x36"; the
// panel sits inset from the viewport by (16px, +153 vertical scanlines).
constexpr uint16_t kPanelX = 0x10, kPanelW = 0x120, kPanelH = 0x36;
constexpr uint16_t kPanelYBias = 0x99; // 153

// Item record byte offsets within the 0x8C-stride array (see HudGaugeItemRecord below).
constexpr uint32_t kSegPrimaryOff = 16u;   // 0x10 — first segment-layout leaf's descriptor arg
constexpr uint32_t kSegSecondaryOff = 61u; // 0x3D — second segment-layout leaf's descriptor arg
constexpr uint32_t kLabelByteOff = 136u;   // 0x88 — byte forwarded to the digit/label leaf

// -------------------------------------------------------------------------------------------
// Scratchpad camera/vertical-scroll byte (DAT_1f800135) — read at every DR_AREA rect this
// emitter builds; always packed as the rect's Y in an 8.8-style fixed point (byte << 8).
uint16_t hudViewportY(Core *c) {
  return (uint16_t)(c->mem_r8(0x1F800135u) << 8);
}

// Item record lens: 0x8C-byte-stride gauge-item record (see docs/findings/render.md census
// note). Field roles below are exactly what gen reads at each offset, traced instruction-by-
// instruction against guest 0x8004FB4C.
struct HudGaugeItemRecord {
  Core *c;
  uint32_t addr;

  uint8_t kind() const {
    return c->mem_r8(addr + 10);
  } // 0/>=3 = single-segment; 1/2 = also
    // draws its own item-box + tile A.
  uint16_t spanBase() const {
    return c->mem_r16(addr + 2);
  } // segment span base
  uint8_t spanBias() const {
    return c->mem_r8(addr + 11);
  } // segment span bias byte, added in
  uint8_t labelByte() const {
    return c->mem_r8(addr + kLabelByteOff);
  }
};

// -------------------------------------------------------------------------------------------
// Build the {x,y,w,h} ushort rect FUN_80081CF8 reads as its param_2, at sp+off..off+6.
void buildDrawAreaRect(Core *c, uint32_t spOff, uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
  uint32_t sp = c->r[29];
  c->mem_w16(sp + spOff + 0, x);
  c->mem_w16(sp + spOff + 2, y);
  c->mem_w16(sp + spOff + 4, w);
  c->mem_w16(sp + spOff + 6, h);
}

// Emit the DR_AREA packet built from the sp+rectOff rect into the packet pool, then tail-append
// it into the fixed HUD OT bucket. GuestReg<16> mirrors gen's own r16 = old-pool-cursor
// assignment (FUN_80081CF8's callee-saved footprint spills r16/r17 — a wrong live value there is
// a real SBS diff, not residual scratch: CLAUDE.md "MIRROR THE GUEST STACK").
void emitDrawAreaAndLink(Core *c, uint32_t raConst, uint32_t rectOff) {
  GuestReg<16> pktAddr(c);
  pktAddr = tomba2::render::PacketPool(*c).allocate(kDrawAreaPacketBytes);
  tomba::guest::dispatchJalToReturn(*c, kFunBuildDrawArea, raConst, (uint32_t)pktAddr, c->r[29] + rectOff);

  // gen leaves the DR_AREA tag length word live in r3/v1, the leaf's v1 return residue.
  c->r[3] = kDrawAreaOtTag;
  const uint32_t tagWord =
      tomba2::render::OrderingTable::active(*c).link((uint32_t)pktAddr, kDrawAreaOtWords, kHudOtBucketIndex);
  // gen computes this tag word in r2 as its literal last step before falling through to the
  // caller's epilogue — a real v0 return-register residue at both of emitFrame's call sites
  // (MIRROR_VERIFY caught this: without it, v0 held stale garbage from unrelated earlier code).
  c->r[2] = tagWord;
}

// FUN_8004eb94(descAddr, sign_extend16(spanBase + spanBias + bias)) call shape, shared by all
// three segment-layout call sites (primary/secondary/else).
void emitSegmentLayout(Core *c, uint32_t raConst, uint32_t descAddr, const HudGaugeItemRecord &rec, int32_t bias) {
  const uint16_t sum = (uint16_t)(rec.spanBase() + rec.spanBias() + bias);
  tomba::guest::dispatchJalToReturn(*c, kFunSegmentLayout, raConst, descAddr, (uint32_t)(int32_t)(int16_t)sum);
}

} // namespace

void HudGaugeEmitter::emitFrame(Core *c) {
  static constexpr GuestFrameSpill kSpills[] = {{18, 32}, {31, 36}, {17, 28}, {16, 24}};
  GuestFrame<40, 4> frame(c, kSpills);
  constexpr uint32_t kRectOff = 16;

  GuestReg<18> recordsBase(c);
  recordsBase = kHudGaugeBase;

  const int32_t count = c->mem_r16s(kHudGaugeBase + kCountOff);
  if (count == 0) {
    c->r[2] = 1;
    return;
  }

  if (c->mem_r8(kHudGaugeBase + kFlagOff) == 1) {
    buildDrawAreaRect(c, kRectOff, kViewportX, hudViewportY(c), kViewportW, kViewportH);
    emitDrawAreaAndLink(c, kRaFrameTileA, kRectOff);
  }

  if (count > 0) {
    GuestReg<16> loopIdx(c);
    GuestReg<17> byteOff(c);
    byteOff = kRecordsOff;
    for (loopIdx = 0; (int32_t)loopIdx < count;) {
      c->r[4] = (uint32_t)recordsBase + (uint32_t)byteOff;
      c->r[31] = kRaFrameCallItem;
      HudGaugeEmitter::emitItem(c);
      loopIdx = (uint32_t)loopIdx + 1;
      byteOff = (uint32_t)byteOff + kRecordStride;
    }
  }

  // gen (L_8004FDF8) reloads the flag byte into r3 and r2 = 1 here, regardless of loop outcome.
  // When flag != 1 it exits immediately, leaving r3 = flag as the leaf's v1 residue and r2 = 1 as
  // v0 (SBS core A MIRROR_VERIFY caught native leaving a nested leaf's r3=0x09000000 on a flag==2
  // item). When flag == 1 it draws tile B and emitDrawAreaAndLink overwrites both r2/r3 with the
  // OT-tag residue. Mirror both branches exactly.
  const uint8_t flag = c->mem_r8(kHudGaugeBase + kFlagOff);
  c->r[2] = 1;
  c->r[3] = flag;
  if (flag == 1) {
    buildDrawAreaRect(c, kRectOff, kPanelX, (uint16_t)(hudViewportY(c) + kPanelYBias), kPanelW, kPanelH);
    emitDrawAreaAndLink(c, kRaFrameTileB, kRectOff);
  }
}

void HudGaugeEmitter::emitItem(Core *c) {
  // Frame: sp -= 64, spill s0..s6/ra at their RE'd offsets.
  static constexpr GuestFrameSpill kSpills[] = {
      {17, 36},
      {31, 60},
      {22, 56},
      {21, 52},
      {20, 48},
      {19, 44},
      {18, 40},
      {16, 32},
  };
  GuestFrame<64, 8> frame(c, kSpills);
  constexpr uint32_t kWord0Off = 16; // sp+16..19 — record's word0, inset -4 (rect x / y-high)
  constexpr uint32_t kWord1Off = 20; // sp+20..23 — record's word1, inset +8 (rect w / h)
  constexpr uint32_t kRectOff = 24;  // sp+24..30 — {x,y,w,h} scratch for FUN_80081CF8's param_2

  GuestReg<17> rec(c);
  rec = c->r[4]; // record address, held live for the whole leaf
  const uint32_t sp = c->r[29];
  const HudGaugeItemRecord item{c, (uint32_t)rec};

  // Stage the record's rect-inset words into stack scratch (an unaligned lwl/lwr load in gen —
  // functionally an ordinary 32-bit read/write pair; mirrored plainly). x0/y-high keep their raw
  // value; w0/h keep theirs too, only the LOW halves (x, w) get the -4/+8 inset applied.
  c->mem_w32(sp + kWord0Off, c->mem_r32((uint32_t)rec + 0));
  c->mem_w32(sp + kWord1Off, c->mem_r32((uint32_t)rec + 4));
  c->mem_w16(sp + kWord0Off, (uint16_t)((int16_t)c->mem_r16(sp + kWord0Off) - 4));
  c->mem_w16(sp + kWord1Off, (uint16_t)((int16_t)c->mem_r16(sp + kWord1Off) + 8));

  const uint8_t kind = item.kind();
  // gen loads the packet-pool base into r20 (32780<<16) in the branch-delay slot reached for every
  // kind<3 record (guest 0x8004FB4C:6609) and keeps it live through the record's nested leaves;
  // FUN_8005019C spills it (sp+48). For kind>=3 gen branches out one instruction earlier and never
  // loads it, so r20 stays the incoming value. Mirror exactly (SBS core A MIRROR_VERIFY caught this
  // as native r20=0 vs substrate 0x800C0000 in FUN_8005019C's spill).
  if (kind < 3) {
    c->r[20] = kPktPoolBaseReg;
  }
  if (kind < 3 && kind != 0) {
    // This item also owns its own scissor box: the fixed full-viewport tile (identical to
    // emitFrame's tile A) plus a per-item "itemBox" tile derived from the inset rect staged
    // above. Both DR_AREA calls reuse sp+24 (matches gen's r21 = sp+24, held live across both).
    // gen keeps the rect-scratch pointer live in r21 across both DR_AREA builds and the final
    // FUN_8005019C (which spills it, sp+52). Mirror it (SBS core A MIRROR_VERIFY caught native
    // r21=incoming vs substrate sp+24).
    c->r[21] = sp + kRectOff;
    buildDrawAreaRect(c, kRectOff, kViewportX, hudViewportY(c), kViewportW, kViewportH);
    emitDrawAreaAndLink(c, kRaItemTileA, kRectOff);

    // gen loads r19 = OT-base register and r18 = OT tag here (right after tile A, before the first
    // FUN_8004EB94), keeps them live through the segment leaves + FUN_8005019C which all spill
    // them (r18@sp+40, r19@sp+44). The tile-A OT link above already used the same constants
    // inline; these assignments make the SPILLED register values byte-match gen.
    c->r[19] = kOtBaseReg;
    c->r[18] = kDrawAreaOtTag;
    emitSegmentLayout(c, kRaItemSeg1, (uint32_t)rec + kSegPrimaryOff, item, 0);
    if (kind == 1) {
      emitSegmentLayout(c, kRaItemSeg2, (uint32_t)rec + kSegSecondaryOff, item, -8);
    }

    const uint16_t itemX = c->mem_r16(sp + kWord0Off);
    const uint16_t itemYHigh = c->mem_r16(sp + kWord0Off + 2);
    const uint16_t itemW = c->mem_r16(sp + kWord1Off);
    const uint16_t itemH = c->mem_r16(sp + kWord1Off + 2);
    buildDrawAreaRect(c, kRectOff, itemX, (uint16_t)(itemYHigh + hudViewportY(c)), itemW, itemH);
    emitDrawAreaAndLink(c, kRaItemTileB, kRectOff);
  } else {
    emitSegmentLayout(c, kRaItemSegElse, (uint32_t)rec + kSegPrimaryOff, item, 0);
  }

  tomba::guest::dispatchJalToReturn(*c, kFunLabelOrDigits, kRaItemFinal, sp + kWord0Off, item.labelByte(), 0u, 3u);
}

void HudGaugeEmitter::registerOverrides(Game *) {
  tomba::native::declareOverride(0x8004FD30u, "&HudGaugeEmitter::emitFrame", &HudGaugeEmitter::emitFrame);
  tomba::native::declareOverride(0x8004FB4Cu, "&HudGaugeEmitter::emitItem", &HudGaugeEmitter::emitItem);
}
