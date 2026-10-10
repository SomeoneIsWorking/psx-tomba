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
#include "c_subsys.h"               // disc_findFile — the native ISO9660 resolver
#include "camera/cutscene_camera.h" // class CutsceneCamera — the field/follow camera
#include "core.h"
#include "core/assets/asset.h" // class Asset
#include "core/engine/engine.h"
#include "core/engine/task_sm.h"         // TaskSm — the typed lens over the task record, owned in one header
#include "core/overrides/guest_jal.h"    // tomba::guest::dispatchJalToReturn — the guest call convention
#include "core/overrides/guest_resume.h" // requestGuestContinuation
#include "entry/game_ctx.h"
#include "game.h" // Game — the per-Core product the Engine drives
#include "guest_abi.h"
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

// One native loop iteration of the guest body 0x801063F4: dispatch sm[0x48]
// handler, bump frame counter. Returns 1 if handled natively, 0 if the current
// state is NOT yet owned and the task must hand back to the cooperative guest
// loop. OWNED so far: sm[0x48] area-init (0/1) + the RUNNING SOP-intro path
// (sm[0x48]==2, sm[0x4a]==0, SOP loaded). The transition sub-modes
// (sm[0x4a]!=0), the area machine (sm[0x4c] 0x80106478), and the non-SOP field
// overlays YIELD DEEP and aren't owned yet — own them (RE in
// scratch/gameplay_start_flow_re.md) to extend native ownership and shrink the
// cooperative fallback. Returning 0 keeps the field REACHABLE (no derail) until
// those are owned.
int Engine::frame() {
  Core *c = core;
  uint32_t sm = c->mem_r32(0x1f800138u);
  uint16_t s48 = c->mem_r16(sm + 0x48);
  if (s48 == 2) {
    uint16_t s4a = c->mem_r16(sm + 0x4a);
    // OWNED running sub-modes: 0 = SOP-intro (SOP overlay must be loaded); 1 =
    // field area machine (0x801088d8, the walkable field — its load is sync via
    // native_transition_area_load, its running states are yield-free); 5 = the
    // area-change transition (FieldTransition::step). Sub-modes 2..4 aren't owned yet.
    if (s4a == 0) {
      if (c->mem_r32(0x80109450u) != 0x3C021F80u) {
        cfg_logf("gframe", "ret0 s48=2 s4a=0 SOP-not-loaded ov=%08X sm@%08X", c->mem_r32(0x80109450u), sm);
        return 0;
      } // SOP not loaded -> cooperative
    } else if (s4a != 1 && s4a != 5) {
      cfg_logf("gframe", "ret0 s48=2 s4a=%u unowned-submode sm@%08X", s4a, sm);
      return 0; // unowned running sub-mode
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
    cfg_logf("gframe", "ret0 unknown s48=%u sm@%08X", s48, sm);
    return 0; // unknown top state -> cooperative
  }
  c->mem_w16(0x1f800198u,
             (uint16_t)(c->mem_r16(0x1f800198u) + 1)); // loop tail 0x8010645c
  return 1;
}

// GAME stage TOP-LEVEL ENTRY 0x8010637C — task-0's stage driver: a one-time
// PROLOGUE then an infinite per-frame loop {dispatch sm[0x48] handler; bump
// frame counter DAT_1f800198; yield FUN_80051f80(1)}. The engine OWNS the
// prologue PC-native here: it is pure register/memory init of the stage state
// machine (no yield, no jal), so it is safe to reimplement and hand to the
// guest loop body IN-CONTEXT via the coro- redirect handshake — exactly like
// the sub-handlers. This is the entry point for owning the loop itself; the
// loop body (0x801063F4) + its handler dispatch (all 3 handlers already native)
// + the FUN_80051f80 yield still run via the resumed task-0 coroutine, so deep
// yields/resumes are unchanged. Registered AUTO (it lives in GAME.BIN, flushed
// on overlay unload). Faithful to 0x8010637C prologue (regs the loop body reads
// MUST be set: s0=s1=0x1f800000, s2=1, sp=sp-0x20, ra saved at 0x1c(sp)):
//   sp-=0x20; sw s1,0x14; s1=0x1f800000; sw s2,0x18; s2=1; sw s0,0x10;
//   s0=0x1f800000; (saves INCOMING regs) 0x1f800206=0; 0x1f800236=0;
//   0x1f800234=0; 0x1f80019a=2; v1=mem[0x1f800138]; a0=mem_r8(0x1f800134); sw
//   ra,0x1c; 0x1f800198=0; 0x800be0e4=0; sm[0x48]=a0;
//   sm[0x4a]=sm[0x4c]=sm[0x4e]=sm[0x50]=0.
// Non-static: called directly from the native scheduler (native_boot.cpp) — the
// ONE remaining native task entry after the override system was removed
// (2026-06-22).
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
  cfg_logf("stage", "ov_game_stage_prologue run, sm[0x48]=%u", init48);
}

