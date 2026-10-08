// game/audio/sequencer_voice_write.cpp — the Sequencer leaves that write SPU voice
// registers: instrument/voice selection, the register-write pipeline, and voice key-on. See
// docs/engine_re.md.

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

// 0x800962B0 — voice selection, called mid-loop by channelVoiceRegisterWrite(). True leaf, no
// stack frame. It consumes whatever is live in r4/r5/r6/r7 at the call site, so the outer
// function's r6/r7 carry through unchanged.
void Sequencer::channelVoiceSelectPrep() {
  Core *c = core;
  c->r[7] = c->r[4] + c->r[0];
  c->r[2] = c->r[7] & 65535u;
  c->r[2] = (uint32_t)(c->r[2] < 16u);
  {
    int _t = (c->r[2] == c->r[0]);
    c->r[8] = c->r[5] + c->r[0];
    if (_t) {
      goto L_80096300;
    }
  }
  c->r[2] = c->r[4] << 16;
  c->r[4] = (uint32_t)((int32_t)c->r[2] >> 16);
  c->r[3] = 0x80100000u + c->r[4];
  c->r[3] = (uint32_t)c->mem_r8(c->r[3] + 23832u);
  c->r[2] = c->r[0] + 1u;
  {
    int _t = (c->r[3] != c->r[2]);
    c->r[2] = (uint32_t)-1;
    if (_t) {
      goto L_80096368;
    }
  }
  c->r[3] = c->r[5] << 16;
  c->r[2] = 0x80100000u;
  c->r[2] = (uint32_t)(int16_t)c->mem_r16(c->r[2] + 23770u);
  c->r[6] = (uint32_t)((int32_t)c->r[3] >> 16);
  c->r[2] = (uint32_t)((int32_t)c->r[6] < (int32_t)c->r[2]);
  {
    int _t = (c->r[2] != c->r[0]);
    c->r[2] = c->r[4] << 2;
    if (_t) {
      goto L_80096308;
    }
  }
  c->r[2] = (uint32_t)-1;
  goto L_80096368;

L_80096308:
  c->r[3] = 0x80100000u + c->r[2];
  c->r[3] = c->mem_r32(c->r[3] + 23632u);
  c->r[5] = 0x80100000u + c->r[2];
  c->r[5] = c->mem_r32(c->r[5] + 23568u);
  c->r[1] = 0x80100000u + c->r[2];
  c->r[2] = c->mem_r32(c->r[1] + 23704u);
  c->r[4] = 0x80100000u + 23801u;
  c->mem_w8(c->r[4] + 0u, (uint8_t)c->r[7]);
  c->mem_w8(c->r[4] + 5u, (uint8_t)c->r[8]);
  c->mem_w32(0x80100000u + 23784u, c->r[2]);
  c->r[2] = c->r[6] << 4;
  c->r[2] = c->r[2] + c->r[5];
  c->mem_w32(0x80100000u + 23780u, c->r[3]);
  c->mem_w32(0x80100000u + 23772u, c->r[5]);
  c->r[3] = (uint32_t)c->mem_r8(c->r[2] + 8u);
  c->r[2] = c->r[0] + c->r[0];
  c->mem_w8(c->r[4] + 6u, (uint8_t)c->r[3]);
L_80096300:;
L_80096368:;
}

