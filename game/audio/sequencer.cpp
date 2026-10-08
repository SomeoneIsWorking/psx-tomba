// game/audio/sequencer.cpp — Sequencer::frameTick, the libsnd per-VBlank tick
// wrapper, and Sequencer::seqChannelDispatch, the sequence/channel scheduler it calls. Both are
// reverse-engineered guest bodies; see docs/engine_re.md.

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

// 0x800909C0 — the per-VBlank tick wrapper. It descends sp by 24 and spills s0/ra before either
// dispatch, and sets ra to the real return-site constant before each call (0x800909EC /
// 0x800909FC); nested calls therefore address the guest stack 24 bytes lower than they would
// without the frame.
void Sequencer::frameTick() {
  Core *c = core;
  static constexpr GuestFrameSpill kSpills[] = {{16, 16}, {31, 20}}; // -24 (abi_extract-verified)
  GuestFrame<24, 2> frame(c, kSpills);
  // FIX (re-verify pass, same class as channelNoteInit's r16 bug): gen loads r16 = 0x800AC430 (the
  // libsnd cb-slot base it reads +0/-4 from) and keeps it LIVE across both dispatches -- callees
  // that spill r16 must see that value, not the caller's stale one.
  GuestReg<16> r16(c);
  r16 = libsnd::kUserCallback;
  uint32_t cb = c->mem_r32(libsnd::kUserCallback);
  if (cb != 0u) {
    tomba::guest::dispatchJalToReturn(*c, cb, 0x800909ECu);
  }
  uint32_t seq = c->mem_r32(libsnd::kSeqTickFn);
  tomba::guest::dispatchJalToReturn(*c, seq, 0x800909FCu);
}

