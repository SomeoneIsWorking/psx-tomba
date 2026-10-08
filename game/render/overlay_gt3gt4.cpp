// game/render/overlay_gt3gt4.cpp — native mirror of the A00-overlay GT3/GT4 packet-emitter
// cluster: the RE trace and the byte-exact transcription.
//
// RE: Ghidra headless decompile of a live seaside-field RAM dump (scratch/decomp/render146.c,
// FUN_80146478 / FUN_801465ec / FUN_801467bc) cross-checked against the recorded binary evidence's
// register-accurate translation (authenticated executable/overlay evidence overlay guest 0x80146478/801465EC,
// authenticated executable/overlay evidence overlay guest 0x801467BC — the recorded guest instruction listing is a
// strict per- instruction transcription so it is the more precise source for GTE register indices/opcodes, which
// Ghidra's COP2 decompilation garbles into placeholder immediates).
//
// FUN_80146478(rec_header, ot_base): uVar2 = *rec_header (low16 = GT3 count, high16 = GT4 count);
//   gt3(rec_header+16, ot_base, uVar2&0xffff) -> returns the GT4 array base; gt4(that, ot_base,
//   uVar2>>16). Owned natively as OverlayGt3Gt4::submitBlock since 2026-07-29 — it was left on the
//   substrate when the two leaves were ported and was, at 127,275 hits per 6000 replay frames, the
//   busiest remaining typed runtime address dispatch target in the game. Wiring the leaves directly (rather than only
//   through this dispatcher) is still what covers the OTHER call site: a duplicate tail-shared copy
//   of the same call sequence the recorded binary evidence folded into FUN_80147FC4.
//
// Both leaves emit POLY_GT3 / POLY_GT4 via the GTE (RTPT + NCLIP + AVSZ3/a custom near-clamped Z
// blend) into the shared packet-pool bump allocator + OT linked list. Every GTE op and guest write
// below is required.
#include "overlay_gt3gt4.h"
#include "cfg.h"
#include "core.h"
#include "core/overrides/native_override_catalog.h" // tomba::native::declareOverride — declared, not locally extern'd
#include "game.h"
#include "guest_call.h"
#include "guest_ordering_table.h"
#include "horizontal_visibility_cull.h"
#include "model_element.h"
#include "model_packet.h"
#include <cstddef>
#include <stdio.h>

#define COL_MASK                                                                                                       \
  0xFFF0F0F0u // low-nibble-per-byte clear on RGB889 words (matches the GPU;
              // same constant as game/render/submit.cpp)

// Shared near-plane-aware OTZ pick: given the record's "blend/flag" byte (top byte of the record's
// second colour word) and the 3 raw SZ FIFO values (GTE data regs 17/18/19, i.e. SZ1/SZ2/SZ3 —
// RTPT/RTPS write SZ1..SZ3, leaving SZ0 the prior value), reproduce the exact clamp-then-average
// the guest instruction path performs when flag != 0. flag&2 selects min-clamp (additive-style blend, whose
// nearer faces should NOT lose the depth sort to a farther match) vs max-clamp (subtractive-style).
static int32_t overlay_gt_z_blend(uint32_t flag, int32_t sz1, int32_t sz2, int32_t sz3) {
  int32_t a = sz1, b = sz2;
  if (flag & 2u) {
    if (a - b >= 0) {
      a = b; // additive: clamp a down to the nearer of (a,b)
    }
    int32_t r = a >> 2;
    if (a - sz3 >= 0) {
      r = sz3 >> 2; // then to the nearer of (that, sz3)
    }
    return r;
  } else {
    if (a - b <= 0) {
      a = b; // subtractive: clamp a up to the farther of (a,b)
    }
    int32_t r = a >> 2;
    if (a - sz3 <= 0) {
      r = sz3 >> 2; // then to the farther of (that, sz3)
    }
    return r;
  }
}

// Both leaves' OT bucket, or kNoBucket outside the exclusive (4, 0x7FF); a dropped record's
// packet-pool writes are orphaned, as in the guest.
static int32_t overlay_gt_otz_index(int32_t z) {
  using tomba2::render::OrderingTable;
  const int32_t idx = OrderingTable::compressDepth(z);
  return OrderingTable::inDepthRangeExclusive(idx) ? idx : OrderingTable::kNoBucket;
}

