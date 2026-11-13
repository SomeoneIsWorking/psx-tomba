// game/core/engine_state_dispatch.cpp — the frame transaction's own dispatch: the one frame() step, the stage
// prologue/body/main trio, and the area, scene-state and per-frame mode dispatchers.
//
// Split out of game/core/engine.cpp on 2026-09-28. `Engine` is one class and stays one class: this
// is a change of TRANSLATION UNIT, not of ownership, and nothing here moved to a different class or
// to a different subsystem. The split is by the level of the guest's own three-level state machine
// the bodies serve, so a reader looking for "what runs in the running sub-mode" has one file to open.
//
// The class's own header (game/core/engine.h) remains the single declaration of every method below;
// nothing is redeclared here.
#include "camera/cutscene_camera.h" // class CutsceneCamera — the field/follow camera
#include "core.h"
#include "core/assets/asset.h" // class Asset
#include "core/engine/engine.h"
#include "core/engine/task_sm.h"         // TaskSm — the typed lens over the task record, owned in one header
#include "core/overrides/guest_jal.h"    // tomba::guest::dispatchJalToReturn — the guest call convention
#include "entry/session.h"
#include "game.h" // Game — the per-Core product the Engine drives
#include "guest_call.h"
#include "level_load.h"
#include "math/rng.h" // class Rng
#include "overrides/native_override_catalog.h"
#include "placement.h" // ov_placeObjects
#include "pool.h"      // ov_poolInitRun
#include "render.h"
#include "scene/card_load_machine.h" // tomba::scene::stepCardLoadMachine
#include "scene/start_bin_stage.h"   // class StartBinStage — the task-0 file-table builder
#include <cstdint>
#include <cstdio>
#include <cstdlib>

