// game/audio/sequencer_pitch_envelope.cpp — the Sequencer leaves that ramp a
// channel's pitch and envelope toward a target over successive VBlank ticks. See docs/engine_re.md.

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

// 0x80090E40 — the portamento ramp. It increments the +160 counter; once it exceeds the +156
// limit it clears flags bit4 and snapshots the channel volume into +92/+94. While still ramping it
// computes delta = (rate(+72) * counter) / limit(+156) - base(+74) through cpu_div (including its
// division traps), applies it to the +88/+90 pair, clamps both to [0,127] and writes them out
// through 0x80095530.
//
// Register-literal on purpose: the branches re-converge on shared tails from several call sites,
// which a hand restructure would silently reorder.
void Sequencer::channelPitchSlideTick() {
  Core *c = core;
  // GuestFrame declared BEFORE any of r16..r21/ra are touched below (same point the gen prologue's
  // spills logically capture — the interleaved r2/r3/r7/r8 scratch math above them in the original
  // transcription never read/wrote a callee-saved register, so hoisting the frame here captures the
  // identical pre-call values gen's own spill sequence did). Internal control flow/register usage
  // kept LITERAL past this point — see this function's header comment for why.
  static constexpr GuestFrameSpill kSpills[] = {
      {21, 44}, {31, 48}, {20, 40}, {19, 36}, {18, 32}, {17, 28}, {16, 24}}; // -56
  GuestFrame<56, 7> frame(c, kSpills);
  c->r[2] = c->r[4] << 16;
  c->r[3] = libsnd::kSeqPtrArray;
  c->r[2] = (uint32_t)((int32_t)c->r[2] >> 14);
  c->r[8] = c->r[2] + c->r[3];
  c->r[3] = c->r[5] << 16;
  c->r[3] = (uint32_t)((int32_t)c->r[3] >> 16);
  c->r[2] = c->r[3] << 1;
  c->r[2] = c->r[2] + c->r[3];
  c->r[2] = c->r[2] << 2;
  c->r[2] = c->r[2] - c->r[3];
  c->r[7] = c->r[2] << 4;
  c->r[21] = c->r[4];
  c->r[3] = c->mem_r32(c->r[8] + 0u);
  c->r[20] = c->r[5];
  c->r[18] = c->r[3] + c->r[7];
  c->r[2] = c->mem_r32(c->r[18] + 160u);
  c->r[3] = c->mem_r32(c->r[18] + 156u);
  c->r[6] = c->r[2] + 1u;
  {
    int _t = ((int32_t)c->r[3] < (int32_t)c->r[6]);
    c->mem_w32(c->r[18] + 160u, c->r[6]);
    if (!_t) {
      goto L_80090EC4;
    }
  }
  c->r[2] = c->mem_r32(c->r[8] + 0u);
  c->r[2] = c->r[7] + c->r[2];
  goto L_80090FF8;

L_80090EC4:
  c->r[2] = (uint32_t)c->mem_r16s(c->r[18] + 72u);
  {
    int64_t _p = (int64_t)(int32_t)c->r[2] * (int64_t)(int32_t)c->r[6];
    c->lo = (uint32_t)_p;
    c->hi = (uint32_t)((uint64_t)_p >> 32);
  }
  c->r[2] = c->lo;
  cpu_div(c, c->r[2], c->r[3]);
  if (c->r[3] == 0u) {
    psx::cpu::handleBreak(*c, 7168u);
  }
  if (c->r[3] == 0xFFFFFFFFu && c->r[2] == 0x80000000u) {
    psx::cpu::handleBreak(*c, 6144u);
  }
  c->r[16] = c->lo;
  c->r[2] = (uint32_t)c->mem_r16s(c->r[18] + 74u);
  c->r[3] = (uint32_t)c->mem_r16(c->r[18] + 74u);
  c->r[16] = c->r[16] - c->r[2];
  {
    int _t = (c->r[16] == 0u);
    if (_t) {
      goto L_80091008;
    }
  }
  c->r[2] = c->r[4] | (c->r[5] << 8);
  c->r[2] = c->r[2] << 16;
  c->r[19] = (uint32_t)((int32_t)c->r[2] >> 16);
  c->r[4] = c->r[19];
  c->r[5] = c->r[29] + 16u;
  c->r[6] = c->r[29] + 18u;
  c->r[2] = c->r[3] + c->r[16];
  c->mem_w16(c->r[18] + 74u, (uint16_t)c->r[2]);
  c->r[31] = 0x80090F40u; // FIX: gen sets the real return-site const before the jal
  channelVolumeSnapshot();
  c->r[2] = (uint32_t)c->mem_r16(c->r[29] + 16u);
  c->r[17] = c->r[2] + c->r[16];
  {
    int _t = ((int32_t)c->r[17] < 128);
    if (_t) {
      goto L_80090F5C;
    }
  }
  c->r[17] = 127u;
L_80090F5C: {
  int _t = ((int32_t)c->r[17] >= 0);
  if (_t) {
    goto L_80090F68;
  }
}
  c->r[17] = 0u;
L_80090F68:
  c->r[2] = (uint32_t)c->mem_r16(c->r[29] + 18u);
  c->r[16] = c->r[2] + c->r[16];
  {
    int _t = ((int32_t)c->r[16] < 128);
    if (_t) {
      goto L_80090F84;
    }
  }
  c->r[16] = 127u;
L_80090F84: {
  int _t = ((int32_t)c->r[16] >= 0);
  if (_t) {
    goto L_80090F90;
  }
}
  c->r[16] = 0u;
L_80090F90:
  c->r[5] = c->r[17] & 65535u;
  c->r[6] = c->r[16] & 65535u;
  c->r[4] = c->r[19];
  c->r[7] = 1u;
  c->r[31] = 0x80090FA0u;      // FIX: gen sets the real return-site const before the jal
  channelVoiceRegisterWrite(); // FIX: 0x80095530 is now owned -- direct native call
  {
    int _t = (c->r[17] != 127u);
    if (_t) {
      goto L_80090FB4;
    }
  }
  {
    int _t = (c->r[16] == c->r[17]);
    c->r[4] = c->r[21] << 16;
    if (_t) {
      goto L_80090FC8;
    }
  }
L_80090FB4: {
  int _t = (c->r[17] != 0u);
  c->r[4] = c->r[20] << 8;
  if (_t) {
    goto L_8009100C;
  }
}
  {
    int _t = (c->r[16] != 0u);
    c->r[4] = c->r[21] | c->r[4];
    if (_t) {
      goto L_80091010;
    }
  }
  c->r[4] = c->r[21] << 16;
L_80090FC8:
  c->r[4] = (uint32_t)((int32_t)c->r[4] >> 14);
  c->r[3] = c->r[20] << 16;
  c->r[3] = (uint32_t)((int32_t)c->r[3] >> 16);
  c->r[2] = c->r[3] << 1;
  c->r[2] = c->r[2] + c->r[3];
  c->r[2] = c->r[2] << 2;
  c->r[2] = c->r[2] - c->r[3];
  c->r[3] = 0x80100000u + c->r[4];
  c->r[3] = c->mem_r32(c->r[3] + 19504u);
  c->r[2] = c->r[2] << 4;
  c->r[2] = c->r[2] + c->r[3];
L_80090FF8:
  c->r[3] = c->mem_r32(c->r[2] + 152u);
  c->r[3] = c->r[3] & ~0x10u;
  c->mem_w32(c->r[2] + 152u, c->r[3]);
L_80091008:
  c->r[4] = c->r[20] << 8;
L_8009100C:
  c->r[4] = c->r[21] | c->r[4];
L_80091010:
  c->r[4] = (uint32_t)((int32_t)(c->r[4] << 16) >> 16);
  c->r[5] = c->r[18] + 92u;
  c->r[31] = 0x80091024u; // FIX: gen sets the real return-site const before the jal
  c->r[6] = c->r[18] + 94u;
  channelVolumeSnapshot();
  // GuestFrame's destructor restores r16..r21/r31 + ascends sp here.
}