// The flag-byte bit that marks a record's texture as scrolling, and the packet UV halfwords the scroll
// moves. The GT3 copy adds the offset to uv0 twice and never to uv2.
static constexpr uint32_t kScrollFlag = 4u;
static constexpr uint32_t kGt3ScrolledUv[] = {12u, 24u, 12u};
static constexpr uint32_t kGt4ScrolledUv[] = {12u, 24u, 36u, 48u};

template <std::size_t N>
static void scrollUv(Core &c, uint32_t pool, uint32_t uvScroll, const uint32_t (&halfwords)[N]) {
  const int16_t offset = static_cast<int16_t>(c.mem_r16(uvScroll));
  for (const uint32_t halfword : halfwords) {
    c.mem_w16(pool + halfword, static_cast<uint16_t>(static_cast<int16_t>(c.mem_r16(pool + halfword)) + offset));
  }
}

// ---------------------------------------------------------------------------------------------
// FUN_80146478 — the FIELD SUBMIT BLOCK dispatcher.
//
// A submit block is a 16-byte header followed by a run of GT3 records (36 bytes each) and then a
// run of GT4 records (52 bytes each), packed back to back. Only the header's first word is read
// here: it carries BOTH run lengths, GT3 in the low half and GT4 in the high half. The GT3 leaf
// returns the address one past the last record it consumed — which is exactly where the GT4 run
// begins — so the two calls chain through v0 with no second length computed anywhere.
//
// This is the single busiest substrate-dispatch target in the game: a recdep-all histogram over
// 6000 frames of replays/bugs/seesaw-weight.pad counted 127,275 hits, 4x the runner-up, i.e. ~21
// per frame (one per submit block the field walker hands over). Both of its leaves have been
// natively owned since 2026-07-08; this wrapper was the last substrate hop between the walker and
// them.
//
// FAITHFUL SUBSTRATE MIRROR, like its leaves — it runs on both SBS cores and its guest writes (the
// 32-byte stack frame below) are part of the byte-exact state, so the frame is MIRRORED rather than
// elided: sp descends 32, s1/ra/s0 spill at +20/+24/+16 in that program order, and the two guest
// return-address constants are live in ra across the calls. Contract confirmed by
// `binary ABI evidence 0x80146478 --contract` (frame_size 32, 3 spills, 2 direct call sites), not by hand.
//
// The two leaves are reached through their GENERATED A00 overlay guest entries wrappers, not by calling
// OverlayGt3Gt4::gt3/gt4 directly, even though on this leg the table resolves to exactly those two
// natives. Calling them directly measurably works and is one indirection cheaper — but it bypasses
// the registry's per-address hit counters, and `PSXPORT_DEBUG=ovhit` then reports the two busiest
// render leaves in the game as "NEVER HIT (registered but unreached)". That is an instrument telling
// a plain lie about live code, and the whole point of overrides::coverage() is that unreached-vs-
// unhooked has to stay distinguishable. Keep the dispatch decision in the one place that counts it.
namespace {
constexpr uint32_t kFrameSize = 32;
constexpr uint32_t kSpillS0 = 16; // sp-relative, per the extracted contract
constexpr uint32_t kSpillS1 = 20;
constexpr uint32_t kSpillRa = 24;
constexpr uint32_t kBlockCounts = 0;          // low16 = GT3 record count, high16 = GT4 count
constexpr uint32_t kBlockRecords = 16;        // the records start past the 16-byte header
constexpr uint32_t kRaAfterGt3 = 0x8014649Cu; // guest return addresses, live in ra across each
constexpr uint32_t kRaAfterGt4 = 0x801464ACu; // call — not magic numbers, they are the RE'd PCs
} // namespace

