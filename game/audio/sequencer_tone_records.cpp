// game/audio/sequencer_tone_records.cpp — the Sequencer leaves that read stream and
// tone-table data into a channel. See docs/engine_re.md.

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

// 0x80090160 — stream accumulation. True leaf, no stack frame.
void Sequencer::channelStreamAccumulate() {
  Core *c = core;
  c->r[4] = c->r[4] << 16;
  c->r[4] = (uint32_t)((int32_t)c->r[4] >> 14);
  c->r[5] = c->r[5] << 16;
  c->r[5] = (uint32_t)((int32_t)c->r[5] >> 16);
  c->r[2] = c->r[5] << 1;
  c->r[2] = c->r[2] + c->r[5];
  c->r[2] = c->r[2] << 2;
  c->r[2] = c->r[2] - c->r[5];
  c->r[3] = (uint32_t)32784u << 16;
  c->r[3] = c->r[3] + c->r[4];
  c->r[3] = c->mem_r32(c->r[3] + 19504u);
  c->r[2] = c->r[2] << 4;
  c->r[5] = c->r[3] + c->r[2];
  c->r[2] = c->mem_r32(c->r[5] + 0u);
  c->r[4] = (uint32_t)c->mem_r8(c->r[2] + 0u);
  c->r[2] = c->r[2] + 1u;
  c->mem_w32(c->r[5] + 0u, c->r[2]);
  {
    int _t = (c->r[4] == c->r[0]);
    c->r[2] = c->r[0] + c->r[0];
    if (_t) {
      goto L_800901FC;
    }
  }
  c->r[2] = c->r[4] & 128u;
  {
    int _t = (c->r[2] == c->r[0]);
    c->r[2] = c->r[4] << 2;
    if (_t) {
      goto L_800901E8;
    }
  }
  c->r[4] = c->r[4] & 127u;
L_800901C0:
  c->r[2] = c->mem_r32(c->r[5] + 0u);
  c->r[4] = c->r[4] << 7;
  c->r[3] = (uint32_t)c->mem_r8(c->r[2] + 0u);
  c->r[2] = c->r[2] + 1u;
  c->mem_w32(c->r[5] + 0u, c->r[2]);
  c->r[2] = c->r[3] & 127u;
  c->r[3] = c->r[3] & 128u;
  {
    int _t = (c->r[3] != c->r[0]);
    c->r[4] = c->r[4] + c->r[2];
    if (_t) {
      goto L_800901C0;
    }
  }
  c->r[2] = c->r[4] << 2;
L_800901E8:
  c->r[2] = c->r[2] + c->r[4];
  c->r[3] = c->mem_r32(c->r[5] + 136u);
  c->r[2] = c->r[2] << 1;
  c->r[3] = c->r[3] + c->r[2];
  c->mem_w32(c->r[5] + 136u, c->r[3]);
L_800901FC:;
}

