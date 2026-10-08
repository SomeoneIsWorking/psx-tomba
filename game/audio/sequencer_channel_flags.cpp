// game/audio/sequencer_channel_flags.cpp — the Sequencer leaves selected by a
// per-channel flags-field bit: pitch-table selection, release and stop housekeeping, the volume
// snapshot, key-event scanning and merge, and note (re)trigger. See docs/engine_re.md.

#include "audio/sequencer.h"

#include "audio/libsnd_globals.h"
#include "audio/sequencer_record.h"
#include "core.h"
#include "core/entry/game_ctx.h"
#include "core/overrides/guest_jal.h"
#include "core/overrides/native_override_catalog.h"
#include "execution_services.h"
#include "game.h"
#include "guest_abi.h"
#include "guest_call.h"

namespace tomba::audio {
using namespace tomba::audio::record;

// 0x800910F0 — sign-extends (seq, chan) to 32 bits and tail-dispatches the pitch-table leaf
// 0x80091120. Stack frame mirrored (sp-24, ra at +16).
void Sequencer::channelPitchSelectDispatch() {
  Core *c = core;
  static constexpr GuestFrameSpill kSpills[] = {{31, 16}}; // -24 (abi_extract-verified)
  GuestFrame<24, 1> frame(c, kSpills);
  c->r[4] = (uint32_t)sext16(c->r[4]);
  c->r[5] = (uint32_t)sext16(c->r[5]);
  tomba::guest::dispatchJalToReturn(
      *c, 0x80091120u, 0x8009110Cu); // FIX: gen sets the real return-site const before the jal
}

// 0x80091050 — release housekeeping: zeroes the channel status byte at +20, clears flags bit1
// (0x02), and calls 0x80095B90 with a0 = seq | (chan<<8). Stack frame mirrored (sp-32,
// ra/s16/s17/s18 at their reverse-engineered offsets).
void Sequencer::channelReleaseClear() {
  Core *c = core;
  static constexpr GuestFrameSpill kSpills[] = {{18, 24}, {16, 16}, {31, 28}, {17, 20}}; // -32
  GuestFrame<32, 4> frame(c, kSpills);
  uint32_t seqRaw = c->r[4];
  uint32_t chanRaw = c->r[5];
  int32_t chan = sext16(chanRaw);
  // gen: a0 = seqRaw | (chanRaw<<8), sign-extended 16 — the combine uses the RAW incoming a0/a1
  // registers (not the sign-extended chan local), matching guest 0x80091050 exactly.
  uint32_t combined = (uint32_t)sext16(seqRaw | (chanRaw << 8));
  uint32_t slot = seqPtrSlot(seqRaw);
  ChannelRecord ch{c, c->mem_r32(slot) + chStride(chan)};
  // FIX (re-verify pass, same class as channelNoteInit's r16 bug): gen keeps r18=seqPtrSlot,
  // r16=chanStride, r17=channelBase LIVE across the 0x80095B90 call, and channelKeyEventScan spills
  // r16/r17/r18 into its frame -- mirror the register lifetimes so the spilled bytes match.
  GuestReg<18> r18(c);
  GuestReg<16> r16(c);
  GuestReg<17> r17(c);
  r18 = slot;
  r16 = chStride(chan);
  r17 = ch.base;
  c->r[4] = combined;
  c->r[31] = 0x800910B0u; // FIX: gen sets the real return-site const before the jal
  channelKeyEventScan();  // FIX: 0x80095B90 is now owned -- direct native call, not typed runtime address dispatch
  ch.setBusy(0);
  // gen re-derefs seqArrayPtr fresh here (redundant unless the callee above mutated it) — mirror
  // that re-read for strict register/memory faithfulness rather than reusing the cached pointer.
  ch.base = c->mem_r32(slot) + chStride(chan);
  ch.clearFlagBits(2u);
}

// 0x80091910 — sets the channel status byte at +20 to 1 and clears flags bit3 (0x08). A true
// leaf: no stack frame, no sub-calls.
void Sequencer::channelStopFlagSet() {
  Core *c = core;
  uint32_t seqRaw = c->r[4];
  int32_t chan = sext16(c->r[5]);
  uint32_t slot = seqPtrSlot(seqRaw);
  ChannelRecord{c, c->mem_r32(slot) + chStride(chan)}.setBusy(1u);
  ChannelRecord ch{c, c->mem_r32(slot) + chStride(chan)}; // re-derefed, mirrors gen
  ch.clearFlagBits(8u);
}

// 0x80095A9C — the volume snapshot. True leaf, no stack frame. a0(r4)=combined (seq |
// chan<<8), a1(r5)=&outL, a2(r6)=&outR; it reads channelBase+88/+90 into them.
void Sequencer::channelVolumeSnapshot() {
  Core *c = core;
  uint32_t combined = c->r[4];
  uint32_t outLPtr = c->r[5];
  uint32_t outRPtr = c->r[6];

  // NOTE: this combined-arg addressing is byte-packed (seq in bits0..7, chan in bits8..15) with NO
  // sign-extension on the seq half — unlike seqPtrSlot()'s sext16(seqRaw), which is for the plain
  // s16 seq arg other leaves take. Different packing, kept separate rather than force-fit one helper.
  uint32_t seqLow = combined & 0xFFu;
  uint32_t seqBasePtr = c->mem_r32(libsnd::kSeqPtrArray + (seqLow << 2));

  c->mem_w16(libsnd::kVolumeSnapshotScratch, (uint16_t)combined); // dead write, mirrored for fidelity

  int32_t chan = (int32_t)((int32_t)(combined & 0xFF00u) >> 8);
  ChannelRecord ch{c, seqBasePtr + chStride(chan)};

  c->mem_w16(outLPtr, ch.volL());
  c->mem_w16(outRPtr, ch.volR());
}

// 0x80094B50 — the key-register merge. True leaf, no stack frame, no ABI args: it reads the
// match value channelKeyEventScan just stamped. It builds a 1-bit-set KON lo/hi word pair from
// it, clears the per-voice status byte in the stride-56 voice table, ORs the bit into the KON
// words, and clears the same bit from the active-voice mask.
void Sequencer::channelKeyRegisterMerge() {
  Core *c = core;
  uint32_t value = c->mem_r16(libsnd::kKeyScanMatch);

  uint32_t bitLo = 0, bitHi = 0;
  if (value < 16u) {
    bitLo = 1u << (value & 31u);
  } else {
    bitHi = 1u << ((value - 16u) & 31u);
  }

  uint32_t tableOff = (uint32_t)(value * 7u) << 3; // value*56 (stride-56 idiom, same as key-scan table)
  uint32_t tableBase = 0x80100000u + tableOff;
  c->mem_w8(tableBase + libsnd::kPerVoiceConsumedFlag, 0u);
  c->mem_w16(tableBase + 21708u, 0u);
  c->mem_w16(tableBase + 21704u, 0u);

  uint32_t konLo = c->mem_r16(libsnd::kKonArmedMaskLo) | bitLo;
  c->mem_w16(libsnd::kKonArmedMaskLo, (uint16_t)konLo);
  uint32_t activeLo = c->mem_r16(libsnd::kActiveVoiceMaskLo) & ~konLo;
  c->mem_w16(libsnd::kActiveVoiceMaskLo, (uint16_t)activeLo);
  // gen publishes v1 (r3) = ~(KON_LO | bitLo) — its final r3 after `r3 = ~(r0|r3)` (r0≡0).
  // The prior draft left r3 stale (MIRROR_VERIFY: native=0x0D substrate=0xFFFFFFFE) because the
  // C rewrite computed konLo as a local but never wrote the ~konLo result the caller reads in v1.
  c->r[3] = ~konLo;

  uint32_t konHi = c->mem_r16(libsnd::kKonArmedMaskHi) | bitHi;
  c->mem_w16(libsnd::kKonArmedMaskHi, (uint16_t)konHi);
  uint32_t activeHi = c->mem_r16(libsnd::kActiveVoiceMaskHi) & ~konHi;
  c->mem_w16(libsnd::kActiveVoiceMaskHi, (uint16_t)activeHi);
}

// 0x80095B90 — the key-event scan. Stack frame mirrored (sp-32, ra/s16/s17/s18). a0(r4) is the
// combined sequence word whose low 16 bits are the target pitch. It skips any voice whose bit is
// set in the hardware voice-active mask and compares the rest against the per-voice pitch table
// (stride 56); on a match it stamps the value and calls channelKeyRegisterMerge().
void Sequencer::channelKeyEventScan() {
  Core *c = core;
  int8_t count = (int8_t)c->mem_r8(libsnd::kSpuKeyScanCount);
  // FIX: gen spills s0(r16) here -- prior draft omitted it entirely.
  static constexpr GuestFrameSpill kSpills[] = {{16, 16}, {31, 28}, {18, 24}, {17, 20}}; // -32
  GuestFrame<32, 4> frame(c, kSpills);
  GuestReg<16> r16(c);
  r16 = 0u;
  if (count > 0) {
    int32_t target = sext16(c->r[4]);
    for (int32_t i = 0; i < (int32_t)(int8_t)c->mem_r8(libsnd::kSpuKeyScanCount); i++) {
      uint32_t voiceBit = 1u << (uint32_t)(i & 31);
      if ((c->mem_r32(libsnd::kHardwareVoiceActive) & voiceBit) != 0u) {
        continue; // voice busy, skip
      }
      uint32_t tableOff = (uint32_t)(i * 7) << 3; // i*56
      int32_t tableVal = (int32_t)(int16_t)c->mem_r16(libsnd::kKeyScanPitchTable + tableOff);
      if (tableVal != target) {
        continue;
      }
      // FIX (re-verify pass, root-caused via SBS bisect to f154 0x801FE928): gen's delay-slot value
      // still live here is `i` (the voice index, set by the branch's delay slot `r2 = r16&255`
      // right before the not-taken fallthrough), NOT tableVal -- the scratch stamp is the matched
      // VOICE INDEX, not the pitch. The prior draft stamped tableVal, corrupting
      // channelKeyRegisterMerge()'s downstream KON-bit/table-offset math (it reads this same value
      // back as `value*56`, the SAME stride channelKeyEventScan just used for `i*56` -- only
      // consistent if the stamped value is the voice index).
      c->mem_w16(libsnd::kKeyScanMatch, (uint16_t)(uint32_t)i);
      c->r[31] = 0x80095C0Cu; // FIX: gen sets the real return-site const before the jal
      channelKeyRegisterMerge();
    }
  }
}

// 0x80091970 — per-channel note retrigger. Stack frame mirrored (sp-24, ra/s0). ABI: a0(r4)=seq,
// a1(r5)=chan. It clears flags bits {0,1,3,10}, sets bit2, calls channelKeyEventScan() and then
// the global-clear leaf 0x800931A0, zeroes ~14 status bytes, reinits several numeric fields, and
// fills a 16-entry breakpoint-table pair (+39../+55..) plus a 16 x u16 array (+96..+126) with 127.
void Sequencer::channelNoteInit() {
  Core *c = core;
  static constexpr GuestFrameSpill kSpills[] = {{31, 20}, {16, 16}}; // -24 (abi_extract-verified)
  GuestFrame<24, 2> frame(c, kSpills);
  int32_t chan = sext16(c->r[5]);
  ChannelRecord ch{c, c->mem_r32(seqPtrSlot(c->r[4])) + chStride(chan)};
  // FIX (re-verify pass, root-caused via SBS bisect to f154 0x801FE928): gen holds channelBase in
  // LIVE r16 (callee-save) across both calls below, and channelKeyEventScan spills r16 into its own
  // frame -- the guest-stack bytes only match if c->r[16] actually carries channelBase at call time.
  // Keeping it solely in a C++ local made the callee spill a stale r16 (A=0x1F800000 vs B=0x800BE698).
  GuestReg<16> r16(c);
  r16 = ch.base;

  ch.clearFlagBits(1u);    // bit0
  ch.clearFlagBits(2u);    // bit1
  ch.clearFlagBits(8u);    // bit3
  ch.clearFlagBits(1025u); // bits{0,10}

  uint32_t combined = (uint32_t)sext16(c->r[4] | (c->r[5] << 8));
  ch.setFlags(ch.flags() | 4u); // set bit2

  c->r[4] = combined;
  c->r[31] = 0x80091A38u; // FIX: gen sets the real return-site const before the jal
  channelKeyEventScan();
  c->r[31] = 0x80091A40u; // FIX: gen sets the real return-site const before the jal
  psx::cpu::dispatchGuestToReturn0(*c,
                                   0x800931A0u,
                                   psx::cpu::ExecutionBudget::currentTurn(*c),
                                   __func__); // input_dispatch_931c0's neighbor, not this wave's target (MAPPED)

  uint32_t f132 = c->mem_r32(ch.base + 132u);
  uint32_t f140 = c->mem_r32(ch.base + 140u);
  uint32_t f86 = c->mem_r16(ch.base + 86u);
  uint32_t f4 = c->mem_r32(ch.base + 4u);

  // ~14 per-channel status bytes cleared, in the guest-visible behavior's own (redundant, re-clears +28) order —
  // not renamed to a loop or a named struct field: none of these bytes has a confirmed semantic
  // role beyond "cleared on note retrigger" (see header comment), so a name here would be invented,
  // not RE'd. Kept as the flat sequence gen emits, byte-for-byte and store-for-store.
  ch.setBusy(0);
  c->mem_w32(ch.base + 136u, 0u);
  c->mem_w8(ch.base + 28u, 0u);
  c->mem_w8(ch.base + 24u, 0u);
  c->mem_w8(ch.base + 25u, 0u);
  c->mem_w8(ch.base + 30u, 0u);
  c->mem_w8(ch.base + 26u, 0u);
  c->mem_w8(ch.base + 27u, 0u);
  c->mem_w8(ch.base + 31u, 0u);
  c->mem_w8(ch.base + 23u, 0u);
  c->mem_w8(ch.base + 33u, 0u);
  c->mem_w8(ch.base + 28u, 0u);
  c->mem_w8(ch.base + 29u, 0u);
  c->mem_w8(ch.base + 21u, 0u);
  c->mem_w8(ch.base + 22u, 0u);
  c->mem_w32(ch.base + 144u, f132);
  c->mem_w32(ch.base + 148u, f140);
  c->mem_w16(ch.base + 84u, (uint16_t)f86);
  c->mem_w32(ch.base + 0u, f4);
  c->mem_w32(ch.base + 8u, f4);

  for (int32_t i = 0; i < 16; i++) {
    c->mem_w8(ch.base + 55u + (uint32_t)i, (uint8_t)i);
    c->mem_w8(ch.base + 39u + (uint32_t)i, 64u);
    c->mem_w16(ch.base + 96u + (uint32_t)i * 2u, 127u);
  }
  c->mem_w16(ch.snapshotTargetLPtr(), 127u);
  c->mem_w16(ch.snapshotTargetRPtr(), 127u);
}

// The dispatcher owns the C entry point; this file owns what it runs.
[[maybe_unused]] static void nat_channelPitchSelectDispatch(Core *c) {
  eng(c).sequencer.channelPitchSelectDispatch();
}

// The dispatcher owns the C entry point; this file owns what it runs.
[[maybe_unused]] static void nat_channelNoteInit(Core *c) {
  eng(c).sequencer.channelNoteInit();
}

// The dispatcher owns the C entry point; this file owns what it runs.
[[maybe_unused]] static void nat_channelKeyEventScan(Core *c) {
  eng(c).sequencer.channelKeyEventScan();
}

// The dispatcher owns the C entry point; this file owns what it runs.
[[maybe_unused]] static void nat_channelKeyRegisterMerge(Core *c) {
  eng(c).sequencer.channelKeyRegisterMerge();
}

// The last three leaves below have no observed reachable call site: their flags bits never came up
// in any recorded run, so the dispatcher routes those bits to the guest body. See docs/engine_re.md.

// The dispatcher owns the C entry point; this file owns what it runs.
[[maybe_unused]] static void nat_channelReleaseClear(Core *c) {
  eng(c).sequencer.channelReleaseClear();
}

// The dispatcher owns the C entry point; this file owns what it runs.
[[maybe_unused]] static void nat_channelStopFlagSet(Core *c) {
  eng(c).sequencer.channelStopFlagSet();
}

// The dispatcher owns the C entry point; this file owns what it runs.
[[maybe_unused]] static void nat_channelVolumeSnapshot(Core *c) {
  eng(c).sequencer.channelVolumeSnapshot();
}

void declareChannelFlagOverrides() {
  tomba::native::declareOverride(0x800910F0u, "nat_channelPitchSelectDispatch", nat_channelPitchSelectDispatch);
  tomba::native::declareOverride(0x80091970u, "nat_channelNoteInit", nat_channelNoteInit);
  tomba::native::declareOverride(0x80095B90u, "nat_channelKeyEventScan", nat_channelKeyEventScan);
  tomba::native::declareOverride(0x80094B50u, "nat_channelKeyRegisterMerge", nat_channelKeyRegisterMerge);
}

} // namespace tomba::audio
