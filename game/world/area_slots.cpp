// game/world/area_slots.cpp — AreaSlots method bodies. See area_slots.h for the per-method RE
// contracts; the 8 leaf callees stay substrate.

#include "world/area_slots.h"
#include "cfg.h"
#include "core.h"
#include "core/entry/game_ctx.h"
#include "core/overrides/native_override_catalog.h" // tomba::native::declareOverride — the one native-override registry
#include "game.h"
#include "guest_call.h"
#include <cstdio>

// FUN_800998E4's own table and encoding — named here rather than repeated as literals. See
// area_slots.h::classifySlotStates for why the stride differs from updateTail's table.
namespace {
constexpr uint32_t kSlotCount = 24;
constexpr uint32_t kEntryTablePtr = 0x800AC604u; // holds the base of the 16-byte-stride entry table
constexpr uint32_t kArmedMaskWord = 0x800AC590u; // bit i = slot i is armed
constexpr uint32_t kEntryStride = 16;
constexpr uint32_t kEntryFieldOff = 12; // u16 whose non-zero-ness is the second fact
constexpr uint8_t kArmedAndField = 1;
constexpr uint8_t kFieldOnly = 2;
constexpr uint8_t kArmedOnly = 3;
constexpr uint8_t kNeither = 0;
} // namespace
// The MAIN-module setter: 0x800998E4 lives in shard_0, so direct guest 0x800998E4(c) callers
// (AreaSlots::updateTail's own call site among them) must be intercepted through it too.