// 0x80092310 — tone record copy. Stack frame mirrored (sp-32).
void Sequencer::channelToneRecordCopy() {
  Core *c = core;
  static constexpr GuestFrameSpill kSpills[] = {{16, 16}, {17, 20}, {31, 24}}; // -32
  GuestFrame<32, 3> frame(c, kSpills);
  c->r[4] = c->r[4] << 16;
  c->r[4] = (uint32_t)((int32_t)c->r[4] >> 16);
  c->r[3] = (uint32_t)32784u << 16;
  c->r[3] = c->r[3] + c->r[4];
  c->r[3] = (uint32_t)c->mem_r8(c->r[3] + 23832u);
  c->r[2] = c->r[0] + 1u;
  {
    int _t = (c->r[3] == c->r[2]);
    c->r[17] = c->r[6] + c->r[0];
    if (_t) {
      goto L_80092348;
    }
  }
  c->r[2] = c->r[0] + (uint32_t)-1;
  goto L_80092400;
L_80092348:
  c->r[16] = c->r[5] << 16;
  c->r[16] = (uint32_t)((int32_t)c->r[16] >> 16);
  c->r[31] = 0x80092358u; // gen sets the real return-site const before the jal
  c->r[5] = c->r[16] + c->r[0];
  channelVoiceSelectPrep(); // 0x800962B0 owned -- direct native call
  c->r[2] = (uint32_t)32784u << 16;
  c->r[2] = c->mem_r32(c->r[2] + 23772u);
  c->r[16] = c->r[16] << 4;
  c->r[2] = c->r[16] + c->r[2];
  c->r[2] = (uint32_t)c->mem_r8(c->r[2] + 0u);
  c->mem_w8(c->r[17] + 0u, (uint8_t)c->r[2]);
  c->r[2] = (uint32_t)32784u << 16;
  c->r[2] = c->mem_r32(c->r[2] + 23772u);
  c->r[2] = c->r[16] + c->r[2];
  c->r[2] = (uint32_t)c->mem_r8(c->r[2] + 1u);
  c->mem_w8(c->r[17] + 1u, (uint8_t)c->r[2]);
  c->r[2] = (uint32_t)32784u << 16;
  c->r[2] = c->mem_r32(c->r[2] + 23772u);
  c->r[2] = c->r[16] + c->r[2];
  c->r[2] = (uint32_t)c->mem_r8(c->r[2] + 2u);
  c->mem_w8(c->r[17] + 2u, (uint8_t)c->r[2]);
  c->r[2] = (uint32_t)32784u << 16;
  c->r[2] = c->mem_r32(c->r[2] + 23772u);
  c->r[2] = c->r[16] + c->r[2];
  c->r[2] = (uint32_t)c->mem_r8(c->r[2] + 3u);
  c->mem_w8(c->r[17] + 3u, (uint8_t)c->r[2]);
  c->r[2] = (uint32_t)32784u << 16;
  c->r[2] = c->mem_r32(c->r[2] + 23772u);
  c->r[2] = c->r[16] + c->r[2];
  c->r[2] = (uint32_t)c->mem_r8(c->r[2] + 4u);
  c->mem_w8(c->r[17] + 4u, (uint8_t)c->r[2]);
  c->r[2] = (uint32_t)32784u << 16;
  c->r[2] = c->mem_r32(c->r[2] + 23772u);
  c->r[16] = c->r[16] + c->r[2];
  c->r[3] = (uint32_t)c->mem_r16(c->r[16] + 6u);
  c->r[2] = c->r[0] + c->r[0];
  c->mem_w16(c->r[17] + 6u, (uint16_t)c->r[3]);
L_80092400:; // return value already in r2 (0 on success, -1 on guard fail)
             // GuestFrame's destructor restores r16/r17/r31 + ascends sp here.
}