// One native loop iteration of the guest body 0x801063F4: dispatch the sm[0x48] handler, bump the frame counter.
namespace tomba {

void Engine::frame() {
  Core *c = core;
  uint32_t sm = c->mem_r32(0x1f800138u);
  uint16_t s48 = c->mem_r16(sm + 0x48);
  if (s48 == 2) {
    // Sub-mode 0 runs the SOP overlay's field; the guest enters it only with SOP in the MODE slot.
    if (c->mem_r16(sm + 0x4a) == 0 && c->mem_r32(0x80109450u) != 0x3C021F80u) {
      lucent::error("gframe", "sub-mode 0 without SOP loaded (0x80109450 = {:08X})", c->mem_r32(0x80109450u));
      std::abort();
    }
    c->r[31] = 0x8010645Cu; // guest loop jal site (L_80106454)
    eng(c).stageRunning();
  } else if (s48 == 0) {
    c->r[31] = 0x8010643Cu; // guest loop jal site (L_80106434)
    eng(c).stageAreaInit();
  } else if (s48 == 1) {
    c->r[31] = 0x8010644Cu; // guest loop jal site (L_80106444)
    eng(c).stageResumeInit();
  } else {
    // The guest loop runs no handler for any other state and spins without one.
    lucent::error("gframe", "GAME top state sm[0x48] = {} has no handler", s48);
    std::abort();
  }
  c->mem_w16(0x1f800198u,
             (uint16_t)(c->mem_r16(0x1f800198u) + 1)); // loop tail 0x8010645c
}

// GAME stage entry 0x8010637C's prologue: the registers the loop body reads (s0=s1=0x1F800000, s2=1), the
// 0x20 frame with the incoming s0..s2 and ra, and the stage machine's reset. The loop itself is Engine::frame.
void Engine::stagePrologue() {
  Core *c = core;
  uint32_t ra = c->r[31], sp = c->r[29];
  uint32_t s0_in = c->r[16], s1_in = c->r[17], s2_in = c->r[18];
  c->r[29] = sp - 0x20;
  c->mem_w32(c->r[29] + 0x14,
             s1_in);                  // sw s1,0x14(sp)  — save INCOMING callee-saved regs
  c->mem_w32(c->r[29] + 0x18, s2_in); // sw s2,0x18(sp)
  c->mem_w32(c->r[29] + 0x10, s0_in); // sw s0,0x10(sp)
  c->r[16] = 0x1f800000u;             // s0 = 0x1f800000  (loop reads 0x198(s0))
  c->r[17] = 0x1f800000u;             // s1 = 0x1f800000  (loop reads 0x138(s1))
  c->r[18] = 1;                       // s2 = 1           (loop compares sm[0x48]==s2)
  c->mem_w8(0x1f800206u, 0);
  c->mem_w8(0x1f800236u, 0);
  c->mem_w8(0x1f800234u, 0);
  c->mem_w8(0x1f80019Au, 2);
  uint32_t task = c->mem_r32(0x1f800138u);
  uint8_t init48 = c->mem_r8(0x1f800134u); // initial sm[0x48] = boot/resume selector
  c->mem_w32(c->r[29] + 0x1c, ra);         // sw ra,0x1c(sp)
  c->mem_w16(0x1f800198u, 0);              // sh zero,0x198(s0) — frame counter reset
  c->mem_w8(0x800be0e4u, 0);
  c->mem_w16(task + 0x48, init48); // sm[0x48] = mem_r8(0x1f800134)
  c->mem_w16(task + 0x4a, 0);
  c->mem_w16(task + 0x4c, 0);
  c->mem_w16(task + 0x4e, 0);
  c->mem_w16(task + 0x50, 0);
  lucent::debug("stage", "ov_game_stage_prologue run, sm[0x48]={}", init48);
}

void Engine::areaModeDispatch() {
  Core *c = core;
  uint8_t idx = c->mem_r8(0x800BF870u);
  if (idx >= 22) {
    return;
  }
  constexpr uint32_t handlers[22] = {
      /* 0*/ 0x8011534Cu, /* 1*/ 0,
      /* 2*/ 0,           /* 3*/ 0,
      /* 4*/ 0x8013EE84u, /* 5*/ 0x80136CDCu,
      /* 6*/ 0x8014189Cu, /* 7*/ 0x8012F6ECu,
      /* 8*/ 0,           /* 9*/ 0,
      /*10*/ 0x801140D0u, /*11*/ 0x80113F94u,
      /*12*/ 0,           /*13*/ 0x80116980u,
      /*14*/ 0,           /*15*/ 0x80116560u,
      /*16*/ 0,           /*17*/ 0,
      /*18*/ 0,           /*19*/ 0,
      /*20*/ 0,           /*21*/ 0x8010B918u,
  };
  uint32_t target = handlers[idx];
  if (!target) {
    return;
  }
  c->r[4] = 0x800ED018u;
  psx::cpu::callGuestNow(*c, __func__, target);
}

// Engine::sceneStateStep — the SCENE-INIT / SCENE-RUN state machine at guest
// 0x80050DE4. See engine.h.
void Engine::sceneStateStep() {
  Core *c = core;
  constexpr uint32_t SCENE_STATE = 0x800F2418u;
  int8_t phase = (int8_t)c->mem_r8(SCENE_STATE);

  if (phase == 1) {
    // RUN table (@0x80015A98). Idx 9 = default 0x80051118 = no-op return.
    uint8_t idx = c->mem_r8(0x800BF870u);
    if (idx >= 22) {
      return;
    }
    constexpr uint32_t run[22] = {
        /* 0*/ 0x8013EFA8u, /* 1*/ 0x8012EE14u,
        /* 2*/ 0x80123E1Cu, /* 3*/ 0x8010E964u,
        /* 4*/ 0x80116CCCu, /* 5*/ 0x80136488u,
        /* 6*/ 0x8013D6D8u, /* 7*/ 0x8012E2F4u,
        /* 8*/ 0x8012AB30u, /* 9*/ 0,
        /*10*/ 0x80110A14u, /*11*/ 0x80113770u,
        /*12*/ 0x801144D4u, /*13*/ 0x80113D68u,
        /*14*/ 0x80114A78u, /*15*/ 0x80115DECu,
        /*16*/ 0x8010C9FCu, /*17*/ 0x8010BCDCu,
        /*18*/ 0x8010C160u, /*19*/ 0x8010B140u,
        /*20*/ 0x80116B9Cu, /*21*/ 0x8010B200u,
    };
    uint32_t target = run[idx];
    if (!target) {
      return;
    }
    c->r[4] = SCENE_STATE;
    psx::cpu::callGuestNow(*c, __func__, target);
    return;
  }
  if (phase != 0) {
    return; // < 0 or >= 2 -> no-op (signed slti 2 + neq 0)
  }

  // INIT table (@0x80015A40). Idx 9 = default 0x80050F90 (no-op body, then set
  // phase=1).
  uint8_t idx = c->mem_r8(0x800BF870u);
  if (idx >= 22) {
    return;
  }
  constexpr uint32_t init[22] = {
      /* 0*/ 0x8013FB4Cu, /* 1*/ 0x8012F89Cu,
      /* 2*/ 0x80124678u, /* 3*/ 0x8010F174u,
      /* 4*/ 0x801175D0u, /* 5*/ 0x80136CB0u,
      /* 6*/ 0x8013E144u, /* 7*/ 0x8012EB50u,
      /* 8*/ 0x8012B3E8u, /* 9*/ 0,
      /*10*/ 0x80111238u, /*11*/ 0x80113F68u,
      /*12*/ 0x80114CCCu, /*13*/ 0x80114560u,
      /*14*/ 0x80115270u, /*15*/ 0x80116534u,
      /*16*/ 0x8010D21Cu, /*17*/ 0x8010C4FCu,
      /*18*/ 0x8010C980u, /*19*/ 0x8010B960u,
      /*20*/ 0x801173A8u, /*21*/ 0x8010B8ECu,
  };
  uint32_t target = init[idx];
  if (target) {
    c->r[4] = SCENE_STATE;
    psx::cpu::callGuestNow(*c, __func__, target);
  }
  c->mem_w8(SCENE_STATE,
            1); // advance to RUN (all L_80050F94 paths, incl. the default at idx 9)
}

} // namespace tomba