// 0x80095530 — the SPU voice-register write leaf. Stack frame mirrored (sp-64; r16..r23 =
// s0..s7, r30 = s8/fp, r31 = ra).
//
// Register-literal on purpose: several branches re-converge on shared tails and this is dense
// fixed-point arithmetic where a hand restructure risks a silent operand-order error.
void Sequencer::channelVoiceRegisterWrite() {
  Core *c = core;
  // GuestFrame declared BEFORE any of r16..r23/r30/ra are touched below — the r7/r3/r2 scratch
  // math above them never reads/writes a callee-saved register, so hoisting the frame here captures
  // the identical pre-call values gen's own spill sequence did (same reasoning as
  // channelPitchSlideTick's frame comment). Internal control flow/register usage kept LITERAL past
  // this point — see this function's header comment for why.
  static constexpr GuestFrameSpill kSpills[] = {
      {31, 60}, {30, 56}, {23, 52}, {22, 48}, {21, 44}, {20, 40}, {19, 36}, {18, 32}, {17, 28}, {16, 24}};
  GuestFrame<64, 10> frame(c, kSpills);
  c->r[7] = c->r[4] & 255u;
  c->r[7] = c->r[7] << 2;
  c->r[3] = c->r[4] & 65280u;
  c->r[3] = (uint32_t)((int32_t)c->r[3] >> 8);
  c->r[2] = c->r[3] << 1;
  c->r[2] = c->r[2] + c->r[3];
  c->r[2] = c->r[2] << 2;
  c->r[2] = c->r[2] - c->r[3];
  c->r[3] = 0x80100000u + c->r[7];
  c->r[3] = c->mem_r32(c->r[3] + 19504u); // libsnd::kSeqPtrArray-relative, matches chBase() convention
  c->r[2] = c->r[2] << 4;
  c->r[17] = c->r[3] + c->r[2]; // channelBase
  c->mem_w16(c->r[17] + 88u, (uint16_t)c->r[5]);
  c->r[2] = (uint32_t)c->mem_r16(c->r[17] + 88u);
  c->r[22] = c->r[4] + c->r[0];
  c->r[2] = (uint32_t)(c->r[2] < 127u);
  {
    int _t = (c->r[2] != c->r[0]);
    c->mem_w16(c->r[17] + 90u, (uint16_t)c->r[6]);
    if (_t) {
      goto L_800955B0;
    }
  }
  c->r[2] = c->r[0] + 127u;
  c->mem_w16(c->r[17] + 88u, (uint16_t)c->r[2]);
L_800955B0:
  c->r[2] = (uint32_t)c->mem_r16(c->r[17] + 90u);
  c->r[2] = (uint32_t)(c->r[2] < 127u);
  {
    int _t = (c->r[2] != c->r[0]);
    c->r[2] = c->r[0] + 127u;
    if (_t) {
      goto L_800955C8;
    }
  }
  c->mem_w16(c->r[17] + 90u, (uint16_t)c->r[2]);
L_800955C8:
  c->r[2] = (uint32_t)(int8_t)c->mem_r8(0x80100000u + 23788u); // libsnd::kSpuKeyScanCount
  {
    int _t = ((int32_t)c->r[2] <= 0);
    c->r[18] = c->r[0] + c->r[0];
    if (_t) {
      goto L_80095A64;
    }
  }
  c->r[21] = ((uint32_t)516u << 16) | 2065u;
  c->r[23] = c->r[0] + 127u;
  c->r[19] = ((uint32_t)33288u << 16) | 8323u;
  c->r[20] = ((uint32_t)32770u << 16) | 9u;
  c->r[30] = 0x80100000u + 23080u;
  c->r[2] = c->r[18] << 16;
L_80095604:
  c->r[4] = (uint32_t)((int32_t)c->r[2] >> 16); // voice index i
  c->r[2] = c->r[0] + 1u;
  c->r[3] = c->mem_r32(libsnd::kHardwareVoiceActive);
  c->r[2] = c->r[2] << (c->r[4] & 31u);
  c->r[3] = c->r[3] & c->r[2];
  {
    int _t = (c->r[3] != c->r[0]);
    c->r[2] = c->r[18] + 1u;
    if (_t) {
      goto L_80095A44; // voice busy -- skip
    }
  }
  c->r[2] = c->r[4] << 3;
  c->r[2] = c->r[2] - c->r[4];
  c->r[16] = c->r[2] << 3; // voice*56 (record base, libsnd::kPerVoiceTable-8)
  c->r[3] = 0x80100000u + c->r[16];
  c->r[3] = (uint32_t)(int16_t)c->mem_r16(c->r[3] + 21720u);
  c->r[2] = c->r[22] << 16;
  c->r[2] = (uint32_t)((int32_t)c->r[2] >> 16);
  {
    int _t = (c->r[3] != c->r[2]);
    c->r[2] = c->r[18] + 1u;
    if (_t) {
      goto L_80095A44;
    }
  }
  c->r[4] = 0x80100000u + c->r[16];
  c->r[4] = (uint32_t)(int16_t)c->mem_r16(c->r[4] + 21728u);
  c->r[2] = (uint32_t)(int8_t)c->mem_r8(c->r[17] + 38u);
  {
    int _t = (c->r[4] != c->r[2]);
    c->r[2] = c->r[18] + 1u;
    if (_t) {
      goto L_80095A44;
    }
  }
  c->r[5] = 0x80100000u + c->r[16];
  c->r[5] = (uint32_t)(int16_t)c->mem_r16(c->r[5] + 21722u);
  c->r[31] = 0x8009567Cu; // FIX: gen sets the real return-site const before the jal
  channelVoiceSelectPrep();
  c->r[2] = 0x80100000u + c->r[16];
  c->r[2] = (uint32_t)(int16_t)c->mem_r16(c->r[2] + 21716u);
  c->r[3] = 0x80100000u + c->r[16];
  c->r[3] = (uint32_t)(int16_t)c->mem_r16(c->r[3] + 21712u);
  c->r[2] = c->r[2] << 1;
  c->r[2] = c->r[2] + c->r[17];
  c->r[2] = (uint32_t)(int16_t)c->mem_r16(c->r[2] + 96u);
  {
    int64_t _p = (int64_t)(int32_t)c->r[3] * (int64_t)(int32_t)c->r[2];
    c->lo = (uint32_t)_p;
    c->hi = (uint32_t)((uint64_t)_p >> 32);
  }
  c->r[3] = c->lo;
  c->r[2] = ((uint32_t)33026u << 16) | 1033u;
  {
    int64_t _p = (int64_t)(int32_t)c->r[3] * (int64_t)(int32_t)c->r[2];
    c->lo = (uint32_t)_p;
    c->hi = (uint32_t)((uint64_t)_p >> 32);
  }
  c->r[7] = c->hi;
  c->r[4] = c->r[7] + c->r[3];
  c->r[4] = (uint32_t)((int32_t)c->r[4] >> 6);
  c->r[3] = (uint32_t)((int32_t)c->r[3] >> 31);
  c->r[4] = c->r[4] - c->r[3];
  c->r[3] = c->mem_r32(0x80100000u + 23780u);
  c->r[2] = c->r[4] << 14;
  c->r[3] = (uint32_t)c->mem_r8(c->r[3] + 24u);
  c->r[2] = c->r[2] - c->r[4];
  {
    int64_t _p = (int64_t)(int32_t)c->r[3] * (int64_t)(int32_t)c->r[2];
    c->lo = (uint32_t)_p;
    c->hi = (uint32_t)((uint64_t)_p >> 32);
  }
  c->r[3] = c->lo;
  c->r[2] = ((uint32_t)33286u << 16) | 4137u;
  {
    int64_t _p = (int64_t)(int32_t)c->r[3] * (int64_t)(int32_t)c->r[2];
    c->lo = (uint32_t)_p;
    c->hi = (uint32_t)((uint64_t)_p >> 32);
  }
  c->r[2] = 0x80100000u + c->r[16];
  c->r[2] = (uint32_t)(int16_t)c->mem_r16(c->r[2] + 21724u);
  c->r[4] = c->mem_r32(0x80100000u + 23772u);
  c->r[2] = c->r[2] << 4;
  c->r[2] = c->r[2] + c->r[4];
  c->r[2] = (uint32_t)c->mem_r8(c->r[2] + 1u);
  c->r[7] = c->hi;
  c->r[5] = c->r[7] + c->r[3];
  c->r[5] = (uint32_t)((int32_t)c->r[5] >> 13);
  c->r[3] = (uint32_t)((int32_t)c->r[3] >> 31);
  c->r[4] = c->r[5] - c->r[3];
  {
    int64_t _p = (int64_t)(int32_t)c->r[4] * (int64_t)(int32_t)c->r[2];
    c->lo = (uint32_t)_p;
    c->hi = (uint32_t)((uint64_t)_p >> 32);
  }
  c->r[6] = 0x80100000u + c->r[16];
  c->r[6] = (uint32_t)(int16_t)c->mem_r16(c->r[6] + 21722u);
  c->r[2] = 0x80100000u + c->r[16];
  c->r[2] = (uint32_t)(int16_t)c->mem_r16(c->r[2] + 21726u);
  c->r[6] = c->r[6] << 4;
  c->r[6] = c->r[6] + c->r[2];
  c->r[2] = c->mem_r32(0x80100000u + 23784u);
  c->r[6] = c->r[6] << 5;
  c->r[6] = c->r[6] + c->r[2];
  c->r[3] = c->lo;
  c->r[2] = (uint32_t)c->mem_r8(c->r[6] + 2u);
  {
    int64_t _p = (int64_t)(int32_t)c->r[3] * (int64_t)(int32_t)c->r[2];
    c->lo = (uint32_t)_p;
    c->hi = (uint32_t)((uint64_t)_p >> 32);
  }
  c->r[2] = c->lo;
  c->r[3] = ((uint32_t)1036u << 16) | 8273u;
  {
    uint64_t _p = (uint64_t)c->r[2] * (uint64_t)c->r[3];
    c->lo = (uint32_t)_p;
    c->hi = (uint32_t)(_p >> 32);
  }
  c->r[3] = c->hi;
  c->r[2] = c->r[2] - c->r[3];
  c->r[2] = c->r[2] >> 1;
  c->r[3] = c->r[3] + c->r[2];
  c->r[2] = (uint32_t)c->mem_r16(c->r[17] + 88u);
  c->r[5] = c->r[3] >> 13;
  {
    int64_t _p = (int64_t)(int32_t)c->r[5] * (int64_t)(int32_t)c->r[2];
    c->lo = (uint32_t)_p;
    c->hi = (uint32_t)((uint64_t)_p >> 32);
  }
  c->r[2] = c->lo;
  c->r[3] = (uint32_t)c->mem_r16(c->r[17] + 90u);
  {
    int64_t _p = (int64_t)(int32_t)c->r[5] * (int64_t)(int32_t)c->r[3];
    c->lo = (uint32_t)_p;
    c->hi = (uint32_t)((uint64_t)_p >> 32);
  }
  c->r[3] = c->lo;
  {
    uint64_t _p = (uint64_t)c->r[2] * (uint64_t)c->r[21];
    c->lo = (uint32_t)_p;
    c->hi = (uint32_t)(_p >> 32);
  }
  c->r[4] = c->hi;
  {
    uint64_t _p = (uint64_t)c->r[3] * (uint64_t)c->r[21];
    c->lo = (uint32_t)_p;
    c->hi = (uint32_t)(_p >> 32);
  }
  c->r[2] = c->r[2] - c->r[4];
  c->r[2] = c->r[2] >> 1;
  c->r[4] = c->r[4] + c->r[2];
  c->r[4] = c->r[4] >> 6;
  c->r[5] = c->hi;
  c->r[3] = c->r[3] - c->r[5];
  c->r[3] = c->r[3] >> 1;
  c->r[5] = c->r[5] + c->r[3];
  c->r[3] = (uint32_t)c->mem_r8(c->r[6] + 3u);
  c->r[2] = (uint32_t)(c->r[3] < 64u);
  {
    int _t = (c->r[2] == c->r[0]);
    c->r[5] = c->r[5] >> 6;
    if (_t) {
      goto L_80095828;
    }
  }
  {
    int64_t _p = (int64_t)(int32_t)c->r[5] * (int64_t)(int32_t)c->r[3];
    c->lo = (uint32_t)_p;
    c->hi = (uint32_t)((uint64_t)_p >> 32);
  }
  c->r[2] = c->lo;
  c->r[3] = ((uint32_t)1040u << 16) | 16645u;
  {
    uint64_t _p = (uint64_t)c->r[2] * (uint64_t)c->r[3];
    c->lo = (uint32_t)_p;
    c->hi = (uint32_t)(_p >> 32);
  }
  c->r[3] = c->hi;
  c->r[2] = c->r[2] - c->r[3];
  c->r[2] = c->r[2] >> 1;
  c->r[3] = c->r[3] + c->r[2];
  c->r[5] = c->r[3] >> 5;
  goto L_80095854;
L_80095828:
  c->r[2] = c->r[23] - c->r[3];
  {
    int64_t _p = (int64_t)(int32_t)c->r[4] * (int64_t)(int32_t)c->r[2];
    c->lo = (uint32_t)_p;
    c->hi = (uint32_t)((uint64_t)_p >> 32);
  }
  c->r[2] = c->lo;
  c->r[3] = ((uint32_t)1040u << 16) | 16645u;
  {
    uint64_t _p = (uint64_t)c->r[2] * (uint64_t)c->r[3];
    c->lo = (uint32_t)_p;
    c->hi = (uint32_t)(_p >> 32);
  }
  c->r[3] = c->hi;
  c->r[2] = c->r[2] - c->r[3];
  c->r[2] = c->r[2] >> 1;
  c->r[3] = c->r[3] + c->r[2];
  c->r[4] = c->r[3] >> 5;
L_80095854:
  c->r[3] = c->r[18] << 16;
  c->r[3] = (uint32_t)((int32_t)c->r[3] >> 16);
  c->r[2] = c->r[3] << 3;
  c->r[2] = c->r[2] - c->r[3];
  c->r[2] = c->r[2] << 3;
  c->r[1] = 0x80100000u + c->r[2];
  c->r[2] = (uint32_t)(int16_t)c->mem_r16(c->r[1] + 21724u);
  c->r[3] = c->mem_r32(0x80100000u + 23772u);
  c->r[2] = c->r[2] << 4;
  c->r[2] = c->r[2] + c->r[3];
  c->r[3] = (uint32_t)c->mem_r8(c->r[2] + 4u);
  c->r[2] = (uint32_t)(c->r[3] < 64u);
  {
    int _t = (c->r[2] == c->r[0]);
    c->r[2] = c->r[5] & 65535u;
    if (_t) {
      goto L_800958BC;
    }
  }
  {
    int64_t _p = (int64_t)(int32_t)c->r[2] * (int64_t)(int32_t)c->r[3];
    c->lo = (uint32_t)_p;
    c->hi = (uint32_t)((uint64_t)_p >> 32);
  }
  c->r[2] = c->lo;
  {
    int64_t _p = (int64_t)(int32_t)c->r[2] * (int64_t)(int32_t)c->r[19];
    c->lo = (uint32_t)_p;
    c->hi = (uint32_t)((uint64_t)_p >> 32);
  }
  c->r[8] = c->hi;
  c->r[2] = c->r[8] + c->r[2];
  c->r[5] = c->r[2] >> 5;
  goto L_800958EC;
L_800958BC:
  c->r[2] = c->r[4] & 65535u;
  c->r[3] = c->r[23] - c->r[3];
  {
    int64_t _p = (int64_t)(int32_t)c->r[2] * (int64_t)(int32_t)c->r[3];
    c->lo = (uint32_t)_p;
    c->hi = (uint32_t)((uint64_t)_p >> 32);
  }
  c->r[2] = c->lo;
  {
    int64_t _p = (int64_t)(int32_t)c->r[2] * (int64_t)(int32_t)c->r[19];
    c->lo = (uint32_t)_p;
    c->hi = (uint32_t)((uint64_t)_p >> 32);
  }
  c->r[8] = c->hi;
  c->r[3] = c->r[8] + c->r[2];
  c->r[3] = (uint32_t)((int32_t)c->r[3] >> 5);
  c->r[2] = (uint32_t)((int32_t)c->r[2] >> 31);
  c->r[4] = c->r[3] - c->r[2];
L_800958EC:
  c->r[2] = c->r[18] << 16;
  c->r[2] = (uint32_t)((int32_t)c->r[2] >> 16);
  c->r[3] = c->r[2] << 3;
  c->r[3] = c->r[3] - c->r[2];
  c->r[3] = c->r[3] << 3;
  c->r[1] = 0x80100000u + c->r[3];
  c->r[3] = (uint32_t)c->mem_r8(c->r[1] + 21714u);
  c->r[2] = (uint32_t)(c->r[3] < 64u);
  {
    int _t = (c->r[2] == c->r[0]);
    c->r[2] = c->r[5] & 65535u;
    if (_t) {
      goto L_80095940;
    }
  }
  {
    int64_t _p = (int64_t)(int32_t)c->r[2] * (int64_t)(int32_t)c->r[3];
    c->lo = (uint32_t)_p;
    c->hi = (uint32_t)((uint64_t)_p >> 32);
  }
  c->r[2] = c->lo;
  {
    int64_t _p = (int64_t)(int32_t)c->r[2] * (int64_t)(int32_t)c->r[19];
    c->lo = (uint32_t)_p;
    c->hi = (uint32_t)((uint64_t)_p >> 32);
  }
  c->r[8] = c->hi;
  c->r[2] = c->r[8] + c->r[2];
  c->r[5] = c->r[2] >> 5;
  goto L_80095970;
L_80095940:
  c->r[2] = c->r[4] & 65535u;
  c->r[3] = c->r[23] - c->r[3];
  {
    int64_t _p = (int64_t)(int32_t)c->r[2] * (int64_t)(int32_t)c->r[3];
    c->lo = (uint32_t)_p;
    c->hi = (uint32_t)((uint64_t)_p >> 32);
  }
  c->r[2] = c->lo;
  {
    int64_t _p = (int64_t)(int32_t)c->r[2] * (int64_t)(int32_t)c->r[19];
    c->lo = (uint32_t)_p;
    c->hi = (uint32_t)((uint64_t)_p >> 32);
  }
  c->r[8] = c->hi;
  c->r[3] = c->r[8] + c->r[2];
  c->r[3] = (uint32_t)((int32_t)c->r[3] >> 5);
  c->r[2] = (uint32_t)((int32_t)c->r[2] >> 31);
  c->r[4] = c->r[3] - c->r[2];
L_80095970:
  c->r[3] = (uint32_t)(int16_t)c->mem_r16(0x80100000u + 23768u);
  c->r[2] = c->r[0] + 1u;
  {
    int _t = (c->r[3] != c->r[2]);
    c->r[2] = c->r[4] & 65535u;
    if (_t) {
      goto L_800959A4;
    }
  }
  c->r[3] = c->r[5] & 65535u;
  c->r[2] = (uint32_t)(c->r[2] < c->r[3]);
  {
    int _t = (c->r[2] == c->r[0]);
    if (_t) {
      goto L_8009599C;
    }
  }
  c->r[4] = c->r[5] + c->r[0];
  goto L_800959A0;
L_8009599C:
  c->r[5] = c->r[4] + c->r[0];
L_800959A0:
  c->r[2] = c->r[4] & 65535u;
L_800959A4: {
  int64_t _p = (int64_t)(int32_t)c->r[2] * (int64_t)(int32_t)c->r[2];
  c->lo = (uint32_t)_p;
  c->hi = (uint32_t)((uint64_t)_p >> 32);
}
  c->r[2] = c->lo;
  c->r[3] = c->r[5] & 65535u;
  {
    int64_t _p = (int64_t)(int32_t)c->r[3] * (int64_t)(int32_t)c->r[3];
    c->lo = (uint32_t)_p;
    c->hi = (uint32_t)((uint64_t)_p >> 32);
  }
  c->r[3] = c->lo;
  {
    int64_t _p = (int64_t)(int32_t)c->r[2] * (int64_t)(int32_t)c->r[20];
    c->lo = (uint32_t)_p;
    c->hi = (uint32_t)((uint64_t)_p >> 32);
  }
  c->r[9] = c->hi;
  {
    int64_t _p = (int64_t)(int32_t)c->r[3] * (int64_t)(int32_t)c->r[20];
    c->lo = (uint32_t)_p;
    c->hi = (uint32_t)((uint64_t)_p >> 32);
  }
  c->r[6] = c->r[18] << 16;
  c->r[6] = (uint32_t)((int32_t)c->r[6] >> 16);
  c->r[5] = c->r[6] << 4;
  c->r[8] = 0x80100000u + 23082u;
  c->r[4] = c->r[9] + c->r[2];
  c->r[4] = (uint32_t)((int32_t)c->r[4] >> 13);
  c->r[2] = (uint32_t)((int32_t)c->r[2] >> 31);
  c->r[4] = c->r[4] - c->r[2];
  c->r[2] = c->r[5] + c->r[30];
  c->r[5] = c->r[5] + c->r[8];
  c->mem_w16(c->r[2] + 0u, (uint16_t)c->r[4]);
  c->r[7] = c->hi;
  c->r[2] = c->r[7] + c->r[3];
  c->r[2] = (uint32_t)((int32_t)c->r[2] >> 13);
  c->r[3] = (uint32_t)((int32_t)c->r[3] >> 31);
  c->r[2] = c->r[2] - c->r[3];
  c->mem_w16(c->r[5] + 0u, (uint16_t)c->r[2]);
  c->r[2] = 0x80100000u + c->r[6];
  c->r[2] = (uint32_t)c->mem_r8(c->r[2] + 23048u);
  c->r[2] = c->r[2] | 3u;
  c->r[1] = 0x80100000u + c->r[6];
  c->mem_w8(c->r[1] + 23048u, (uint8_t)c->r[2]);
  c->r[2] = c->r[18] + 1u;
L_80095A44:
  c->r[18] = c->r[2] + c->r[0];
  c->r[2] = c->r[2] << 16;
  c->r[3] = (uint32_t)(int8_t)c->mem_r8(0x80100000u + 23788u); // libsnd::kSpuKeyScanCount
  c->r[2] = (uint32_t)((int32_t)c->r[2] >> 16);
  c->r[2] = (uint32_t)((int32_t)c->r[2] < (int32_t)c->r[3]);
  {
    int _t = (c->r[2] != c->r[0]);
    c->r[2] = c->r[18] << 16;
    if (_t) {
      goto L_80095604;
    }
  }
L_80095A64:
  // v0 = sext16(chan) — gen's return-value setup; must read r22 BEFORE GuestFrame's destructor
  // (below, at the closing brace) restores it to the caller's pre-call value.
  c->r[2] = c->r[22] << 16;
  c->r[2] = (uint32_t)((int32_t)c->r[2] >> 16);
  // GuestFrame's destructor restores r16..r23/r30/r31 + ascends sp here.
}