// 0x80092420 — the wide tone record copy. Stack frame mirrored (sp-32).
void Sequencer::channelToneRecordCopyWide() {
  Core *c = core;
  static constexpr GuestFrameSpill kSpills[] = {{16, 16}, {17, 20}, {31, 24}}; // -32
  GuestFrame<32, 3> frame(c, kSpills);
  c->r[17] = c->r[6] + c->r[0];
  c->r[4] = c->r[4] << 16;
  c->r[4] = (uint32_t)((int32_t)c->r[4] >> 16);
  c->r[3] = (uint32_t)32784u << 16;
  c->r[3] = c->r[3] + c->r[4];
  c->r[3] = (uint32_t)c->mem_r8(c->r[3] + 23832u);
  c->r[2] = c->r[0] + 1u;
  {
    int _t = (c->r[3] == c->r[2]);
    c->r[16] = c->r[7] + c->r[0];
    if (_t) {
      goto L_8009245C;
    }
  }
  c->r[2] = c->r[0] + (uint32_t)-1;
  goto L_80092644;
L_8009245C:
  c->r[5] = c->r[5] << 16;
  c->r[31] = 0x80092468u; // gen sets the real return-site const before the jal
  c->r[5] = (uint32_t)((int32_t)c->r[5] >> 16);
  channelVoiceSelectPrep(); // 0x800962B0 owned -- direct native call
  c->r[3] = (uint32_t)32784u << 16;
  c->r[3] = (uint32_t)(int8_t)c->mem_r8(c->r[3] + 23807u);
  c->r[2] = (uint32_t)32784u << 16;
  c->r[2] = c->mem_r32(c->r[2] + 23784u);
  c->r[3] = c->r[3] << 4;
  c->r[3] = c->r[17] + c->r[3];
  c->r[3] = c->r[3] << 16;
  c->r[3] = (uint32_t)((int32_t)c->r[3] >> 11);
  c->r[2] = c->r[3] + c->r[2];
  c->r[2] = (uint32_t)c->mem_r8(c->r[2] + 0u);
  c->mem_w8(c->r[16] + 0u, (uint8_t)c->r[2]);
  c->r[2] = (uint32_t)32784u << 16;
  c->r[2] = c->mem_r32(c->r[2] + 23784u);
  c->r[2] = c->r[3] + c->r[2];
  c->r[2] = (uint32_t)c->mem_r8(c->r[2] + 1u);
  c->mem_w8(c->r[16] + 1u, (uint8_t)c->r[2]);
  c->r[2] = (uint32_t)32784u << 16;
  c->r[2] = c->mem_r32(c->r[2] + 23784u);
  c->r[2] = c->r[3] + c->r[2];
  c->r[2] = (uint32_t)c->mem_r8(c->r[2] + 2u);
  c->mem_w8(c->r[16] + 2u, (uint8_t)c->r[2]);
  c->r[2] = (uint32_t)32784u << 16;
  c->r[2] = c->mem_r32(c->r[2] + 23784u);
  c->r[2] = c->r[3] + c->r[2];
  c->r[2] = (uint32_t)c->mem_r8(c->r[2] + 3u);
  c->mem_w8(c->r[16] + 3u, (uint8_t)c->r[2]);
  c->r[2] = (uint32_t)32784u << 16;
  c->r[2] = c->mem_r32(c->r[2] + 23784u);
  c->r[2] = c->r[3] + c->r[2];
  c->r[2] = (uint32_t)c->mem_r8(c->r[2] + 4u);
  c->mem_w8(c->r[16] + 4u, (uint8_t)c->r[2]);
  c->r[2] = (uint32_t)32784u << 16;
  c->r[2] = c->mem_r32(c->r[2] + 23784u);
  c->r[2] = c->r[3] + c->r[2];
  c->r[2] = (uint32_t)c->mem_r8(c->r[2] + 5u);
  c->mem_w8(c->r[16] + 5u, (uint8_t)c->r[2]);
  c->r[2] = (uint32_t)32784u << 16;
  c->r[2] = c->mem_r32(c->r[2] + 23784u);
  c->r[2] = c->r[3] + c->r[2];
  c->r[2] = (uint32_t)c->mem_r8(c->r[2] + 7u);
  c->mem_w8(c->r[16] + 7u, (uint8_t)c->r[2]);
  c->r[2] = (uint32_t)32784u << 16;
  c->r[2] = c->mem_r32(c->r[2] + 23784u);
  c->r[2] = c->r[3] + c->r[2];
  c->r[2] = (uint32_t)c->mem_r8(c->r[2] + 6u);
  c->mem_w8(c->r[16] + 6u, (uint8_t)c->r[2]);
  c->r[2] = (uint32_t)32784u << 16;
  c->r[2] = c->mem_r32(c->r[2] + 23784u);
  c->r[2] = c->r[3] + c->r[2];
  c->r[2] = (uint32_t)c->mem_r8(c->r[2] + 8u);
  c->mem_w8(c->r[16] + 8u, (uint8_t)c->r[2]);
  c->r[2] = (uint32_t)32784u << 16;
  c->r[2] = c->mem_r32(c->r[2] + 23784u);
  c->r[2] = c->r[3] + c->r[2];
  c->r[2] = (uint32_t)c->mem_r8(c->r[2] + 9u);
  c->mem_w8(c->r[16] + 9u, (uint8_t)c->r[2]);
  c->r[2] = (uint32_t)32784u << 16;
  c->r[2] = c->mem_r32(c->r[2] + 23784u);
  c->r[2] = c->r[3] + c->r[2];
  c->r[2] = (uint32_t)c->mem_r8(c->r[2] + 10u);
  c->mem_w8(c->r[16] + 10u, (uint8_t)c->r[2]);
  c->r[2] = (uint32_t)32784u << 16;
  c->r[2] = c->mem_r32(c->r[2] + 23784u);
  c->r[2] = c->r[3] + c->r[2];
  c->r[2] = (uint32_t)c->mem_r8(c->r[2] + 11u);
  c->mem_w8(c->r[16] + 11u, (uint8_t)c->r[2]);
  c->r[2] = (uint32_t)32784u << 16;
  c->r[2] = c->mem_r32(c->r[2] + 23784u);
  c->r[2] = c->r[3] + c->r[2];
  c->r[2] = (uint32_t)c->mem_r8(c->r[2] + 12u);
  c->mem_w8(c->r[16] + 12u, (uint8_t)c->r[2]);
  c->r[2] = (uint32_t)32784u << 16;
  c->r[2] = c->mem_r32(c->r[2] + 23784u);
  c->r[2] = c->r[3] + c->r[2];
  c->r[2] = (uint32_t)c->mem_r8(c->r[2] + 13u);
  c->mem_w8(c->r[16] + 13u, (uint8_t)c->r[2]);
  c->r[2] = (uint32_t)32784u << 16;
  c->r[2] = c->mem_r32(c->r[2] + 23784u);
  c->r[3] = c->r[3] + c->r[2];
  c->r[2] = (uint32_t)c->mem_r16(c->r[3] + 16u);
  c->mem_w16(c->r[16] + 16u, (uint16_t)c->r[2]);
  c->r[2] = (uint32_t)c->mem_r16(c->r[3] + 18u);
  c->mem_w16(c->r[16] + 18u, (uint16_t)c->r[2]);
  c->r[2] = (uint32_t)c->mem_r16(c->r[3] + 20u);
  c->mem_w16(c->r[16] + 20u, (uint16_t)c->r[2]);
  c->r[3] = (uint32_t)c->mem_r16(c->r[3] + 22u);
  c->r[2] = c->r[0] + c->r[0];
  c->mem_w16(c->r[16] + 22u, (uint16_t)c->r[3]);
L_80092644:; // return value already in r2 (0 on success, -1 on guard fail)
             // GuestFrame's destructor restores r16/r17/r31 + ascends sp here.
}