void OverlayGt3Gt4::submitBlock(Core *c) {
  const uint32_t block = c->r[4];
  const uint32_t otBase = c->r[5];

  const uint32_t sp = c->r[29] - kFrameSize;
  c->r[29] = sp;
  c->mem_w32(sp + kSpillS1, c->r[17]);
  c->mem_w32(sp + kSpillRa, c->r[31]);
  c->mem_w32(sp + kSpillS0, c->r[16]);

  const uint32_t counts = c->mem_r32(block + kBlockCounts);
  c->r[16] = counts; // s0 — the packed pair, live across both calls
  c->r[17] = otBase; // s1 — the OT base, restored into a1 for the second call

  c->r[4] = block + kBlockRecords;
  c->r[5] = otBase;
  c->r[6] = counts & 0xFFFFu;
  c->r[31] = kRaAfterGt3;
  psx::cpu::dispatchGuestToReturn0(*c, 0x801465ECu, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);

  c->r[4] = c->r[2]; // the GT4 run begins where the GT3 run ended
  c->r[5] = c->r[17];
  c->r[6] = counts >> 16;
  c->r[31] = kRaAfterGt4;
  psx::cpu::dispatchGuestToReturn0(*c,
                                   0x801467BCu,
                                   psx::cpu::ExecutionBudget::currentTurn(*c),
                                   __func__); // its v0 falls through as this function's return value

  c->r[31] = c->mem_r32(sp + kSpillRa);
  c->r[17] = c->mem_r32(sp + kSpillS1);
  c->r[16] = c->mem_r32(sp + kSpillS0);
  c->r[29] = sp + kFrameSize;

  if (cfg_dbg("ovgt")) {
    static long n = 0;
    if (n++ % 512 == 0) {
      cfg_logf("ovgt", "submitBlock call#%ld block=%08X gt3=%u gt4=%u", n, block, counts & 0xFFFFu, counts >> 16);
    }
  }
}