// pc_faithful GAME stage body (fiber task; see engine.h). Byte shape:
// overlay guest 0x8010637C + overlay guest 0x801063F4. stagePrologue leaves the guest
// frame descended and s0/s1/s2 holding the loop constants, exactly like the
// substrate (the frame stays live for the whole stage). frame() bumps the
// 0x1F800198 counter itself when it handles the state; the unowned-state
// fallback dispatches the guest sm[0x48] handler (deep yields park the fiber)
// and bumps the counter here.
void Engine::stageBodyFaithful() {
  Core *c = core;
  stagePrologue();
  for (;;) {
    int handled = frame();
    if (!handled) {
      uint32_t sm = c->mem_r32(0x1f800138u);
      uint16_t s48 = c->mem_r16(sm + 0x48);
      if (s48 == 1) {
        c->r[31] = 0x8010644Cu;
        psx::cpu::dispatchGuestToReturn0(*c, 0x80108720u, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
      } else if (s48 == 0) {
        c->r[31] = 0x8010643Cu;
        psx::cpu::dispatchGuestToReturn0(*c, 0x801086E0u, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
      } else if (s48 == 2) {
        c->r[31] = 0x8010645Cu;
        psx::cpu::dispatchGuestToReturn0(*c, 0x80108784u, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
      }
      c->mem_w16(0x1f800198u, (uint16_t)(c->mem_r16(0x1f800198u) + 1));
    }
    c->r[4] = 1;
    c->r[31] = 0x80106470u;
    psx::cpu::dispatchGuestToReturn0(*c,
                                     0x80051F80u,
                                     psx::cpu::ExecutionBudget::currentTurn(*c),
                                     __func__); // loop-tail yield (override registry -> yieldPrim)
  }
}

// OLD guest-loop entry (prologue + guest-continuation into the guest loop
// 0x801063F4). SUPERSEDED by the native per-frame path (game_native in
// PcScheduler::step calls eng(c).stagePrologue + eng(c).frame). Retained as a
// reference / fallback; not on the live path.
void Engine::stageMain() {
  Core *c = core;
  stagePrologue();
  tomba::requestGuestContinuation(*c, 0x801063F4u);
}

void Engine::areaModeDispatch() {
  Core *c = core;
  uint8_t idx = c->mem_r8(0x800BF870u);
  if (idx >= 22) {
    return;
  }
  static const uint32_t handlers[22] = {
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
  psx::cpu::dispatchGuestToReturn0(*c, target, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
}

// Engine::sceneStateStep — the SCENE-INIT / SCENE-RUN state machine at guest
// 0x80050DE4. See engine.h.
void Engine::sceneStateStep() {
  Core *c = core;
  static constexpr uint32_t SCENE_STATE = 0x800F2418u;
  int8_t phase = c->mem_r8s(SCENE_STATE);

  if (phase == 1) {
    // RUN table (@0x80015A98). Idx 9 = default 0x80051118 = no-op return.
    uint8_t idx = c->mem_r8(0x800BF870u);
    if (idx >= 22) {
      return;
    }
    static const uint32_t run[22] = {
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
    psx::cpu::dispatchGuestToReturn0(*c, target, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
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
  static const uint32_t init[22] = {
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
    psx::cpu::dispatchGuestToReturn0(*c, target, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
  }
  c->mem_w8(SCENE_STATE,
            1); // advance to RUN (all L_80050F94 paths, incl. the default at idx 9)
}

// Engine::modePerFrameDispatch — the mode-keyed per-frame overlay handler at
// guest 0x80022A80. Faithful to the disasm: skip mode 3 (A00 village)
// explicitly, then read the fn-pointer table at 0x8009D1D4 (MAIN.EXE .rodata,
// indexed by 0x800BF870) and dispatch the current overlay's entry. NO bounds
// check in the guest — the render-mode byte is bounded by the writer sites, not
// here. No a0 setup (the guest jalr inherits whatever a0 the caller had;
// ov_field_frame doesn't touch a0 before the call, so handlers that read a0 are
// dead code in this path — none observed).
#include "ai/behaviors.h" // Behaviors::areaSeasidePerframe (FUN_80113C5C)

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
  psx::cpu::dispatchGuestToReturn0(*c, target, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
}

// Engine::postRenderTick — 3-state fx-trigger + countdown on byte 0x800BF842 at
// guest 0x80077D8C. Faithful to the disasm: low 7 bits select (== 1: fire FX
// 41, set b42 = 0x87), (== 2: fire FX 42, clear b42), other/nonzero: decrement
// b42. Zero = no-op. FX 41/42 leaf FUN_80074590 stays substrate.