// 0x80094474 — note period computation. True leaf, no stack frame.
void Sequencer::channelNotePeriodCompute() {
  Core *c = core;
  c->r[7] = c->r[7] & 255u;
  c->r[7] = c->r[7] + c->r[5];
  c->r[7] = c->r[7] << 16;
  c->r[7] = (uint32_t)((int32_t)c->r[7] >> 16);
  {
    int _t = ((int32_t)c->r[7] >= 0);
    c->r[3] = c->r[7] + c->r[0];
    if (_t) {
      goto L_80094490;
    }
  }
  c->r[3] = c->r[7] + 127u;
L_80094490:
  c->r[3] = (uint32_t)((int32_t)c->r[3] >> 7);
  c->r[4] = c->r[4] + c->r[3];
  c->r[2] = c->r[6] & 255u;
  c->r[4] = c->r[4] - c->r[2];
  c->r[5] = c->r[4] + c->r[0];
  c->r[3] = c->r[3] << 7;
  c->r[7] = c->r[7] - c->r[3];
  c->r[2] = c->r[7] << 16;
  {
    int _t = ((int32_t)c->r[2] >= 0);
    c->r[8] = c->r[7] + c->r[0];
    if (_t) {
      goto L_800944DC;
    }
  }
  c->r[2] = c->r[7] + 128u;
  c->r[8] = c->r[2] + c->r[0];
  c->r[2] = c->r[2] << 16;
  c->r[2] = (uint32_t)((int32_t)c->r[2] >> 16);
  {
    int _t = ((int32_t)c->r[2] >= 0);
    c->r[4] = c->r[4] + (uint32_t)-1;
    if (_t) {
      goto L_800944D4;
    }
  }
  c->r[2] = c->r[2] + 127u;
L_800944D4:
  c->r[2] = (uint32_t)((int32_t)c->r[2] >> 7);
  c->r[5] = c->r[4] + c->r[2];
L_800944DC:
  c->r[3] = (uint32_t)10922u << 16;
  c->r[3] = c->r[3] | 43691u;
  c->r[2] = c->r[5] << 16;
  c->r[4] = (uint32_t)((int32_t)c->r[2] >> 16);
  {
    int64_t _p = (int64_t)(int32_t)c->r[4] * (int64_t)(int32_t)c->r[3];
    c->lo = (uint32_t)_p;
    c->hi = (uint32_t)((uint64_t)_p >> 32);
  }
  c->r[2] = (uint32_t)((int32_t)c->r[2] >> 31);
  c->r[9] = c->hi;
  c->r[3] = (uint32_t)((int32_t)c->r[9] >> 1);
  c->r[3] = c->r[3] - c->r[2];
  c->r[6] = c->r[3] + (uint32_t)-2;
  c->r[2] = c->r[3] << 1;
  c->r[2] = c->r[2] + c->r[3];
  c->r[2] = c->r[2] << 2;
  c->r[4] = c->r[4] - c->r[2];
  c->r[2] = c->r[4] << 16;
  {
    int _t = ((int32_t)c->r[2] >= 0);
    c->r[5] = c->r[4] + c->r[0];
    if (_t) {
      goto L_80094528;
    }
  }
  c->r[5] = c->r[4] + 12u;
  c->r[6] = c->r[3] + (uint32_t)-3;
L_80094528:
  c->r[3] = c->r[5] << 16;
  c->r[3] = (uint32_t)((int32_t)c->r[3] >> 15);
  c->r[2] = c->r[8] << 16;
  c->r[2] = (uint32_t)((int32_t)c->r[2] >> 15);
  c->r[1] = (uint32_t)32779u << 16;
  c->r[1] = c->r[1] + c->r[3];
  c->r[3] = (uint32_t)c->mem_r16(c->r[1] + (uint32_t)-15260);
  c->r[1] = (uint32_t)32779u << 16;
  c->r[1] = c->r[1] + c->r[2];
  c->r[2] = (uint32_t)c->mem_r16(c->r[1] + (uint32_t)-15236);
  {
    int64_t _p = (int64_t)(int32_t)c->r[3] * (int64_t)(int32_t)c->r[2];
    c->lo = (uint32_t)_p;
    c->hi = (uint32_t)((uint64_t)_p >> 32);
  }
  c->r[2] = c->r[6] << 16;
  c->r[2] = (uint32_t)((int32_t)c->r[2] >> 16);
  c->r[9] = c->lo;
  {
    int _t = ((int32_t)c->r[2] < 0);
    c->r[5] = (uint32_t)((int32_t)c->r[9] >> 16);
    if (_t) {
      goto L_80094574;
    }
  }
  c->r[5] = c->r[0] + 16383u;
  goto L_8009458C;
L_80094574:
  c->r[4] = c->r[0] - c->r[2];
  c->r[3] = c->r[4] + (uint32_t)-1;
  c->r[2] = c->r[0] + 1u;
  c->r[2] = c->r[2] << (c->r[3] & 31);
  c->r[5] = c->r[5] + c->r[2];
  c->r[5] = c->r[5] >> (c->r[4] & 31);
L_8009458C:
  c->r[2] = c->r[5] & 65535u;
}