// FUN_801465EC — POLY_GT3 (gouraud-textured triangle) emit, GTE-driven, guest-writing.
// Record = 36 bytes: {+0 rgb0|code, +4 rgb1(rgb2=rgb1<<4)|flag@[31:24], +8 uv0|clut, +12 uv1|tpage,
//   +16 VXY0, +20 VZ0(lo)|VZ1(hi), +24 VXY1, +28 VXY2, +32 VZ2(lo)|uv2hi(hi)}.
// Output POLY_GT3 packet = 40 bytes (10 words): {+0 OT tag(len=9<<24|next), +4 rgb0|code RAW
//   (unmasked — a real asymmetry vs the GT4 leaf below and vs submit.cpp's own GT3, verified by
//   the guest instruction path: this record's colour0 word never passes through COL_MASK), +8 SXY0,
//   +12 uv0|clut, +16 rgb1&MASK, +20 SXY1, +24 uv1|tpage, +28 rgb2&MASK, +32 SXY2, +36 uv2hi}.
void OverlayGt3Gt4::gt3(Core &core, uint32_t uvScroll, const tomba2::horizontal_cull::Visibility &visible) {
  Core *const c = &core;
  uint32_t rec = c->r[4], ot_base = c->r[5], count = c->r[6];
  if (cfg_dbg("ovgt")) {
    static long n = 0;
    if (n++ % 512 == 0) {
      cfg_logf("ovgt", "gt3 call#%ld count=%u", n, count);
    }
  }
  if (count == 0) {
    c->r[2] = rec;
    return;
  }
  uint32_t pool = tomba2::render::PacketPool(*c).cursor();
  const uint32_t list = rec;
  for (; count != 0; count--, rec += 36) {
    const auto primitive =
        c->emission.element(tomba2::render::modelElement(tomba2::render::ModelList::Gt3, (rec - list) / 36));
    gte_write_data(0, c->mem_r32(rec + 16)); // VXY0
    uint32_t vz01 = c->mem_r32(rec + 20);
    gte_write_data(2, c->mem_r32(rec + 24)); // VXY1
    gte_write_data(1, vz01);                 // VZ0
    gte_write_data(4, c->mem_r32(rec + 28)); // VXY2
    uint32_t vz23 = c->mem_r32(rec + 32);
    gte_write_data(3, vz01 >> 16);     // VZ1
    gte_write_data(5, vz23);           // VZ2
    c->mem_w32(pool + 36, vz23 >> 16); // uv2hi staged (pre-RTPT scratch write)
    gte_op(c, 0x4A280030u);            // RTPT (triple perspective transform)

    uint32_t uv0 = c->mem_r32(rec + 8), uv1 = c->mem_r32(rec + 12);
    uint32_t rgb0_code = c->mem_r32(rec + 0), rgb1_src = c->mem_r32(rec + 4);
    uint32_t flagreg = gte_read_ctrl(31);
    c->mem_w32(pool + 12, uv0);
    c->mem_w32(pool + 24, uv1); // both writes happen regardless of flag
    if ((int32_t)flagreg < 0) {
      continue; // GTE FLAG error -> drop this record
    }

    gte_store_xy(c, pool + 8, 12);  // SXY0
    gte_store_xy(c, pool + 20, 13); // SXY1
    gte_store_xy(c, pool + 32, 14); // SXY2
    int32_t sxy0 = (int32_t)gte_read_data(12), sxy1 = (int32_t)gte_read_data(13), sxy2 = (int32_t)gte_read_data(14);

    // Frustum reject, the recovered predicate: unsigned-compare the packed SXY words against the
    // frame HEIGHT, then (after <<16 each) against the draw WIDTH — an OR over corners on each axis,
    // the two axes AND-ed. The guest instruction path's own screen-bound test, at 0x8003B438 for the
    // quad form.
    //
    // WHAT CHANGED. The horizontal bound was a literal 320. A scan of all 29 authenticated images
    // found 70 structurally distinct culling owners and NOT ONE derives its bound from the guest's
    // own draw environment, so a
    // literal is narrow by construction the moment the frustum is widened, and this site was dropping
    // the ground in the new right band. The bound now comes from the window the port draws into; at
    // 4:3 that is 320 and the decision is bit-identical to the literal it replaces.
    //
    // The vertical bound STAYS kGuestFrameHeight, and that is a fact about the projection rather than
    // a leftover: the publication is guest 0x800509B4, which sets OFX/OFY = 160/120 and H = 350, and
    // widening OFX at unchanged OFY and H leaves the vertical field of view alone. The frame is 2*OFY
    // tall at every canvas width, so 240 is the height and only the width moves.
    const std::uint32_t height16 = static_cast<std::uint32_t>(tomba2::horizontal_cull::kGuestFrameHeight) << 16;
    if (!((static_cast<std::uint32_t>(sxy0) < height16) || (static_cast<std::uint32_t>(sxy1) < height16) ||
          (static_cast<std::uint32_t>(sxy2) < height16))) {
      continue;
    }
    const std::uint32_t xs[3] = {static_cast<std::uint32_t>(sxy0) << 16,
                                 static_cast<std::uint32_t>(sxy1) << 16,
                                 static_cast<std::uint32_t>(sxy2) << 16};
    if (!visible.keepsX(xs, 3, tomba2::horizontal_cull::Domain::Fixed16_16)) {
      continue;
    }

    gte_op(c, 0x4B400006u);                     // NCLIP (backface / MAC0)
    c->mem_w32(pool + 16, rgb1_src & COL_MASK); // rgb1
    c->mem_w32(pool + 4, rgb0_code);            // rgb0|code, UNMASKED (faithful)
    uint32_t rgb2 = (rgb1_src << 4) & COL_MASK;
    int32_t mac0 = (int32_t)gte_read_data(24);
    c->mem_w32(pool + 28, rgb2);
    if (mac0 <= 0) {
      continue; // backface cull
    }

    uint32_t flagbyte = rgb1_src >> 24;
    if (uvScroll != kNoScroll && (flagbyte & kScrollFlag) != 0) {
      scrollUv(core, pool, uvScroll, kGt3ScrolledUv);
    }
    int32_t z;
    if (flagbyte == 0) {
      gte_op(c, 0x4B58002Du); // AVSZ3 (straight OTZ average)
      z = (int32_t)gte_read_data(7);
    } else {
      int32_t sz1 = (int32_t)gte_read_data(17), sz2 = (int32_t)gte_read_data(18), sz3 = (int32_t)gte_read_data(19);
      z = overlay_gt_z_blend(flagbyte, sz1, sz2, sz3);
    }

    int32_t idx = overlay_gt_otz_index(z);
    if (idx < 0) {
      continue;
    }

    tomba2::render::OrderingTable(*c, ot_base).link(pool, 9u, (uint32_t)idx);
    pool += 40;
  }
  tomba2::render::PacketPool(*c).setCursor(pool);
  c->r[2] = rec;
}