// Engine::modePerFrameDispatch — the mode-keyed per-frame overlay handler at
// guest 0x80022A80. Faithful to the disasm: skip mode 3 (A00 village)
// explicitly, then read the fn-pointer table at 0x8009D1D4 (MAIN.EXE .rodata,
// indexed by 0x800BF870) and dispatch the current overlay's entry. NO bounds
// check in the guest — the render-mode byte is bounded by the writer sites, not
// here. No a0 setup (the guest jalr inherits whatever a0 the caller had;
// ov_field_frame doesn't touch a0 before the call, so handlers that read a0 are
// dead code in this path — none observed).
#include "ai/behaviors.h" // Behaviors::areaSeasidePerframe (FUN_80113C5C)
#include <lucent/log.h>

namespace tomba {

void Engine::modePerFrameDispatch() {
  Core *c = core;
  uint8_t idx = c->mem_r8(0x800BF870u);
  if (idx == 3) {
    return;
  }
  uint32_t target = c->mem_r32(0x8009D1D4u + (uint32_t)idx * 4u);
  if (!target) {
    return;
  }
  // Area-0 (seaside) per-frame update is native — this is Tomba's seaside
  // per-frame tick.
  if (target == 0x80113C5Cu) {
    Behaviors::areaSeasidePerframe(c);
    return;
  }
  psx::cpu::callGuestNow(*c, __func__, target);
}

} // namespace tomba

// Engine::postRenderTick — 3-state fx-trigger + countdown on byte 0x800BF842 at
// guest 0x80077D8C. Faithful to the disasm: low 7 bits select (== 1: fire FX
// 41, set b42 = 0x87), (== 2: fire FX 42, clear b42), other/nonzero: decrement
// b42. Zero = no-op. FX 41/42 leaf FUN_80074590 stays substrate.
