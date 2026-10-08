// game/audio/sequencer_voice_state.cpp — Sequencer::voiceStateFlush, the
// per-frame SPU voice-state flush the sound driver runs twice a field. See docs/engine_re.md.

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

// 0x800931C0 — the sound driver's per-frame SPU voice-state flush. Its guest frame spills
// s0..s7 and ra at the offsets named in the body.
void Sequencer::voiceStateFlush() {
  Core *c = core;
  using namespace tomba::audio;
  // The guest's 120-byte frame, descending sp by 120 and spilling s0..s7 and ra at +80..+112. The
  // slot numbers are the guest's own; naming them says WHICH callee-save register each one holds,
  // which is the only way a reader can tell why a body still writes to s1 twenty lines after the
  // prologue. r1 is the guest's assembler-at, not callee-saved, and is spilled at +16..+76 by the
  // body itself.
  static constexpr uint32_t kSpillSlot0 = 16, kSpillSlot1 = 20, kSpillSlot2 = 24, kSpillSlot3 = 26, kSpillSlot4 = 36,
                            kSpillSlot5 = 44, kSpillSlot6 = 74, kSpillSlot7 = 76;
  static constexpr uint32_t kSpillS0 = 80, kSpillS1 = 84, kSpillS2 = 88, kSpillS3 = 92, kSpillS4 = 96, kSpillS5 = 100,
                            kSpillS6 = 104, kSpillS7 = 108, kSpillRa = 112;
  c->r[2] = libsnd::kBase;
  c->r[2] = c->mem_r32((c->r[2] + libsnd::kVoiceCursor));
  c->r[29] = c->r[29] + (uint32_t)-120;
  c->mem_w32((c->r[29] + kSpillS0), c->r[16]);
  c->mem_w32((c->r[29] + kSpillRa), c->r[31]);
  c->mem_w32((c->r[29] + kSpillS7), c->r[23]);
  c->mem_w32((c->r[29] + kSpillS6), c->r[22]);
  c->mem_w32((c->r[29] + kSpillS5), c->r[21]);
  c->mem_w32((c->r[29] + kSpillS4), c->r[20]);
  c->mem_w32((c->r[29] + kSpillS3), c->r[19]);
  c->mem_w32((c->r[29] + kSpillS2), c->r[18]);
  c->mem_w32((c->r[29] + kSpillS1), c->r[17]);
  c->r[2] = c->r[2] + (uint32_t)1;
  c->r[2] = c->r[2] & 15u;
  c->r[1] = libsnd::kBase;
  c->mem_w32((c->r[1] + libsnd::kVoiceCursor), c->r[2]);
  c->r[2] = c->r[2] << 2;
  c->r[1] = libsnd::kBase;
  c->r[1] = c->r[1] + c->r[2];
  c->mem_w32((c->r[1] + libsnd::kVoiceStateTable), c->r[0]);
  c->r[2] = libsnd::kBase;
  c->r[2] = (uint32_t)(int8_t)c->mem_r8((c->r[2] + libsnd::kSpuKeyScanCount));
  c->r[3] = libsnd::kBase;
  c->r[3] = c->r[3] + libsnd::kVoiceStateTable;
  {
    int _t = ((int32_t)c->r[2] <= 0);
    c->r[16] = c->r[0] + c->r[0];
    if (_t) {
      goto L_800932A0;
    }
  }
  c->r[20] = c->r[3] + c->r[0];
  c->r[19] = c->r[0] + (uint32_t)1;
  c->r[18] = libsnd::kBase;
  c->r[18] = c->r[18] + libsnd::kPerVoiceTable;
  c->r[17] = c->r[0] + c->r[0];
L_8009323C:;
  c->r[4] = c->r[16] + c->r[0];
  c->r[31] = 0x80093248u;
  c->r[5] = c->r[18] + c->r[0];
  psx::cpu::dispatchGuestToReturn0(*c, 0x8009A1D0u, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
  c->r[2] = libsnd::kBase;
  c->r[2] = c->r[2] + c->r[17];
  c->r[2] = (uint32_t)c->mem_r16((c->r[2] + libsnd::kPerVoiceTable));
  {
    int _t = (c->r[2] != c->r[0]);
    c->r[4] = c->r[19] << (c->r[16] & 31);
    if (_t) {
      goto L_80093284;
    }
  }
  c->r[2] = libsnd::kBase;
  c->r[2] = c->mem_r32((c->r[2] + libsnd::kVoiceCursor));
  c->r[2] = c->r[2] << 2;
  c->r[2] = c->r[2] + c->r[20];
  c->r[3] = c->mem_r32((c->r[2] + (uint32_t)0));
  c->r[3] = c->r[3] | c->r[4];
  c->mem_w32((c->r[2] + (uint32_t)0), c->r[3]);
L_80093284:;
  c->r[18] = c->r[18] + (uint32_t)56;
  c->r[2] = libsnd::kBase;
  c->r[2] = (uint32_t)(int8_t)c->mem_r8((c->r[2] + libsnd::kSpuKeyScanCount));
  c->r[16] = c->r[16] + (uint32_t)1;
  c->r[2] = (uint32_t)((int32_t)c->r[16] < (int32_t)c->r[2]);
  {
    int _t = (c->r[2] != c->r[0]);
    c->r[17] = c->r[17] + (uint32_t)56;
    if (_t) {
      goto L_8009323C;
    }
  }
L_800932A0:;
  c->r[2] = libsnd::kBase;
  c->r[2] = (uint32_t)(int8_t)c->mem_r8((c->r[2] + libsnd::kToneCursor));
  {
    int _t = (c->r[2] != c->r[0]);
    c->r[16] = c->r[0] + c->r[0];
    if (_t) {
      goto L_8009336C;
    }
  }
  c->r[18] = c->r[0] + (uint32_t)-1;
  c->r[3] = libsnd::kBase;
  c->r[3] = c->r[3] + libsnd::kVoiceStateTable;
L_800932C0:;
  c->r[2] = c->mem_r32((c->r[3] + (uint32_t)0));
  c->r[16] = c->r[16] + (uint32_t)1;
  c->r[18] = c->r[18] & c->r[2];
  c->r[2] = (uint32_t)((int32_t)c->r[16] < 15);
  {
    int _t = (c->r[2] != c->r[0]);
    c->r[3] = c->r[3] + (uint32_t)4;
    if (_t) {
      goto L_800932C0;
    }
  }
  c->r[2] = libsnd::kBase;
  c->r[2] = (uint32_t)(int8_t)c->mem_r8((c->r[2] + libsnd::kSpuKeyScanCount));
  {
    int _t = ((int32_t)c->r[2] <= 0);
    c->r[16] = c->r[0] + c->r[0];
    if (_t) {
      goto L_80093368;
    }
  }
  c->r[19] = c->r[0] + (uint32_t)1;
  c->r[20] = c->r[0] + (uint32_t)2;
  c->r[17] = libsnd::kBase;
  c->r[17] = c->r[17] + libsnd::kPerVoiceConsumedFlag;
L_800932FC:;
  c->r[5] = c->r[19] << (c->r[16] & 31);
  c->r[2] = c->r[18] & c->r[5];
  {
    int _t = (c->r[2] == c->r[0]);
    if (_t) {
      goto L_80093350;
    }
  }
  c->r[2] = (uint32_t)(int8_t)c->mem_r8((c->r[17] + (uint32_t)0));
  {
    int _t = (c->r[2] != c->r[20]);
    c->r[2] = (uint32_t)((int32_t)c->r[16] < 16);
    if (_t) {
      goto L_8009334C;
    }
  }
  {
    int _t = (c->r[2] != c->r[0]);
    c->r[2] = c->r[0] + c->r[0];
    if (_t) {
      goto L_80093330;
    }
  }
  c->r[5] = c->r[0] + c->r[0];
  c->r[2] = c->r[16] + (uint32_t)-16;
  c->r[2] = c->r[19] << (c->r[2] & 31);
L_80093330:;
  c->r[4] = c->r[0] + c->r[0];
  c->r[2] = c->r[2] & 255u;
  c->r[2] = c->r[2] << 16;
  c->r[5] = c->r[5] << 16;
  c->r[5] = (uint32_t)((int32_t)c->r[5] >> 16);
  c->r[31] = 0x8009334Cu;
  c->r[5] = c->r[2] | c->r[5];
  psx::cpu::dispatchGuestToReturn0(*c, 0x80097E10u, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
L_8009334C:;
  c->mem_w8((c->r[17] + (uint32_t)0), (uint8_t)c->r[0]);
L_80093350:;
  c->r[2] = libsnd::kBase;
  c->r[2] = (uint32_t)(int8_t)c->mem_r8((c->r[2] + libsnd::kSpuKeyScanCount));
  c->r[16] = c->r[16] + (uint32_t)1;
  c->r[2] = (uint32_t)((int32_t)c->r[16] < (int32_t)c->r[2]);
  {
    int _t = (c->r[2] != c->r[0]);
    c->r[17] = c->r[17] + (uint32_t)56;
    if (_t) {
      goto L_800932FC;
    }
  }
L_80093368:;
  c->r[16] = c->r[0] + c->r[0];
L_8009336C:;
  c->r[2] = (uint32_t)c->mem_r16(libsnd::kKonArmedMaskLo);
  c->r[3] = (uint32_t)c->mem_r16(libsnd::kActiveVoiceMaskLo);
  c->r[2] = ~(c->r[0] | c->r[2]);
  c->r[3] = c->r[3] & c->r[2];
  c->r[2] = (uint32_t)c->mem_r16(libsnd::kKonArmedMaskHi);
  c->r[17] = c->r[0] + c->r[0];
  c->mem_w16(libsnd::kActiveVoiceMaskLo, (uint16_t)c->r[3]);
  c->r[3] = (uint32_t)c->mem_r16(libsnd::kActiveVoiceMaskHi);
  c->r[2] = ~(c->r[0] | c->r[2]);
  c->r[3] = c->r[3] & c->r[2];
  c->mem_w16(libsnd::kActiveVoiceMaskHi, (uint16_t)c->r[3]);
L_800933B0:;
  c->r[2] = libsnd::kBase;
  c->r[2] = c->r[2] + c->r[17];
  c->r[2] = (uint32_t)(int16_t)c->mem_r16((c->r[2] + libsnd::kUnnamedHalfword21734));
  {
    int _t = (c->r[2] == c->r[0]);
    if (_t) {
      goto L_800933DC;
    }
  }
  c->r[2] = libsnd::kBase;
  c->r[2] = c->mem_r32((c->r[2] + libsnd::kUnnamedWord23464));
  c->r[31] = 0x800933DCu;
  c->r[4] = c->r[16] + c->r[0];
  psx::cpu::dispatchGuestToReturn0(*c, c->r[2], psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
L_800933DC:;
  c->r[2] = libsnd::kBase;
  c->r[2] = c->r[2] + c->r[17];
  c->r[2] = (uint32_t)(int16_t)c->mem_r16((c->r[2] + libsnd::kPerVoiceDispatchLo));
  {
    int _t = (c->r[2] == c->r[0]);
    if (_t) {
      goto L_80093408;
    }
  }
  c->r[2] = libsnd::kBase;
  c->r[2] = c->mem_r32((c->r[2] + libsnd::kUnnamedWord23072));
  c->r[31] = 0x80093408u;
  c->r[4] = c->r[16] + c->r[0];
  psx::cpu::dispatchGuestToReturn0(*c, c->r[2], psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
L_80093408:;
  c->r[16] = c->r[16] + (uint32_t)1;
  c->r[2] = (uint32_t)((int32_t)c->r[16] < 24);
  {
    int _t = (c->r[2] != c->r[0]);
    c->r[17] = c->r[17] + (uint32_t)56;
    if (_t) {
      goto L_800933B0;
    }
  }
  c->r[16] = c->r[0] + c->r[0];
  c->r[17] = libsnd::kBase;
  c->r[17] = c->r[17] + libsnd::kToneBlockBase;
  c->r[2] = libsnd::kBase;
  c->r[2] = c->r[2] + libsnd::kUnnamedWord23080;
  c->r[23] = c->r[2] + (uint32_t)10;
  c->r[22] = c->r[2] + (uint32_t)8;
  c->r[21] = c->r[2] + (uint32_t)6;
  c->r[20] = c->r[2] + (uint32_t)4;
  c->r[19] = c->r[2] + (uint32_t)2;
  c->r[18] = c->r[2] + c->r[0];
L_80093444:;
  c->r[2] = c->r[0] + (uint32_t)1;
  c->r[2] = c->r[2] << (c->r[16] & 31);
  c->mem_w32((c->r[29] + kSpillSlot1), c->r[0]);
  c->mem_w32((c->r[29] + kSpillSlot0), c->r[2]);
  c->r[2] = (uint32_t)c->mem_r8((c->r[17] + (uint32_t)0));
  c->r[2] = c->r[2] & 1u;
  {
    int _t = (c->r[2] == c->r[0]);
    c->r[2] = c->r[0] + (uint32_t)3;
    if (_t) {
      goto L_80093484;
    }
  }
  c->mem_w32((c->r[29] + kSpillSlot1), c->r[2]);
  c->r[2] = (uint32_t)c->mem_r16((c->r[18] + (uint32_t)0));
  c->mem_w16((c->r[29] + kSpillSlot2), (uint16_t)c->r[2]);
  c->r[2] = (uint32_t)c->mem_r16((c->r[19] + (uint32_t)0));
  c->mem_w16((c->r[29] + kSpillSlot3), (uint16_t)c->r[2]);
L_80093484:;
  c->r[2] = (uint32_t)c->mem_r8((c->r[17] + (uint32_t)0));
  c->r[2] = c->r[2] & 4u;
  {
    int _t = (c->r[2] == c->r[0]);
    if (_t) {
      goto L_800934B4;
    }
  }
  c->r[2] = c->mem_r32((c->r[29] + kSpillSlot1));
  c->r[2] = c->r[2] | 16u;
  c->mem_w32((c->r[29] + kSpillSlot1), c->r[2]);
  c->r[2] = (uint32_t)c->mem_r16((c->r[20] + (uint32_t)0));
  c->mem_w16((c->r[29] + kSpillSlot4), (uint16_t)c->r[2]);
L_800934B4:;
  c->r[2] = (uint32_t)c->mem_r8((c->r[17] + (uint32_t)0));
  c->r[2] = c->r[2] & 8u;
  {
    int _t = (c->r[2] == c->r[0]);
    if (_t) {
      goto L_800934E8;
    }
  }
  c->r[2] = c->mem_r32((c->r[29] + kSpillSlot1));
  c->r[2] = c->r[2] | 128u;
  c->mem_w32((c->r[29] + kSpillSlot1), c->r[2]);
  c->r[2] = (uint32_t)c->mem_r16((c->r[21] + (uint32_t)0));
  c->r[2] = c->r[2] << 3;
  c->mem_w32((c->r[29] + kSpillSlot5), c->r[2]);
L_800934E8:;
  c->r[2] = (uint32_t)c->mem_r8((c->r[17] + (uint32_t)0));
  c->r[2] = c->r[2] & 16u;
  {
    int _t = (c->r[2] == c->r[0]);
    c->r[3] = (uint32_t)6u << 16;
    if (_t) {
      goto L_80093524;
    }
  }
  c->r[2] = c->mem_r32((c->r[29] + kSpillSlot1));
  c->r[2] = c->r[2] | c->r[3];
  c->mem_w32((c->r[29] + kSpillSlot1), c->r[2]);
  c->r[2] = (uint32_t)c->mem_r16((c->r[22] + (uint32_t)0));
  c->mem_w16((c->r[29] + kSpillSlot6), (uint16_t)c->r[2]);
  c->r[2] = (uint32_t)c->mem_r16((c->r[23] + (uint32_t)0));
  c->mem_w16((c->r[29] + kSpillSlot7), (uint16_t)c->r[2]);
L_80093524:;
  c->r[2] = c->mem_r32((c->r[29] + kSpillSlot1));
  {
    int _t = (c->r[2] == c->r[0]);
    if (_t) {
      goto L_8009353C;
    }
  }
  c->r[31] = 0x8009353Cu;
  c->r[4] = c->r[29] + (uint32_t)16;
  psx::cpu::dispatchGuestToReturn0(*c, 0x80099970u, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
L_8009353C:;
  c->mem_w8((c->r[17] + (uint32_t)0), (uint8_t)c->r[0]);
  c->r[17] = c->r[17] + (uint32_t)1;
  c->r[23] = c->r[23] + (uint32_t)16;
  c->r[22] = c->r[22] + (uint32_t)16;
  c->r[21] = c->r[21] + (uint32_t)16;
  c->r[20] = c->r[20] + (uint32_t)16;
  c->r[19] = c->r[19] + (uint32_t)16;
  c->r[16] = c->r[16] + (uint32_t)1;
  c->r[2] = (uint32_t)((int32_t)c->r[16] < 24);
  {
    int _t = (c->r[2] != c->r[0]);
    c->r[18] = c->r[18] + (uint32_t)16;
    if (_t) {
      goto L_80093444;
    }
  }
  c->r[4] = c->r[0] + c->r[0];
  c->r[5] = (uint32_t)c->mem_r8(libsnd::kKonArmedMaskHi);
  c->r[2] = (uint32_t)c->mem_r16(libsnd::kKonArmedMaskLo);
  c->r[5] = c->r[5] << 16;
  c->r[31] = 0x80093588u;
  c->r[5] = c->r[5] | c->r[2];
  psx::cpu::dispatchGuestToReturn0(*c, 0x80098F90u, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
  c->r[4] = c->r[0] + (uint32_t)1;
  c->r[5] = (uint32_t)c->mem_r8(libsnd::kActiveVoiceMaskHi);
  c->r[2] = (uint32_t)c->mem_r16(libsnd::kActiveVoiceMaskLo);
  c->r[5] = c->r[5] << 16;
  c->r[31] = 0x800935A8u;
  c->r[5] = c->r[5] | c->r[2];
  psx::cpu::dispatchGuestToReturn0(*c, 0x80098F90u, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
  c->r[4] = c->r[0] + (uint32_t)8;
  c->r[5] = libsnd::kBase;
  c->r[5] = (uint32_t)c->mem_r8((c->r[5] + libsnd::kPerVoicePitchHi));
  c->r[2] = libsnd::kBase;
  c->r[2] = (uint32_t)c->mem_r16((c->r[2] + libsnd::kPerVoicePitchLo));
  c->r[5] = c->r[5] << 16;
  c->r[31] = 0x800935C8u;
  c->r[5] = c->r[5] | c->r[2];
  psx::cpu::dispatchGuestToReturn0(*c, 0x80098DB0u, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
  c->r[4] = c->r[0] + (uint32_t)8;
  c->r[5] = libsnd::kBase;
  c->r[5] = (uint32_t)c->mem_r8((c->r[5] + libsnd::kPerVoiceLevelHi));
  c->r[2] = libsnd::kBase;
  c->r[2] = (uint32_t)c->mem_r16((c->r[2] + libsnd::kPerVoiceLevelLo));
  c->r[5] = c->r[5] << 16;
  c->r[31] = 0x800935E8u;
  c->r[5] = c->r[5] | c->r[2];
  psx::cpu::dispatchGuestToReturn0(*c, 0x80097E10u, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
  c->mem_w16(libsnd::kKonArmedMaskLo, (uint16_t)c->r[0]);
  c->mem_w16(libsnd::kKonArmedMaskHi, (uint16_t)c->r[0]);
  c->mem_w16(libsnd::kActiveVoiceMaskLo, (uint16_t)c->r[0]);
  c->mem_w16(libsnd::kActiveVoiceMaskHi, (uint16_t)c->r[0]);
  c->r[1] = libsnd::kBase;
  c->mem_w16((c->r[1] + libsnd::kPerVoiceLevelLo), (uint16_t)c->r[0]);
  c->r[1] = libsnd::kBase;
  c->mem_w16((c->r[1] + libsnd::kPerVoiceLevelHi), (uint16_t)c->r[0]);
  c->r[31] = c->mem_r32((c->r[29] + kSpillRa));
  c->r[23] = c->mem_r32((c->r[29] + kSpillS7));
  c->r[22] = c->mem_r32((c->r[29] + kSpillS6));
  c->r[21] = c->mem_r32((c->r[29] + kSpillS5));
  c->r[20] = c->mem_r32((c->r[29] + kSpillS4));
  c->r[19] = c->mem_r32((c->r[29] + kSpillS3));
  c->r[18] = c->mem_r32((c->r[29] + kSpillS2));
  c->r[17] = c->mem_r32((c->r[29] + kSpillS1));
  c->r[16] = c->mem_r32((c->r[29] + kSpillS0));
  c->r[29] = c->r[29] + (uint32_t)120;
  return;
}

// The dispatcher owns the C entry point; this file owns what it runs.
static void nat_voiceStateFlush(Core *c) {
  eng(c).sequencer.voiceStateFlush();
}

void declareVoiceStateOverrides() {
  tomba::native::declareOverride(0x800931C0u, "nat_voiceStateFlush", nat_voiceStateFlush);
}

} // namespace tomba::audio