// ============================================================================
// 2026-07-17 wave — six further per-channel sequencer leaves (all owned bottom-up here). Kept
// REGISTER-LITERAL internally (same convention as seqChannelDispatch/channelPitchSlideTick above):
// dense goto/label leaves + fixed-point pipelines where a hand restructure risks a silent
// operand-order/shift error only an SBS run would catch. Each is byte-faithful to its guest-visible behavior
// (same stores/calls/order/frame); the two frame leaves mirror the guest stack via GuestFrame.
// ============================================================================

// 0x80091810 — voice key-on. True leaf, no stack frame.
void Sequencer::channelVoiceKeyOn() {
  Core *c = core;
  c->r[4] = c->r[4] << 16;
  c->r[2] = (uint32_t)32784u << 16;
  c->r[2] = c->r[2] + 19504u;
  c->r[4] = (uint32_t)((int32_t)c->r[4] >> 14);
  c->r[4] = c->r[4] + c->r[2];
  c->r[5] = c->r[5] << 16;
  c->r[5] = (uint32_t)((int32_t)c->r[5] >> 16);
  c->r[6] = c->r[5] << 1;
  c->r[6] = c->r[6] + c->r[5];
  c->r[6] = c->r[6] << 2;
  c->r[6] = c->r[6] - c->r[5];
  c->r[6] = c->r[6] << 4;
  c->r[7] = c->mem_r32(c->r[4] + 0u);
  c->r[8] = c->r[0] + 1u;
  c->r[7] = c->r[7] + c->r[6];
  c->mem_w8(c->r[7] + 32u, (uint8_t)c->r[8]);
  c->mem_w8(c->r[7] + 33u, (uint8_t)c->r[0]);
  c->r[3] = c->mem_r32(c->r[4] + 0u);
  c->r[3] = c->r[6] + c->r[3];
  c->r[2] = c->mem_r32(c->r[3] + 152u);
  c->r[5] = c->r[0] + (uint32_t)-257;
  c->r[2] = c->r[2] & c->r[5];
  c->mem_w32(c->r[3] + 152u, c->r[2]);
  c->r[3] = c->mem_r32(c->r[4] + 0u);
  c->r[3] = c->r[6] + c->r[3];
  c->r[2] = c->mem_r32(c->r[3] + 152u);
  c->r[5] = c->r[0] + (uint32_t)-9;
  c->r[2] = c->r[2] & c->r[5];
  c->mem_w32(c->r[3] + 152u, c->r[2]);
  c->r[3] = c->mem_r32(c->r[4] + 0u);
  c->r[3] = c->r[6] + c->r[3];
  c->r[2] = c->mem_r32(c->r[3] + 152u);
  c->r[5] = c->r[0] + (uint32_t)-3;
  c->r[2] = c->r[2] & c->r[5];
  c->mem_w32(c->r[3] + 152u, c->r[2]);
  c->r[3] = c->mem_r32(c->r[4] + 0u);
  c->r[3] = c->r[6] + c->r[3];
  c->r[2] = c->mem_r32(c->r[3] + 152u);
  c->r[5] = c->r[0] + (uint32_t)-5;
  c->r[2] = c->r[2] & c->r[5];
  c->mem_w32(c->r[3] + 152u, c->r[2]);
  c->r[3] = c->mem_r32(c->r[4] + 0u);
  c->r[3] = c->r[6] + c->r[3];
  c->r[2] = c->mem_r32(c->r[3] + 152u);
  c->r[5] = c->r[0] + (uint32_t)-513;
  c->r[2] = c->r[2] & c->r[5];
  c->mem_w32(c->r[3] + 152u, c->r[2]);
  c->r[2] = c->mem_r32(c->r[7] + 4u);
  c->mem_w8(c->r[7] + 20u, (uint8_t)c->r[8]);
  c->mem_w32(c->r[7] + 0u, c->r[2]);
  c->r[2] = c->mem_r32(c->r[4] + 0u);
  c->r[6] = c->r[6] + c->r[2];
  c->r[2] = c->mem_r32(c->r[6] + 152u);
  c->r[2] = c->r[2] | 1u;
  c->mem_w32(c->r[6] + 152u, c->r[2]);
}

// The dispatcher owns the C entry point; this file owns what it runs.
static void nat_channelVoiceRegisterWrite(Core *c) {
  eng(c).sequencer.channelVoiceRegisterWrite();
}

// The dispatcher owns the C entry point; this file owns what it runs.
static void nat_channelVoiceSelectPrep(Core *c) {
  eng(c).sequencer.channelVoiceSelectPrep();
}

// The dispatcher owns the C entry point; this file owns what it runs.
static void nat_channelVoiceKeyOn(Core *c) {
  eng(c).sequencer.channelVoiceKeyOn();
}

void declareVoiceWriteOverrides() {
  tomba::native::declareOverride(0x80095530u, "nat_channelVoiceRegisterWrite", nat_channelVoiceRegisterWrite);
  tomba::native::declareOverride(0x800962B0u, "nat_channelVoiceSelectPrep", nat_channelVoiceSelectPrep);
  tomba::native::declareOverride(0x80091810u, "nat_channelVoiceKeyOn", nat_channelVoiceKeyOn);
}

} // namespace tomba::audio