// 0x80090BD0 — the per-VBlank sequence/channel scheduler. A reentrancy-guarded double loop over
// sequences x channels, testing each channel record's flags field (+152) and dispatching the
// selected leaf.
//
// The register-literal form is the contract: the loop state stays live in the guest's
// callee-save registers, which every leaf spills into its own frame; and bits 0x10/0x20/0x40/
// 0x80 are tested ONLY when bit 0x01 was set, because the bit0-clear branch jumps past them.
void Sequencer::seqChannelDispatch() {
  Core *c = core;
  // Frame descent + spills are UNCONDITIONAL in the gen prologue (they execute before the
  // reentrancy-guard test), so GuestFrame's ctor runs before the guard check below, exactly
  // matching gen — including on the early-exit path (CLAUDE.md "mirror the guest stack" applies
  // even to the do-nothing path). Internal control flow/register usage kept LITERAL below (see
  // this function's own header comment: a prior restructure introduced two real bugs).
  static constexpr GuestFrameSpill kSpills[] = {
      {31, 52}, {30, 48}, {23, 44}, {22, 40}, {21, 36}, {20, 32}, {19, 28}, {18, 24}, {17, 20}, {16, 16}};
  GuestFrame<56, 10> frame(c, kSpills);
  c->r[2] = c->mem_r32(libsnd::kSeqReentryFlag);
  c->r[3] = 1u;
  if (c->r[2] == c->r[3]) {
    goto L_80090E10;
  }
  c->mem_w32(libsnd::kSeqReentryFlag, c->r[3]);
  c->r[31] = 0x80090C1Cu;
  c->r[23] = 0u;
  psx::cpu::dispatchGuestToReturn0(*c,
                                   libsnd::kSeqPrepFn,
                                   psx::cpu::ExecutionBudget::currentTurn(*c),
                                   __func__); // 0x800931C0 input_dispatch_931c0 — still-unwired by us
  c->r[2] = (uint32_t)c->mem_r16s(libsnd::kSeqCount);
  {
    int _t = ((int32_t)c->r[2] <= 0);
    if (_t) {
      goto L_80090E08;
    }
  }
  c->r[30] = libsnd::kSeqPtrArray;
L_80090C38:
  c->r[2] = 1u;
  c->r[3] = c->mem_r32(libsnd::kSeqActiveMask);
  c->r[2] = c->r[2] << (c->r[23] & 31u);
  c->r[3] = c->r[3] & c->r[2];
  {
    int _t = (c->r[3] == 0u);
    if (_t) {
      goto L_80090DF0;
    }
  }
  c->r[2] = (uint32_t)c->mem_r16s(libsnd::kSeqChanCount);
  {
    int _t = ((int32_t)c->r[2] <= 0);
    c->r[22] = 0u;
    if (_t) {
      goto L_80090DF0;
    }
  }
  c->r[18] = c->r[30];
  c->r[21] = c->r[23] << 16;
  c->r[20] = (uint32_t)((int32_t)c->r[21] >> 16);
  c->r[19] = 0u;
  c->r[16] = 0u;
L_80090C7C:
  c->r[2] = c->mem_r32(c->r[18] + 0u);
  c->r[2] = c->r[16] + c->r[2];
  c->r[2] = c->mem_r32(c->r[2] + 152u);
  c->r[2] = c->r[2] & 1u;
  {
    int _t = (c->r[2] == 0u);
    c->r[4] = c->r[20];
    if (_t) {
      goto L_80090D48;
    }
  }
  c->r[17] = (uint32_t)((int32_t)c->r[19] >> 16);
  c->r[31] = 0x80090CA8u;
  c->r[5] = c->r[17];
  channelPitchSelectDispatch();
  c->r[2] = c->mem_r32(c->r[18] + 0u);
  c->r[2] = c->r[16] + c->r[2];
  c->r[2] = c->mem_r32(c->r[2] + 152u);
  c->r[2] = c->r[2] & 16u;
  {
    int _t = (c->r[2] == 0u);
    c->r[4] = c->r[20];
    if (_t) {
      goto L_80090CD0;
    }
  }
  c->r[31] = 0x80090CD0u;
  c->r[5] = c->r[17];
  psx::cpu::dispatchGuestToReturn0(*c,
                                   0x80090E40u,
                                   psx::cpu::ExecutionBudget::currentTurn(*c),
                                   __func__); // pitch-slide: UNWIRED (never fired in any run -- see findings/audio.md)
L_80090CD0:
  c->r[2] = c->mem_r32(c->r[18] + 0u);
  c->r[2] = c->r[16] + c->r[2];
  c->r[2] = c->mem_r32(c->r[2] + 152u);
  c->r[2] = c->r[2] & 32u;
  {
    int _t = (c->r[2] == 0u);
    c->r[4] = c->r[20];
    if (_t) {
      goto L_80090CF8;
    }
  }
  c->r[31] = 0x80090CF8u;
  c->r[5] = c->r[17];
  psx::cpu::dispatchGuestToReturn0(*c,
                                   0x80090E40u,
                                   psx::cpu::ExecutionBudget::currentTurn(*c),
                                   __func__); // pitch-slide: UNWIRED (never fired in any run)
L_80090CF8:
  c->r[2] = c->mem_r32(c->r[18] + 0u);
  c->r[2] = c->r[16] + c->r[2];
  c->r[2] = c->mem_r32(c->r[2] + 152u);
  c->r[2] = c->r[2] & 64u;
  {
    int _t = (c->r[2] == 0u);
    c->r[4] = c->r[20];
    if (_t) {
      goto L_80090D20;
    }
  }
  c->r[31] = 0x80090D20u;
  c->r[5] = c->r[17];
  psx::cpu::dispatchGuestToReturn0(*c,
                                   0x80092080u,
                                   psx::cpu::ExecutionBudget::currentTurn(*c),
                                   __func__); // envelope ramp: UNWIRED (never fired in any run)
L_80090D20:
  c->r[2] = c->mem_r32(c->r[18] + 0u);
  c->r[2] = c->r[16] + c->r[2];
  c->r[2] = c->mem_r32(c->r[2] + 152u);
  c->r[2] = c->r[2] & 128u;
  {
    int _t = (c->r[2] == 0u);
    c->r[4] = c->r[20];
    if (_t) {
      goto L_80090D48;
    }
  }
  c->r[31] = 0x80090D48u;
  c->r[5] = c->r[17];
  psx::cpu::dispatchGuestToReturn0(*c,
                                   0x80092080u,
                                   psx::cpu::ExecutionBudget::currentTurn(*c),
                                   __func__); // envelope ramp: UNWIRED (never fired in any run)
L_80090D48:
  c->r[2] = c->mem_r32(c->r[18] + 0u);
  c->r[2] = c->r[16] + c->r[2];
  c->r[2] = c->mem_r32(c->r[2] + 152u);
  c->r[2] = c->r[2] & 2u;
  {
    int _t = (c->r[2] == 0u);
    c->r[4] = (uint32_t)((int32_t)c->r[21] >> 16);
    if (_t) {
      goto L_80090D70;
    }
  }
  c->r[31] = 0x80090D70u;
  c->r[5] = (uint32_t)((int32_t)c->r[19] >> 16);
  psx::cpu::dispatchGuestToReturn0(*c,
                                   0x80091050u,
                                   psx::cpu::ExecutionBudget::currentTurn(*c),
                                   __func__); // release-clear: UNWIRED (never fired in any run)
L_80090D70:
  c->r[2] = c->mem_r32(c->r[18] + 0u);
  c->r[2] = c->r[16] + c->r[2];
  c->r[2] = c->mem_r32(c->r[2] + 152u);
  c->r[2] = c->r[2] & 8u;
  {
    int _t = (c->r[2] == 0u);
    c->r[4] = (uint32_t)((int32_t)c->r[21] >> 16);
    if (_t) {
      goto L_80090D98;
    }
  }
  c->r[31] = 0x80090D98u;
  c->r[5] = (uint32_t)((int32_t)c->r[19] >> 16);
  psx::cpu::dispatchGuestToReturn0(*c,
                                   0x80091910u,
                                   psx::cpu::ExecutionBudget::currentTurn(*c),
                                   __func__); // stop-flag: UNWIRED (never fired in any run)
L_80090D98:
  c->r[2] = c->mem_r32(c->r[18] + 0u);
  c->r[2] = c->r[16] + c->r[2];
  c->r[2] = c->mem_r32(c->r[2] + 152u);
  c->r[2] = c->r[2] & 4u;
  {
    int _t = (c->r[2] == 0u);
    c->r[4] = (uint32_t)((int32_t)c->r[21] >> 16);
    if (_t) {
      goto L_80090DD0;
    }
  }
  c->r[31] = 0x80090DC0u;
  c->r[5] = (uint32_t)((int32_t)c->r[19] >> 16);
  channelNoteInit();
  c->r[2] = c->mem_r32(c->r[18] + 0u);
  c->r[2] = c->r[16] + c->r[2];
  c->mem_w32(c->r[2] + 152u, 0u); // SsSeqCalled's OWN post-call full flags clear
L_80090DD0:
  c->r[2] = 1u << 16;
  c->r[19] = c->r[19] + c->r[2];
  c->r[2] = (uint32_t)c->mem_r16s(libsnd::kSeqChanCount);
  c->r[22] = c->r[22] + 1u;
  c->r[2] = (uint32_t)((int32_t)c->r[22] < (int32_t)c->r[2]);
  {
    int _t = (c->r[2] != 0u);
    c->r[16] = c->r[16] + 176u;
    if (_t) {
      goto L_80090C7C;
    }
  }
L_80090DF0:
  c->r[2] = (uint32_t)c->mem_r16s(libsnd::kSeqCount);
  c->r[23] = c->r[23] + 1u;
  c->r[2] = (uint32_t)((int32_t)c->r[23] < (int32_t)c->r[2]);
  {
    int _t = (c->r[2] != 0u);
    c->r[30] = c->r[30] + 4u;
    if (_t) {
      goto L_80090C38;
    }
  }
L_80090E08:
  c->mem_w32(libsnd::kSeqReentryFlag, 0u);
L_80090E10:; // GuestFrame's destructor restores r16..r23/r30/r31 + ascends sp here, both exit paths.
}

// ============================================================================
// 2026-07-10 wide-RE wave — the remaining SsSeqCalled leaves (bit4/5, bit6/7, bit2) + their own
// small callees. See sequencer.h header for the summary / confidence table. All UNWIRED.
// ============================================================================

// The dispatcher owns the C entry point; this file owns what it runs.
static void nat_frameTick(Core *c) {
  eng(c).sequencer.frameTick();
}

// The dispatcher owns the C entry point; this file owns what it runs.
static void nat_seqChannelDispatch(Core *c) {
  eng(c).sequencer.seqChannelDispatch();
}

void declareDispatchOverrides() {
  tomba::native::declareOverride(0x800909C0u, "nat_frameTick", nat_frameTick);
  tomba::native::declareOverride(0x80090BD0u, "nat_seqChannelDispatch", nat_seqChannelDispatch);
}

void Sequencer::registerOverrides() {
  declareDispatchOverrides();
  declareChannelFlagOverrides();
  declareVoiceWriteOverrides();
  declareToneRecordOverrides();
  declareVoiceAllocOverrides();
  declareVoiceStateOverrides();
}

} // namespace tomba::audio
