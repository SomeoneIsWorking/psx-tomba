// game/audio/sequencer_voice_alloc.cpp — Sequencer::voiceAllocateOrSteal, the
// sequencer's choice of which hardware voice a note takes. See docs/engine_re.md.

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

// 0x80094150 — voice allocation and stealing. True leaf, no stack frame.
void Sequencer::voiceAllocateOrSteal() {
  Core *c = core;
  c->r[11] = c->r[0] + 99u;
  c->r[12] = c->r[0] | 65535u;
  c->r[10] = c->r[0] + c->r[0];
  c->r[8] = c->r[0] + c->r[0];
  c->r[9] = c->r[0] + 99u;
  c->r[7] = c->r[0] + c->r[0];
  c->r[2] = (uint32_t)32784u << 16;
  c->r[2] = (uint32_t)c->mem_r8(c->r[2] + 23815u);
  c->r[3] = (uint32_t)32784u << 16;
  c->r[3] = (uint32_t)(int8_t)c->mem_r8(c->r[3] + 23788u);
  c->r[2] = c->r[2] << 24;
  {
    int _t = ((int32_t)c->r[3] <= 0);
    c->r[13] = (uint32_t)((int32_t)c->r[2] >> 24);
    if (_t) {
      goto L_800942C8;
    }
  }
  c->r[24] = c->r[0] + 1u;
  c->r[15] = (uint32_t)32779u << 16;
  c->r[15] = c->mem_r32(c->r[15] + (uint32_t)-15372);
  c->r[14] = c->r[3] + c->r[0];
  c->r[3] = c->r[7] & 255u;
L_80094198:
  c->r[2] = c->r[24] << (c->r[3] & 31);
  c->r[2] = c->r[15] & c->r[2];
  {
    int _t = (c->r[2] != c->r[0]);
    c->r[2] = c->r[3] << 3;
    if (_t) {
      goto L_800942B4;
    }
  }
  c->r[2] = c->r[2] - c->r[3];
  c->r[3] = c->r[2] << 3;
  c->r[2] = (uint32_t)32784u << 16;
  c->r[2] = c->r[2] + c->r[3];
  c->r[2] = (uint32_t)(int8_t)c->mem_r8(c->r[2] + 21733u);
  {
    int _t = (c->r[2] != c->r[0]);
    c->r[2] = c->r[7] & 255u;
    if (_t) {
      goto L_800941E8;
    }
  }
  c->r[2] = (uint32_t)32784u << 16;
  c->r[2] = c->r[2] + c->r[3];
  c->r[2] = (uint32_t)c->mem_r16(c->r[2] + 21710u);
  {
    int _t = (c->r[2] != c->r[0]);
    c->r[2] = c->r[7] & 255u;
    if (_t) {
      goto L_800941E8;
    }
  }
  c->r[11] = c->r[7] + c->r[0];
  goto L_800942C8;
L_800941E8:
  c->r[3] = c->r[2] << 3;
  c->r[3] = c->r[3] - c->r[2];
  c->r[3] = c->r[3] << 3;
  c->r[5] = c->r[13] & 65535u;
  c->r[6] = (uint32_t)32784u << 16;
  c->r[6] = c->r[6] + c->r[3];
  c->r[6] = (uint32_t)(int16_t)c->mem_r16(c->r[6] + 21730u);
  c->r[4] = (uint32_t)32784u << 16;
  c->r[4] = c->r[4] + c->r[3];
  c->r[4] = (uint32_t)c->mem_r16(c->r[4] + 21730u);
  c->r[2] = (uint32_t)((int32_t)c->r[6] < (int32_t)c->r[5]);
  {
    int _t = (c->r[2] == c->r[0]);
    if (_t) {
      goto L_80094244;
    }
  }
  c->r[13] = c->r[4] + c->r[0];
  c->r[9] = c->r[7] + c->r[0];
  c->r[12] = (uint32_t)32784u << 16;
  c->r[12] = c->r[12] + c->r[3];
  c->r[12] = (uint32_t)c->mem_r16(c->r[12] + 21710u);
  c->r[8] = (uint32_t)32784u << 16;
  c->r[8] = c->r[8] + c->r[3];
  c->r[8] = (uint32_t)c->mem_r16(c->r[8] + 21706u);
  c->r[10] = c->r[0] + 1u;
  goto L_800942B4;
L_80094244: {
  int _t = (c->r[6] != c->r[5]);
  c->r[4] = c->r[12] & 65535u;
  if (_t) {
    goto L_800942B4;
  }
}
  c->r[6] = (uint32_t)32784u << 16;
  c->r[6] = c->r[6] + c->r[3];
  c->r[6] = (uint32_t)c->mem_r16(c->r[6] + 21710u);
  c->r[5] = c->r[6] & 65535u;
  c->r[2] = (uint32_t)(c->r[5] < c->r[4]);
  {
    int _t = (c->r[2] == c->r[0]);
    c->r[10] = c->r[10] + 1u;
    if (_t) {
      goto L_80094280;
    }
  }
  c->r[8] = (uint32_t)32784u << 16;
  c->r[8] = c->r[8] + c->r[3];
  c->r[8] = (uint32_t)c->mem_r16(c->r[8] + 21706u);
  c->r[12] = c->r[6] + c->r[0];
  goto L_800942B0;
L_80094280: {
  int _t = (c->r[5] != c->r[4]);
  if (_t) {
    goto L_800942B4;
  }
}
  c->r[2] = (uint32_t)32784u << 16;
  c->r[2] = c->r[2] + c->r[3];
  c->r[2] = (uint32_t)(int16_t)c->mem_r16(c->r[2] + 21706u);
  c->r[1] = (uint32_t)32784u << 16;
  c->r[1] = c->r[1] + c->r[3];
  c->r[3] = (uint32_t)c->mem_r16(c->r[1] + 21706u);
  c->r[2] = (uint32_t)((int32_t)c->r[8] < (int32_t)c->r[2]);
  {
    int _t = (c->r[2] == c->r[0]);
    if (_t) {
      goto L_800942B4;
    }
  }
  c->r[8] = c->r[3] + c->r[0];
L_800942B0:
  c->r[9] = c->r[7] + c->r[0];
L_800942B4:
  c->r[7] = c->r[7] + 1u;
  c->r[2] = c->r[7] & 255u;
  c->r[2] = (uint32_t)((int32_t)c->r[2] < (int32_t)c->r[14]);
  {
    int _t = (c->r[2] != c->r[0]);
    c->r[3] = c->r[7] & 255u;
    if (_t) {
      goto L_80094198;
    }
  }
L_800942C8:
  c->r[3] = c->r[11] & 255u;
  c->r[2] = c->r[0] + 99u;
  {
    int _t = (c->r[3] != c->r[2]);
    c->r[2] = c->r[10] & 255u;
    if (_t) {
      goto L_800942E8;
    }
  }
  {
    int _t = (c->r[2] != c->r[0]);
    c->r[11] = c->r[9] + c->r[0];
    if (_t) {
      goto L_800942E8;
    }
  }
  c->r[11] = (uint32_t)32784u << 16;
  c->r[11] = (uint32_t)c->mem_r8(c->r[11] + 23788u);
L_800942E8:
  c->r[3] = (uint32_t)32784u << 16;
  c->r[3] = (uint32_t)(int8_t)c->mem_r8(c->r[3] + 23788u);
  c->r[2] = c->r[11] & 255u;
  c->r[2] = (uint32_t)((int32_t)c->r[2] < (int32_t)c->r[3]);
  {
    int _t = (c->r[2] == c->r[0]);
    if (_t) {
      goto L_800943B8;
    }
  }
  {
    int _t = ((int32_t)c->r[3] <= 0);
    c->r[7] = c->r[0] + c->r[0];
    if (_t) {
      goto L_80094368;
    }
  }
  c->r[8] = c->r[0] + 1u;
  c->r[6] = (uint32_t)32779u << 16;
  c->r[6] = c->mem_r32(c->r[6] + (uint32_t)-15372);
  c->r[5] = c->r[3] + c->r[0];
  c->r[4] = c->r[7] & 255u;
L_8009431C:
  c->r[2] = c->r[8] << (c->r[4] & 31);
  c->r[2] = c->r[6] & c->r[2];
  {
    int _t = (c->r[2] != c->r[0]);
    c->r[3] = c->r[4] << 3;
    if (_t) {
      goto L_80094354;
    }
  }
  c->r[3] = c->r[3] - c->r[4];
  c->r[3] = c->r[3] << 3;
  c->r[2] = (uint32_t)32784u << 16;
  c->r[2] = c->r[2] + c->r[3];
  c->r[2] = (uint32_t)c->mem_r16(c->r[2] + 21706u);
  c->r[2] = c->r[2] + 1u;
  c->r[1] = (uint32_t)32784u << 16;
  c->r[1] = c->r[1] + c->r[3];
  c->mem_w16(c->r[1] + 21706u, (uint16_t)c->r[2]);
L_80094354:
  c->r[7] = c->r[7] + 1u;
  c->r[2] = c->r[7] & 255u;
  c->r[2] = (uint32_t)((int32_t)c->r[2] < (int32_t)c->r[5]);
  {
    int _t = (c->r[2] != c->r[0]);
    c->r[4] = c->r[7] & 255u;
    if (_t) {
      goto L_8009431C;
    }
  }
L_80094368:
  c->r[2] = c->r[11] & 255u;
  c->r[3] = c->r[2] << 3;
  c->r[3] = c->r[3] - c->r[2];
  c->r[3] = c->r[3] << 3;
  c->r[1] = (uint32_t)32784u << 16;
  c->r[1] = c->r[1] + c->r[3];
  c->mem_w16(c->r[1] + 21706u, (uint16_t)c->r[0]);
  c->r[2] = (uint32_t)32784u << 16;
  c->r[2] = (uint32_t)c->mem_r8(c->r[2] + 23815u);
  c->r[1] = (uint32_t)32784u << 16;
  c->r[1] = c->r[1] + c->r[3];
  c->mem_w16(c->r[1] + 21746u, (uint16_t)c->r[0]);
  c->r[1] = (uint32_t)32784u << 16;
  c->r[1] = c->r[1] + c->r[3];
  c->mem_w16(c->r[1] + 21734u, (uint16_t)c->r[0]);
  c->r[2] = c->r[2] << 24;
  c->r[2] = (uint32_t)((int32_t)c->r[2] >> 24);
  c->r[1] = (uint32_t)32784u << 16;
  c->r[1] = c->r[1] + c->r[3];
  c->mem_w16(c->r[1] + 21730u, (uint16_t)c->r[2]);
L_800943B8:
  c->r[2] = c->r[11] & 255u;
}

// The dispatcher owns the C entry point; this file owns what it runs.
static void nat_voiceAllocateOrSteal(Core *c) {
  eng(c).sequencer.voiceAllocateOrSteal();
}

void declareVoiceAllocOverrides() {
  tomba::native::declareOverride(0x80094150u, "nat_voiceAllocateOrSteal", nat_voiceAllocateOrSteal);
}

} // namespace tomba::audio