// The dispatcher owns the C entry point; this file owns what it runs.
static void nat_channelStreamAccumulate(Core *c) {
  eng(c).sequencer.channelStreamAccumulate();
}

// The dispatcher owns the C entry point; this file owns what it runs.
static void nat_channelToneRecordCopy(Core *c) {
  eng(c).sequencer.channelToneRecordCopy();
}

// The dispatcher owns the C entry point; this file owns what it runs.
static void nat_channelToneRecordCopyWide(Core *c) {
  eng(c).sequencer.channelToneRecordCopyWide();
}

// The dispatcher owns the C entry point; this file owns what it runs.
static void nat_channelNotePeriodCompute(Core *c) {
  eng(c).sequencer.channelNotePeriodCompute();
}

void declareToneRecordOverrides() {
  tomba::native::declareOverride(0x80090160u, "nat_channelStreamAccumulate", nat_channelStreamAccumulate);
  tomba::native::declareOverride(0x80092310u, "nat_channelToneRecordCopy", nat_channelToneRecordCopy);
  tomba::native::declareOverride(0x80092420u, "nat_channelToneRecordCopyWide", nat_channelToneRecordCopyWide);
  tomba::native::declareOverride(0x80094474u, "nat_channelNotePeriodCompute", nat_channelNotePeriodCompute);
}

} // namespace tomba::audio