// FUN_801467BC — POLY_GT4 (gouraud-textured quad) emit, GTE-driven, guest-writing.
// Record = 44 bytes: {+0 rgb0|code, +4 rgb2(rgb3=rgb2<<4)|flag@[31:24], +8 uv0|clut, +12 uv1|tpage,
//   +16 uv2(lo)|uv3(hi), +20 VXY0, +24 VZ0(lo)|VZ1(hi), +28 VXY1, +32 VXY2, +36 VZ2(lo)|VZ3(hi),
//   +40 VXY3}. VXY3/VZ3 are transformed by a separate RTPS (the GTE's RTPT only handles 3 points).
// Output POLY_GT4 packet = 52 bytes (13 words): {+0 tag(len=0xC<<24|next), +4 rgb0&MASK, +8 SXY0,
//   +12 uv0|clut, +16 rgb1&MASK, +20 SXY1, +24 uv1|tpage, +28 rgb2&MASK, +32 SXY2, +36 uv2,
//   +40 rgb3&MASK, +44 SXY3, +48 uv3}. Unlike the GT3 leaf above, rgb0 here IS masked — verified
// against the guest instruction path, not "fixed" to match GT3 (the asymmetry is faithful, not a bug).
void OverlayGt3Gt4::gt4(Core &core, uint32_t uvScroll) {
  Core *const c = &core;
  uint32_t rec = c->r[4], ot_base = c->r[5], count = c->r[6];
  if (count == 0) {
    c->r[2] = rec;
    return;
  }
  uint32_t pool = tomba2::render::PacketPool(*c).cursor();
  const uint32_t list = rec;
  for (; count != 0; count--, rec += 44) {
    const auto primitive =
        c->emission.element(tomba2::render::modelElement(tomba2::render::ModelList::Gt4, (rec - list) / 44));
    gte_write_data(0, c->mem_r32(rec + 20)); // VXY0
    uint32_t vz01 = c->mem_r32(rec + 24);
    gte_write_data(2, c->mem_r32(rec + 28)); // VXY1
    gte_write_data(1, vz01);                 // VZ0
    gte_write_data(4, c->mem_r32(rec + 32)); // VXY2
    uint32_t vz23 = c->mem_r32(rec + 36);
    gte_write_data(3, vz01 >> 16); // VZ1
    gte_write_data(5, vz23);       // VZ2
    gte_op(c, 0x4A280030u);        // RTPT (verts 0..2)

    uint32_t flagreg = gte_read_ctrl(31);
    if ((int32_t)flagreg < 0) {
      continue;
    }
    gte_op(c, 0x4B400006u); // NCLIP (backface / MAC0)
    int32_t mac0 = (int32_t)gte_read_data(24);
    if (mac0 <= 0) {
      continue; // backface cull
    }

    gte_store_xy(c, pool + 8, 12);  // SXY0
    gte_store_xy(c, pool + 20, 13); // SXY1
    gte_store_xy(c, pool + 32, 14); // SXY2

    uint32_t uv0 = c->mem_r32(rec + 8), uv1 = c->mem_r32(rec + 12);
    uint32_t rgb0_code = c->mem_r32(rec + 0), rgb2_src = c->mem_r32(rec + 4);
    uint32_t uv23 = c->mem_r32(rec + 16);
    c->mem_w32(pool + 12, uv0);
    c->mem_w32(pool + 24, uv1);
    c->mem_w32(pool + 4, rgb0_code & COL_MASK);         // rgb0, MASKED (differs from GT3 leaf)
    c->mem_w32(pool + 16, (rgb0_code << 4) & COL_MASK); // rgb1
    c->mem_w32(pool + 28, rgb2_src & COL_MASK);         // rgb2
    c->mem_w32(pool + 40, (rgb2_src << 4) & COL_MASK);  // rgb3
    c->mem_w32(pool + 36, uv23);                        // uv2 (lo half)
    c->mem_w32(pool + 48, uv23 >> 16);                  // uv3 (hi half)

    gte_write_data(0, c->mem_r32(rec + 40)); // VXY3
    gte_write_data(1, vz23 >> 16);           // VZ3
    gte_op(c, 0x4A180001u);                  // RTPS (4th point, single transform)
    uint32_t flagreg2 = gte_read_ctrl(31);
    if ((int32_t)flagreg2 < 0) {
      continue;
    }
    gte_store_xy(c, pool + 44, 14); // SXY3

    uint32_t flagbyte = rgb2_src >> 24;
    if (uvScroll != kNoScroll && (flagbyte & kScrollFlag) != 0) {
      scrollUv(core, pool, uvScroll, kGt4ScrolledUv);
    }
    int32_t z;
    if (flagbyte == 0) {
      gte_op(c, 0x4B68002Eu); // AVSZ4 (straight OTZ average, 4 pts)
      z = (int32_t)gte_read_data(7);
    } else {
      int32_t sz1 = (int32_t)gte_read_data(16), sz2 = (int32_t)gte_read_data(17), sz3 = (int32_t)gte_read_data(18),
              sz4 = (int32_t)gte_read_data(19);
      // 4-point variant: clamp sz1 vs sz2 and sz3 vs sz4 pairwise first, then combine — the
      // guest instruction path's exact widened form of the 3-point blend above (see FUN_801467bc decomp).
      int32_t a = sz1, b = sz2, e = sz3, f = sz4;
      if (flagbyte & 2u) {
        if (a - b >= 0) {
          a = b;
        }
        if (e - f >= 0) {
          e = f;
        }
        int32_t r = a >> 2;
        if (a - e >= 0) {
          r = e >> 2;
        }
        z = r;
      } else {
        if (a - b <= 0) {
          a = b;
        }
        if (e - f <= 0) {
          e = f;
        }
        int32_t r = a >> 2;
        if (a - e <= 0) {
          r = e >> 2;
        }
        z = r;
      }
    }

    int32_t idx = overlay_gt_otz_index(z);
    if (idx < 0) {
      continue;
    }

    tomba2::render::OrderingTable(*c, ot_base).link(pool, 12u, (uint32_t)idx);
    pool += 52;
  }
  tomba2::render::PacketPool(*c).setCursor(pool);
  c->r[2] = rec;
}

