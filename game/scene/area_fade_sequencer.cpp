// class AreaFadeSequencer — implementation. See area_fade_sequencer.h.
#include "area_fade_sequencer.h"
#include "core.h"
#include "core/engine/engine.h"
#include "core/entry/game_ctx.h"
#include "guest_call.h"
#include "render/screen_fade.h"

// 0x8010CC68 (returns ready-to-advance in v0) and 0x8010D030 (node init) stay guest.
void AreaFadeSequencer::step(uint32_t node) {
  Core *c = core;
  const uint8_t outer = c->mem_r8(node + 2);

  if (outer == 0) {
    // Init step: three prep calls + arm the state to run its first ramp on the next tick.
    eng(c).modeStateArm.arm();                    // native — was typed runtime address dispatch 0x8005082C(0,0,0)
    eng(c).audioDispatch.zoneTransitionSetup(11); // FUN_8001D71C(11) — native
    c->mem_w8(0x800BFA55u, 0);
    c->mem_w8(node + 3, 0);
    c->mem_w8(node + 2, (uint8_t)(outer + 1)); // -> outer state 1
    c->r[4] = node;
    psx::cpu::dispatchGuestToReturn0(*c,
                                     0x8010D030u,
                                     psx::cpu::ExecutionBudget::currentTurn(*c),
                                     __func__); // overlay guest 0x8010D030(node) — not yet decoded
    c->mem_w16(node + 106, 31);
    fade(c).applyLeafCall(0x00FFFFFFu, ScreenFade::kSubtractive, 0);
    return;
  }

  if (outer != 1) {
    return; // any other outer value: permanent no-op
  }

  const uint8_t substep = c->mem_r8(node + 3);
  if (substep >= 6) {
    return; // bounds check — once step reaches 6 this is inert
  }

  auto rampLevel = [&](int32_t sign) -> uint32_t {
    // v = (level << 3) [negated if sign<0] & 0xFF, replicated into R/G/B.
    const int16_t level = c->mem_r16s(node + 106);
    const uint32_t v = (uint32_t)((sign < 0) ? -(level << 3) : (level << 3)) & 0xFFu;
    return (v << 16) | (v << 8) | v;
  };
  auto decrementLevelClamped = [&]() {
    const int16_t level = c->mem_r16s(node + 106);
    if (level != 0) {
      c->mem_w16(node + 106, (uint16_t)(level - 1));
    }
  };
  auto advanceStep = [&]() {
    c->mem_w8(node + 3, (uint8_t)(c->mem_r8(node + 3) + 1));
  };
  auto helperCC68 = [&](uint32_t arg) {
    c->r[4] = arg;
    psx::cpu::dispatchGuestToReturn0(*c,
                                     0x8010CC68u,
                                     psx::cpu::ExecutionBudget::currentTurn(*c),
                                     __func__); // overlay guest 0x8010CC68 — returns bool in v0
  };

  switch (substep) {
  case 0: { // ramp UP, gated by helper return value
    fade(c).applyLeafCall(rampLevel(+1), ScreenFade::kSubtractive, 1);
    decrementLevelClamped();
    helperCC68(0);
    if (c->r[2] == 0) {
      return; // not done yet
    }
    c->mem_w16(node + 106, 31);
    advanceStep();
    return;
  }
  case 1: { // ramp DOWN, gated by fade LEVEL reaching 0
    fade(c).applyLeafCall(rampLevel(-1), ScreenFade::kSubtractive, 1);
    decrementLevelClamped();
    helperCC68(0); // result unused this branch
    if (c->mem_r16s(node + 106) != 0) {
      return;
    }
    advanceStep();
    c->mem_w16(node + 104, 20); // arm the step-2 delay counter
    return;
  }
  case 2: { // plain ~20-tick delay, then reset level + advance
    const uint16_t d = (uint16_t)(c->mem_r16(node + 104) - 1);
    c->mem_w16(node + 104, d);
    if (d != 0) {
      return;
    }
    c->mem_w16(node + 106, 31);
    advanceStep();
    return;
  }
  case 3: { // same as case 0 but helper called with a0=1
    fade(c).applyLeafCall(rampLevel(+1), ScreenFade::kSubtractive, 1);
    decrementLevelClamped();
    helperCC68(1);
    if (c->r[2] == 0) {
      return;
    }
    c->mem_w16(node + 106, 31);
    advanceStep();
    return;
  }
  case 4: { // same as case 1 but helper called with a0=1;
            // on completion does NOT reset the level to 31
    fade(c).applyLeafCall(rampLevel(-1), ScreenFade::kSubtractive, 1);
    decrementLevelClamped();
    helperCC68(1);
    if (c->mem_r16s(node + 106) != 0) {
      return;
    }
    advanceStep();
    return;
  }
  case 5: { // completion tail: poke the shared sm struct + the
            // 0x800BF839/0x800BF83A control globals, advance
    const uint32_t sm = c->mem_r32(0x1F800138u);
    c->mem_w16(sm + 74, 1);
    c->mem_w16(sm + 76, 2);
    c->mem_w16(sm + 78, 6);
    c->mem_w8(0x800BF839u, 3);
    c->mem_w16(0x800BF83Au, 0x1501);
    advanceStep();
    return;
  }
  }
}