// 0x80092080 — the ADSR/envelope ramp. It decrements the +168 counter; if it was already 0 it
// clears bit6 and finishes. Otherwise it divides the decremented counter by rate(+78) and nudges the
// envelope value (+148) by +/-1 toward the target (+172) only on an exact division; a non-zero
// remainder returns immediately. With rate <= 0 it steps by |rate| against +80, clamped so it cannot
// overshoot. The output level (+84) is then cpu_divu(envelope*10, scale*15), clamped to at least 1,
// and bits 6+7 clear when the counter reached 0 or the envelope equals the target.
//
// Register-literal for the same reason as channelPitchSlideTick.
void Sequencer::channelEnvelopeRampTick() {
  Core *c = core;
  c->r[2] = c->r[4] << 16;
  c->r[3] = libsnd::kSeqPtrArray;
  c->r[2] = (uint32_t)((int32_t)c->r[2] >> 14);
  c->r[9] = c->r[2] + c->r[3];
  c->r[3] = c->r[5] << 16;
  c->r[3] = (uint32_t)((int32_t)c->r[3] >> 16);
  c->r[2] = c->r[3] << 1;
  c->r[2] = c->r[2] + c->r[3];
  c->r[2] = c->r[2] << 2;
  c->r[2] = c->r[2] - c->r[3];
  c->r[8] = c->r[2] << 4; // chanStride
  c->r[3] = c->mem_r32(c->r[9] + 0u);
  c->r[6] = c->r[4];
  c->r[7] = c->r[3] + c->r[8]; // channelBase
  c->r[2] = c->mem_r32(c->r[7] + 168u);
  c->r[2] = c->r[2] - 1u;
  {
    int _t = ((int32_t)c->r[2] >= 0);
    c->mem_w32(c->r[7] + 168u, c->r[2]);
    if (_t) {
      goto L_800920F8;
    }
  }
  // counter just went negative -- envelope already finished; clear bit6 only, then finalize.
  c->r[3] = c->mem_r32(c->r[9] + 0u);
  c->r[3] = c->r[8] + c->r[3];
  c->r[2] = c->mem_r32(c->r[3] + 152u);
  c->r[4] = ~0x40u;
  c->r[2] = c->r[2] & c->r[4];
  c->mem_w32(c->r[3] + 152u, c->r[2]);
  c->r[3] = c->mem_r32(c->r[9] + 0u);
  c->r[3] = c->r[8] + c->r[3];
  goto L_80092284;

L_800920F8:
  c->r[4] = (uint32_t)c->mem_r16s(c->r[7] + 78u);
  {
    int _t = ((int32_t)c->r[4] <= 0);
    if (_t) {
      goto L_80092168;
    }
  }
  cpu_div(c, c->r[2], c->r[4]);
  if (c->r[4] == 0u) {
    psx::cpu::handleBreak(*c, 7168u);
  }
  if (c->r[4] == 0xFFFFFFFFu && c->r[2] == 0x80000000u) {
    psx::cpu::handleBreak(*c, 6144u);
  }
  c->r[2] = c->hi;
  {
    int _t = (c->r[2] != 0u);
    if (_t) {
      return;
    }
  } // nonzero remainder -- skip EVERYTHING below, return
  c->r[3] = c->mem_r32(c->r[7] + 148u);    // cur
  c->r[4] = c->mem_r32(c->r[7] + 172u);    // target
  c->r[2] = (uint32_t)(c->r[4] < c->r[3]); // target < cur ?
  {
    int _t = (c->r[2] != 0u);
    c->r[2] = c->r[3] - 1u;
    if (_t) {
      goto L_80092160;
    }
  }
  c->r[2] = (uint32_t)(c->r[3] < c->r[4]); // cur < target ?
  {
    int _t = (c->r[2] == 0u);
    c->r[2] = c->r[3] + 1u;
    if (_t) {
      goto L_800921B0; // cur == target already: no write
    }
  }
L_80092160:
  c->mem_w32(c->r[7] + 148u, c->r[2]);
  goto L_800921B0;

L_80092168:
  c->r[3] = c->mem_r32(c->r[7] + 148u);    // cur
  c->r[8] = c->mem_r32(c->r[7] + 172u);    // target
  c->r[2] = (uint32_t)(c->r[8] < c->r[3]); // target < cur ?
  {
    int _t = (c->r[2] == 0u);
    c->r[2] = c->r[3] + c->r[4]; // cur + rate  (rate<=0 here -> decreases)
    if (_t) {
      goto L_8009218C;
    }
  }
  c->mem_w32(c->r[7] + 148u, c->r[2]);
  c->r[2] = (uint32_t)(c->r[2] < c->r[8]); // undershot target?
  goto L_800921A4;
L_8009218C:
  c->r[2] = (uint32_t)(c->r[3] < c->r[8]); // cur < target ?
  {
    int _t = (c->r[2] == 0u);
    c->r[2] = c->r[3] - c->r[4]; // cur - rate  (rate<=0 here -> increases)
    if (_t) {
      goto L_800921B0; // cur == target already: no write
    }
  }
  c->r[8] = c->mem_r32(c->r[7] + 172u);
  c->mem_w32(c->r[7] + 148u, c->r[2]);
  c->r[2] = (uint32_t)(c->r[8] < c->r[2]); // overshot target?
L_800921A4: {
  int _t = (c->r[2] == 0u);
  if (_t) {
    goto L_800921B0;
  }
}
  c->mem_w32(c->r[7] + 148u, c->r[8]); // clamp to target exactly

L_800921B0:
  c->r[2] = (uint32_t)c->mem_r16s(c->r[7] + 80u);
  c->r[3] = c->mem_r32(c->r[7] + 148u);
  {
    int64_t _p = (int64_t)(int32_t)c->r[2] * (int64_t)(int32_t)c->r[3];
    c->lo = (uint32_t)_p;
    c->hi = (uint32_t)((uint64_t)_p >> 32);
  }
  c->r[4] = c->mem_r32(0x80100000u + 19500u); // cluster-adjacent scale field
  c->r[2] = c->lo;
  c->r[3] = c->r[2] << 2;
  c->r[3] = c->r[3] + c->r[2];
  c->r[3] = c->r[3] << 1; // r3 = lo*10
  c->r[2] = c->r[4] << 4;
  c->r[2] = c->r[2] - c->r[4];
  c->r[2] = c->r[2] << 2; // r2 = scale*15
  cpu_divu(c, c->r[3], c->r[2]);
  if (c->r[2] == 0u) {
    psx::cpu::handleBreak(*c, 7168u);
  }
  c->r[3] = c->lo;
  c->mem_w16(c->r[7] + 84u, (uint16_t)c->r[3]);
  c->r[3] = c->r[3] << 16;
  {
    int _t = ((int32_t)c->r[3] > 0);
    c->r[2] = 1u;
    if (_t) {
      goto L_8009220C;
    }
  }
  c->mem_w16(c->r[7] + 84u, (uint16_t)c->r[2]);
L_8009220C:
  c->r[2] = c->mem_r32(c->r[7] + 168u);
  {
    int _t = (c->r[2] == 0u);
    if (_t) {
      goto L_80092230;
    }
  }
  c->r[3] = c->mem_r32(c->r[7] + 148u);
  c->r[2] = c->mem_r32(c->r[7] + 172u);
  {
    int _t = (c->r[3] != c->r[2]);
    if (_t) {
      return;
    }
  } // still ramping, not at target -- leave bits set
L_80092230:
  c->r[6] = c->r[6] << 16;
  c->r[2] = libsnd::kSeqPtrArray;
  c->r[6] = (uint32_t)((int32_t)c->r[6] >> 14);
  c->r[6] = c->r[6] + c->r[2];
  c->r[2] = c->r[5] << 16;
  c->r[2] = (uint32_t)((int32_t)c->r[2] >> 16);
  c->r[3] = c->r[2] << 1;
  c->r[3] = c->r[3] + c->r[2];
  c->r[3] = c->r[3] << 2;
  c->r[3] = c->r[3] - c->r[2];
  c->r[5] = c->mem_r32(c->r[6] + 0u);
  c->r[3] = c->r[3] << 4;
  c->r[5] = c->r[3] + c->r[5];
  c->r[2] = c->mem_r32(c->r[5] + 152u);
  c->r[4] = ~0x40u;
  c->r[2] = c->r[2] & c->r[4];
  c->mem_w32(c->r[5] + 152u, c->r[2]);
  c->r[2] = c->mem_r32(c->r[6] + 0u);
  c->r[3] = c->r[3] + c->r[2];
L_80092284:
  c->r[2] = c->mem_r32(c->r[3] + 152u);
  c->r[4] = ~0x80u;
  c->r[2] = c->r[2] & c->r[4];
  c->mem_w32(c->r[3] + 152u, c->r[2]);
  // L_80092294: return
}

// Both leaves below have no observed reachable call site: their flags bits never came up in any
// recorded run, so the dispatcher routes those bits to the guest body. See docs/engine_re.md.

// The dispatcher owns the C entry point; this file owns what it runs.
[[maybe_unused]] static void nat_channelPitchSlideTick(Core *c) {
  eng(c).sequencer.channelPitchSlideTick();
}

// The dispatcher owns the C entry point; this file owns what it runs.
[[maybe_unused]] static void nat_channelEnvelopeRampTick(Core *c) {
  eng(c).sequencer.channelEnvelopeRampTick();
}

} // namespace tomba::audio