namespace {

void a00Gt3(Core *c) {
  OverlayGt3Gt4::gt3(*c, OverlayGt3Gt4::kNoScroll, tomba2::horizontal_cull::forDrawWindow(c));
}

void a00Gt4(Core *c) {
  OverlayGt3Gt4::gt4(*c, OverlayGt3Gt4::kNoScroll);
}

// A08's pair is called by the scenery walker outside any producer: the list is the drawn object.
constexpr uint32_t kA08Gt3 = 0x80140FBCu;
constexpr uint32_t kA08Gt4 = 0x801411D8u;

void a08Gt3(Core *c) {
  const tomba2::render::ModelObjectScope object(c->emission, kA08Gt3, c->r[4]);
  OverlayGt3Gt4::gt3(*c, OverlayGt3Gt4::kA08Scroll, tomba2::horizontal_cull::forDrawWindow(c));
}

void a08Gt4(Core *c) {
  const tomba2::render::ModelObjectScope object(c->emission, kA08Gt4, c->r[4]);
  OverlayGt3Gt4::gt4(*c, OverlayGt3Gt4::kA08Scroll);
}

} // namespace

void OverlayGt3Gt4::registerOverrides(Game *) {
  tomba::native::declareOverlayOverride("A00", 0x80146478u, "&OverlayGt3Gt4::submitBlock", &OverlayGt3Gt4::submitBlock);
  tomba::native::declareOverlayOverride("A00", 0x801465ECu, "OverlayGt3Gt4::gt3", &a00Gt3);
  tomba::native::declareOverlayOverride("A00", 0x801467BCu, "OverlayGt3Gt4::gt4", &a00Gt4);
  // A00's leaves copied unchanged: A07's drawer 0x8012E1A0 and A0L's block dispatcher 0x8010B1B8 call them.
  tomba::native::declareOverlayOverride("A07", 0x801311D0u, "OverlayGt3Gt4::gt3", &a00Gt3);
  tomba::native::declareOverlayOverride("A07", 0x801313A0u, "OverlayGt3Gt4::gt4", &a00Gt4);
  tomba::native::declareOverlayOverride("A0L", 0x80112DECu, "OverlayGt3Gt4::gt3", &a00Gt3);
  tomba::native::declareOverlayOverride("A0L", 0x80112FBCu, "OverlayGt3Gt4::gt4", &a00Gt4);
  tomba::native::declareOverlayOverride("A08", kA08Gt3, "OverlayGt3Gt4::gt3", &a08Gt3);
  tomba::native::declareOverlayOverride("A08", kA08Gt4, "OverlayGt3Gt4::gt4", &a08Gt4);
}