// AreaSlots::updateTail — the last direct child of ov_field_frame at guest 0x80075A80.
// Slot-table state machine over the 24-entry × 12-byte area at 0x800BE238; see area_slots.h for
// the per-arm contract. All 8 callees stay substrate (FUN_800998E4 buf-fill, FUN_80092660 action
// leaf, FUN_80098F90 mask-drain, FUN_80075824 + FUN_80099490 common tail, FUN_8008E0C0 key2 probe,
// FUN_80074BF8 / FUN_80074E48 sub-obj tails). Guest allocates 88 bytes of stack — the buffer address
// is passed to FUN_800998E4 and the action leaf's 4 stacked args, so we mirror the sp adjust.
//
// GUEST-FRAME FIDELITY (tools/binary ABI evidence 0x80075A80 --contract): the prologue spills the
// LIVE incoming r16..r21/r31 to sp+56..80, and every one of the 8 leaf calls below runs with a
// specific r16..r21/r31 live in the guest register file — FUN_80092660 in particular re-spills
// those onto ITS OWN guest stack frame, so if we don't reproduce the exact live values here the
// bytes it spills diverge from retired comparison path (SBS byte diff at 0x801FE900). r16..r21/r31 are set
// at every point gen reassigns them and otherwise left untouched (a compliant callee transparently
// preserves them across its own call, so a value set once here rides unchanged through subsequent
// calls exactly like it does in gen).
void AreaSlots::updateTail() {
  Core *c = core;
  uint32_t sp_save = c->r[29];
  c->r[29] = sp_save - 88u; // addiu sp, -88
  const uint32_t sp = c->r[29];
  const uint32_t S5 = 0x800BE1F8u;
  const uint32_t buf_addr = sp + 0x20u;

  // Prologue register spills (7), gen program order: sp+76<-r21(incoming); r21:=S5; sp+80<-r31
  // (incoming ra); sp+72<-r20; sp+68<-r19; sp+64<-r18; sp+60<-r17; r31:=0x80075AB0 (jal-site for
  // FUN_800998E4); sp+56<-r16(incoming). r16..r20 stay at their INCOMING values for that first
  // call — gen never reassigns them before it.
  c->mem_w32(sp + 76u, c->r[21]);
  c->r[21] = S5;
  c->mem_w32(sp + 80u, c->r[31]);
  c->mem_w32(sp + 72u, c->r[20]);
  c->mem_w32(sp + 68u, c->r[19]);
  c->mem_w32(sp + 64u, c->r[18]);
  c->mem_w32(sp + 60u, c->r[17]);
  c->r[31] = 0x80075AB0u;
  c->mem_w32(sp + 56u, c->r[16]);

  // (1) Fill the 24-byte per-slot state buffer for this frame.
  c->r[4] = buf_addr;
  psx::cpu::dispatchGuestToReturn0(*c, 0x800998E4u, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);

  // (2) Slot loop over 24 entries at 0x800BE238 (12 bytes each), starting at the counter at 0x800BED78.
  int32_t s2 = (int32_t)c->mem_r32(0x800BED78u);
  uint32_t s1 = 0x800BE238u + (uint32_t)s2 * 12u;
  const bool loopEntered = (s2 < 24);
  cfg_logf("asent",
           "updateTail ENTER counter(0x800BED78)=%d loopEntered=%d area=%u slot23kind=%02X sp=%08X r22=%08X r23=%08X "
           "r30=%08X",
           s2,
           (int)loopEntered,
           c->mem_r8(0x800BF870u),
           c->mem_r8(0x800BE34Cu),
           c->r[29],
           c->r[22],
           c->r[23],
           c->r[30]);
  if (loopEntered) {
    c->r[20] = 0x800C0000u; // gen: r20 set once before the loop, ONLY on the
                            // taken path — left incoming/untouched if skipped.
  }
  for (; s2 < 24; s2++, s1 += 12u) {
    const uint32_t s0 = s1 + 1u;
    uint8_t kind = c->mem_r8(s1);
    if (kind == 0xFF) {
      // Action arm — hword and a3 pick by top bit of slot[3]. slot[2..7] fill a2/a3 + 4 stack args.
      uint8_t s3b = c->mem_r8(s0 + 2u);
      int16_t hword;
      uint32_t a3;
      if (s3b & 0x80u) {
        hword = c->mem_r16s(0x800A4F7Eu);
        a3 = (uint32_t)(s3b & 0x0Fu);
      } else {
        hword = c->mem_r16s(0x800BED84u);
        a3 = (uint32_t)s3b;
      }
      c->mem_w32(sp + 0x10u, (uint32_t)c->mem_r8(s0 + 3u));
      c->mem_w32(sp + 0x14u, (uint32_t)c->mem_r8(s0 + 4u));
      c->mem_w32(sp + 0x18u, (uint32_t)c->mem_r8(s0 + 5u));
      c->mem_w32(sp + 0x1Cu, (uint32_t)c->mem_r8(s0 + 6u));
      c->r[4] = (uint32_t)(int32_t)(int16_t)s2;
      c->r[5] = (uint32_t)(int32_t)hword;
      c->r[6] = (uint32_t)c->mem_r8(s0 + 1u); // slot byte +2 — gen: `r6 = mem_r8(r16+1)` with
                                              // r16 = slot+1. The original `mem_r8(s0)` read
                                              // slot byte +1 (one short) and fed the wrong
                                              // INSTRUMENT to FUN_80092660 — every sequencer-
                                              // routed SFX in the prologue played program 0x0F
                                              // instead of 0x01 (wrong sample, wrong pitch).
      c->r[7] = a3;
      // Live regs at THIS call (abi_extract): r16=s0, r17=s1, r18=s2, r19=s2<<16, r20=0x800C0000,
      // r21=S5 — FUN_80092660 spills these onto its own guest stack frame; must be exact.
      c->r[16] = s0;
      c->r[17] = s1;
      c->r[18] = (uint32_t)s2;
      c->r[19] = (uint32_t)s2 << 16;
      c->r[20] = 0x800C0000u;
      c->r[21] = S5;
      c->r[31] = 0x80075B84u;
      cfg_logf("as37",
               "updateTail action-arm spawn 0x80092660 slot=%d r16=%08X r19=%08X area=%u",
               s2,
               c->r[16],
               c->r[19],
               c->mem_r8(0x800BF870u));
      psx::cpu::dispatchGuestToReturn0(*c, 0x80092660u, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
      uint32_t mask = c->mem_r32(0x800BE358u); // clear bit s2 in the arm-mask
      mask &= ~(1u << (uint32_t)s2);
      c->mem_w32(0x800BE358u, mask);
      // gen re-reads slot[0] AFTER FUN_80092660 returns (it may mutate the slot) rather than using
      // the pre-call cached `kind` — subtract 1 from the FRESH value, not the stale local.
      uint8_t freshKind = c->mem_r8(s1);
      c->mem_w8(s1, (uint8_t)(freshKind - 1u)); // kind -= 1 (0xFF -> 0xFE)
      continue;                                 // skip buf post-check (guest goto 0x80075C14)
    }
    if (kind != 0) {
      // Other-kind arm — always decrement kind; if slot[8]==4 and it hit 0, set the arm-mask bit and
      // zero slot[2]+slot[3] (guest 0x80075BE4/0x80075BEC — both writes fire via the jr delay slot).
      uint8_t s8b = c->mem_r8(s0 + 7u);
      uint8_t newkind = (uint8_t)(kind - 1u);
      c->mem_w8(s1, newkind);
      if (s8b == 4 && newkind == 0) {
        uint32_t mask = c->mem_r32(0x800BE358u);
        mask |= (1u << (uint32_t)s2);
        c->mem_w32(0x800BE358u, mask);
        c->mem_w8(s1 + 3u, 0); // slot[3]
        c->mem_w8(s1 + 2u, 0); // slot[2]
      }
    }
    // Buf post-check: buf[s2] in {0,3} -> zero slot[1]. Reached for kind==0 and the other-kind arm.
    uint8_t bv = c->mem_r8(buf_addr + (uint32_t)s2);
    if (bv == 0 || bv == 3) {
      c->mem_w8(s1 + 1u, 0);
    }
  }

  // Post-loop live-register mirror (gen L_80075C30): r16:=0x800C0000; r17/r18 hold the loop-final
  // s1/s2 (or, if the loop never ran, still the incoming s1/s2 — same locals either way); r19 only
  // touched if the loop ran (else left at its incoming value, matching gen leaving it unset).
  c->r[16] = 0x800C0000u;
  c->r[17] = s1;
  c->r[18] = (uint32_t)s2;
  if (loopEntered) {
    c->r[19] = (uint32_t)s2 << 16;
  }
  c->r[21] = S5;

  // (3) Mask-drain: if any bit set, call FUN_80098F90(0) then clear the mask.
  if (c->mem_r32(0x800BE358u) != 0) {
    c->r[4] = 0;
    c->r[31] = 0x80075C4Cu;
    psx::cpu::dispatchGuestToReturn0(*c, 0x80098F90u, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
    c->mem_w32(0x800BE358u, 0);
  }

  // (4) Common tail leaves — both take a0 = 0x800BE1F8.
  // Was: eng(c).musicCoord.musicFadeIn() — WRONG. FUN_80075824 was misnamed as musicFadeIn;
  // the RE (2026-07-03, ghidra) shows it is the per-voice VOLUME MIXER tick, not a fade snap.
  // SBS gameplay mode surfaced the divergence at 0x800BE208/A the moment we replaced the guest instruction path
  // dispatch with musicFadeIn; the proper port is voiceMixTick(0x800BE1F8).
  c->r[4] = S5;
  c->r[31] = 0x80075C58u;             // jal-site const, kept even for the native call
  eng(c).musicCoord.voiceMixTick(S5); // FUN_80075824 (native)
  c->r[4] = S5;
  c->r[31] = 0x80075C60u;
  psx::cpu::dispatchGuestToReturn0(*c, 0x80099490u, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);

  // (5) Key2 branch. gen reassigns r17:=0x800C0000 right here, before the key2 read, and the S5
  // zero-write below is UNCONDITIONAL — it lives in the branch's delay slot, so it fires whether
  // or not key2 == -1 (was gated on `key2 != -1`, dropping the write on the skip path).
  c->r[17] = 0x800C0000u;
  int16_t key2 = c->mem_r16s(0x800BED80u);
  c->mem_w32(S5, 0);
  if (key2 != -1) {
    uint32_t entry_addr = 0x800BE368u + (uint32_t)((int32_t)key2 * 8);
    int16_t entry_hw = c->mem_r16s(entry_addr);
    c->r[4] = (uint32_t)(int32_t)entry_hw;
    c->r[5] = 0;
    c->r[31] = 0x80075C90u;
    psx::cpu::dispatchGuestToReturn0(*c, 0x8008E0C0u, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
    if ((c->r[2] & 0xFFFFu) == 0) {
      // Guest checked (return << 16) != 0 == (return & 0xFFFF) != 0.
      // gen reassigns r16:=S5 here (r16 = 0x800C0000 - 7688), live for both the subid==0 and
      // subid!=0 leaves below.
      c->r[16] = S5;
      uint8_t subid = c->mem_r8(0x800BE22Au);
      // gen source order (shard_7.c:11082-11087): the subid!=0 (BF8) leaf is the fall-through and
      // comes FIRST, the subid==0 (E48) leaf is the branch target — mirror that order so the static
      // call/store sequence matches the oracle (logically identical; the two leaves are exclusive).
      if (subid != 0) {
        // gen zeroes key2 (0x800BED80) in FUN_80074BF8's call delay slot — BEFORE the callee runs.
        c->mem_w16(0x800BED80u, 0);
        c->r[4] = (uint32_t)subid;
        c->r[31] = 0x80075CB8u;
        psx::cpu::dispatchGuestToReturn0(*c, 0x80074BF8u, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
        c->mem_w8(0x800BE22Au, 0); // gen mem_w8(r16+50,0) after BF8 (r16=S5, +50=0x800BE22A)
      } else {
        c->r[4] = 0;
        c->r[31] = 0x80075CC8u;
        psx::cpu::dispatchGuestToReturn0(*c, 0x80074E48u, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
      }
    }
  }

  // Epilogue: restore r16..r21,r31 from the FRAME (gen L_80075CC8) — not from C++ locals, so the
  // guest-stack read that produced them stays the byte-exact source of truth.
  c->r[31] = c->mem_r32(sp + 80u);
  c->r[21] = c->mem_r32(sp + 76u);
  c->r[20] = c->mem_r32(sp + 72u);
  c->r[19] = c->mem_r32(sp + 68u);
  c->r[18] = c->mem_r32(sp + 64u);
  c->r[17] = c->mem_r32(sp + 60u);
  c->r[16] = c->mem_r32(sp + 56u);
  c->r[29] = sp_save;
}

// AreaSlots::ackIfMatch — FUN_80074AF0 body. Pure 21-instruction primitive over the same
// slot table (0x800BE238, 12-byte stride) + armed-mask (0x800BE358) that updateTail iterates,
// but scoped to a SINGLE ack event: the caller passes an arg encoding {entryIdx: arg & 0xFF,
// signature: arg & 0xFFFFFF00}; if the signature matches the u32 stored at slot[idx].w0's high
// 3 bytes, set the armed bit AND clear the slot's trigger-pending byte at +1. Signature mismatch
// is a silent no-op (the guest instruction path's `bne v0, a0, 0x80074B3C` short-circuits directly to jr ra).
// RE'd verbatim from disas 0x80074AF0..0x80074B40.
void AreaSlots::ackIfMatch(uint32_t arg) {
  Core *c = core;
  uint32_t idx = arg & 0xFFu;
  uint32_t entry = 0x800BE238u + idx * 12u;
  uint32_t stored_hi3 = c->mem_r32(entry) & 0xFFFFFF00u;
  uint32_t arg_hi3 = arg & 0xFFFFFF00u;
  if (stored_hi3 != arg_hi3) {
    return;
  }
  uint32_t mask = c->mem_r32(0x800BE358u);
  c->mem_w32(0x800BE358u, mask | (1u << idx)); // set armed bit `idx`
  c->mem_w8(entry + 1, 0);                     // clear trigger-pending byte
}

// AreaSlots::primeCountdown — FUN_80074A38 body. Pure 1-store leaf: table[idx].kind = 10.
void AreaSlots::primeCountdown(uint32_t idx) {
  Core *c = core;
  uint32_t entry = 0x800BE238u + (idx & 0xFFu) * 12u;
  c->mem_w8(entry, 10);
}

// AreaSlots::updateCell — FUN_8007496C body. sigArg carries {idx: low byte, signature: high 3
// bytes} exactly like ackIfMatch's arg; on a signature mismatch this is a silent no-op (false).
// On match: dx/dy are adjusted by a fixed global cell-scroll offset (the byte at 0x800FB165,
// scaled), clamped to [0,127], written into the slot's cell-coord bytes (entry+6=dx, entry+7=dy),
// and the notifier FUN_80092E3C(idx, dx<<7, dy<<7) fires (kept substrate — outside this band).
// RE'd verbatim from Ghidra (scratch/decomp/cluster1.c: FUN_8007496c).
bool AreaSlots::updateCell(uint32_t sigArg, int32_t dx, int32_t dy) {
  Core *c = core;
  uint32_t idx = sigArg & 0xFFu;
  uint32_t entry = 0x800BE238u + idx * 12u;
  if ((c->mem_r32(entry) & 0xFFFFFF00u) != (sigArg & 0xFFFFFF00u)) {
    return false;
  }

  int32_t bias = ((int32_t)c->mem_r8(0x800FB165u) - 9) * 16; // (9 - DAT_800FB165) * -16
  dx += bias;
  dy += bias;
  if (dx < 0) {
    dx = 0;
  } else if (dx > 0x7F) {
    dx = 0x7F;
  }
  if (dy < 0) {
    dy = 0;
  } else if (dy > 0x7F) {
    dy = 0x7F;
  }

  c->mem_w8(entry + 6, (uint8_t)dx);
  c->mem_w8(entry + 7, (uint8_t)dy);
  c->r[4] = idx;
  c->r[5] = (uint32_t)(dx << 7);
  c->r[6] = (uint32_t)(dy << 7);
  psx::cpu::dispatchGuestToReturn0(*c, 0x80092E3Cu, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
  return true;
}

static void eov_areaSlotsPrime(Core *c) {
  eng(c).areaSlots.primeCountdown(c->r[4]);
  // Mirror guest 0x80074A38's register outputs (shard_0.c): r2=10 (the kind byte), r3=entry addr.
  // The native C++ body only writes the guest byte; the substrate leaves these in v0/v1 at return.
  c->r[2] = 10;
  c->r[3] = 0x800BE238u + (c->r[4] & 0xFFu) * 12u;
}
static void eov_areaSlotsUpdateCell(Core *c) {
  // guest 0x8007496C allocates a 24-byte frame + spills r31@sp+16 (shard_0.c). The native body
  // dispatches a still-substrate leaf (FUN_80092E3C) which spills caller r31 — needs the frame.
  c->r[29] -= 24;
  c->mem_w32(c->r[29] + 16, c->r[31]);
  c->r[2] = eng(c).areaSlots.updateCell(c->r[4], (int32_t)c->r[5], (int32_t)c->r[6]) ? 1 : 0;
  c->r[31] = c->mem_r32(c->r[29] + 16);
  c->r[29] += 24;
}

// ORACLE: guest 0x800998E4
// FUN_800998E4 — see area_slots.h. Fills the 24-byte slot-status buffer updateTail consults, one byte
// per slot, from two independent facts per slot.
//
// The four output values are NOT a bitfield despite looking like one: armed+field is 1, armed-only is
// 3, field-only is 2, neither is 0. Reading it as "bit0 = armed, bit1 = field" would swap two of the
// four cases, and this header's own updateTail notes already ascribe distinct meanings to 0 and 3.
//
// A pure leaf: frame_size 0, no calls, so nothing to mirror on the guest stack. The guest body's
// trailing `guest 0x80099970(c); return;` after its real `return;` is the recorded binary evidence's dead-tail artifact
// (a folded sibling), not part of this function.
void AreaSlots::classifySlotStates(uint32_t outBuf) {
  Core *c = core;
  for (uint32_t slot = 0; slot < kSlotCount; slot++) {
    const uint32_t entry = c->mem_r32(kEntryTablePtr) + slot * kEntryStride;
    const uint32_t armed = c->mem_r32(kArmedMaskWord) & (1u << slot);
    const uint32_t field = c->mem_r16(entry + kEntryFieldOff);
    // FOUR separate store sites, not two ternaries: the guest has one store per arm and the
    // equivalence gate compares STATIC store sites, so collapsing them is byte-identical in effect
    // but fails by construction. Same trap as the rope port's clamp earlier in this session.
    if (armed != 0) {
      if (field != 0) {
        c->mem_w8(outBuf + slot, kArmedAndField);
      } else {
        c->mem_w8(outBuf + slot, kArmedOnly);
      }
    } else {
      if (field != 0) {
        c->mem_w8(outBuf + slot, kFieldOnly);
      } else {
        c->mem_w8(outBuf + slot, kNeither);
      }
    }
  }

  // REPRODUCE THE EXIT REGISTER STATE. This leaf has no stack frame, so there is nothing to mirror
  // there — but it is a REGISTER-STATE divergence all the same, and it is observable: the guest-visible behavior
  // uses a1..a3 and t0..t2 as loop scratch and leaves them holding the loop's final values, and a
  // caller that spills those caller-saved registers after the call writes them into guest RAM. Left
  // untouched, this port diverged at lockstep frame 14 in the guest stack at 0x801FE998 — a stack
  // diff caused by registers, which is exactly the trap that reads as "impossible, I write no stack".
  const uint32_t base = c->mem_r32(kEntryTablePtr);
  c->r[10] = kSlotCount;                                           // t2 — the loop limit
  c->r[9] = kArmedOnly;                                            // t1 — 3
  c->r[8] = kFieldOnly;                                            // t0 — 2
  c->r[7] = kArmedAndField;                                        // a3 — 1
  c->r[6] = kSlotCount;                                            // a2 — the index, at its exit value
  c->r[5] = outBuf + kSlotCount;                                   // a1 — the output cursor, one past the end
  c->r[4] = base + (kSlotCount - 1) * kEntryStride;                // a0 — the last entry address
  c->r[3] = c->mem_r32(kArmedMaskWord) & (1u << (kSlotCount - 1)); // the last slot's armed bit
  c->r[2] = 0;                                                     // the loop test that ended it
}

static void eov_areaSlotsClassify(Core *c) {
  eng(c).areaSlots.classifySlotStates(c->r[4]);
}

void AreaSlots::registerOverrides() {
  tomba::native::declareOverride(0x80074A38u, "AreaSlots::primeCountdown", eov_areaSlotsPrime);
  tomba::native::declareOverride(0x8007496Cu, "AreaSlots::updateCell", eov_areaSlotsUpdateCell);
  tomba::native::declareOverride(0x800998E4u, "AreaSlots::classifySlotStates", eov_areaSlotsClassify);
}
