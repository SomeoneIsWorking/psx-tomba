// game/core/engine_task_machine.cpp — the GAME task state machine's TOP level — sm[0x48]. The area-init and resume-init
// pair, the area load/intro/play machine at sm[0x4c], the two resident 0x810C page submits, the running state, its
// sub-mode selector, and the scene-event FIFO that feeds it.
//
// Split out of game/core/engine.cpp on 2026-09-28. `Engine` is one class and stays one class: this
// is a change of TRANSLATION UNIT, not of ownership, and nothing here moved to a different class or
// to a different subsystem. The split is by the level of the guest's own three-level state machine
// the bodies serve, so a reader looking for "what runs in the running sub-mode" has one file to open.
//
// The class's own header (game/core/engine.h) remains the single declaration of every method below;
// nothing is redeclared here.
#include "c_subsys.h" // disc_findFile — the native ISO9660 resolver
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

// sm[0x48] == 0 — area INIT: advance to running (sm[0x48]=2), reset the
// sub-machine state, run the per-area setup fns. (GAME.BIN 0x801086e0) Verified
// runtime-exercised + RAM 0-diff. GUEST FRAME MIRROR (abi_extract --contract,
// single epilogue label -> GuestFrame RAII is safe): sp-24, ra@+16.
void Engine::stageAreaInit() {
  Core *c = core;
  static constexpr GuestFrameSpill kSpills[] = {{31, 16}};
  GuestFrame<24, 1> frame(c, kSpills);
  TaskSm sm(c);
  sm.setTop(2); // sm[0x48] = 2 (running)
  sm.setSubMode(0);
  sm.setStage4c(0);
  sm.setF69(0);
  tomba::guest::dispatchJalToReturn(*c, 0x8007a8e0u, 0x80108708u); // per-area setup (resident system, synchronous)
  tomba::guest::dispatchJalToReturn(*c, 0x8007b38cu, 0x80108710u); // per-area setup (resident system, synchronous)
}

// sm[0x48] == 1 — area RESUME-INIT (re-enter a running area, sub-mode 1): like
// init but sm[0x4a]=1 plus flag resets. (GAME.BIN 0x80108720) Faithful
// transcription of the disasm; the field-intro path used to verify (later-168)
// does not hit sm[0x48]==1, so this handler is not yet runtime-exercised — its
// callee FUN_8007b3f4 is synchronous like the init pair, so it is registered
// alongside ov_game_s48_0. GUEST FRAME MIRROR (abi_extract --contract, single
// epilogue label -> GuestFrame RAII is safe): sp-24, ra@+16.
void Engine::stageResumeInit() {
  Core *c = core;
  static constexpr GuestFrameSpill kSpills[] = {{31, 16}};
  GuestFrame<24, 1> frame(c, kSpills);
  TaskSm sm(c);
  sm.setTop(2); // sm[0x48] = 2 (running)
  sm.setSubMode(1);
  sm.setStage4c(0);
  sm.setF69(0);
  c->mem_w8(0x1f8001ffu, 0xff); // DAT_1f8001ff = 0xff
  c->mem_w16(0x1f800278u,
             0); // DAT_1f800278 = 0 (16-bit; delay-slot before the setup call)
  tomba::guest::dispatchJalToReturn(*c, 0x8007b3f4u, 0x80108760u); // per-area setup (resident system, synchronous)
  c->mem_w8(0x1f800206u, 0);                                       // display flags cleared after setup
  c->mem_w8(0x1f800236u, 0);
  c->mem_w8(0x1f800234u, 0);
}

// (The staged Engine::s48_2 mirror of GAME.BIN 0x80108784 — which
// guest-continuationed each sm[0x4a] sub-handler to the substrate — was never called
// and is deleted. Engine::stageRunning below is the LIVE native dispatcher for
// sm[0x48]==2; it calls the owned sub-mode handlers directly. RE note kept: a
// sub-handler that YIELDS DEEP must not be typed runtime address dispatch'd (that nests a
// test-only reference execution whose CORO_SENTINEL the yield's longjmp destroys, mis-reading the
// return as task-end and killing task 0, later-168) — reach such a handler via
// requestGuestContinuation instead, as stageRunning still does for the sub-modes it
// does not own.)

// The sm[0x4c] AREA machine (9-state load/intro/play scene state machine,
// GAME.BIN, reached as the sm[0x4a]==2 area LOAD/TRANSITION path) is OWNED as
// `Engine::areaLoadState()` and wired on the LIVE path at
// `Engine::stageRunning`'s s4a==2 branch. RE note kept from the retired staging
// body: the guest fn has NO jal to the yield primitive FUN_80051f80 in any of
// its 9 states — it is a plain SYNCHRONOUS pause/save/quit-menu sequencer,
// which is why areaLoadState needs no guest-continuation. (The old staged
// s4c()/state[] guest-continuation mirror was never registered and is deleted — see
// areaLoadState.)

// Engine::areaLoadState — native ownership of FUN_80106478 (the
// RUNNING/sm[0x4a]==2 sub-mode's sm[0x4c] area LOAD/TRANSITION machine;
// stageRunning's handler[2]). Verified SYNCHRONOUS: the decompiled body (Ghidra
// scratch/decomp/game_all_list.c) has no jal to the yield primitive
// FUN_80051f80 anywhere in its 9 states, so it's safe to call as a plain native
// method (unlike FUN_801088d8's FIELD area machine, which genuinely yields and
// stays behind requestGuestContinuation — that is a DIFFERENT sm[0x4c] context, reused
// field, not this one).
//
// WALKABLE-TOMBA SPAWN HUNT — NEGATIVE RESULT: none of states 0-8 spawn
// anything. This machine is the PAUSE/SAVE/QUIT menu's confirm-dialog
// sequencer: state 0 arms a ~330-frame fade-out timer + dispatch3Way(0x2C)
// (audio-armed poll) + a 128-byte zero-init (FUN_8004D8B0, still un-owned);
// state 1 counts the fade-out timer down against pad-edge bit 0x800E7E68,
// driving a camera/view helper (FUN_8007E8DC); state 2 selects state-index 4
// (AudioDispatch::selectState) and advances; states 3-5 are per-frame
// SAVE-prompt / CONTINUE-prompt renders (FUN_8007ED5C/EE74/EF60 — text widgets,
// own the "Save"/"Continue"/"Load data"/"Quit game" strings) gated entirely on
// pad-edge bits (0x4000 confirm, 0x10/0x40/0x2000 cursor/cancel) with SFX cues
// via eng(c).sfx.trigger; state 6 just advances sm[0x4a] to 4 (hands off to the
// next running sub-mode, itself substrate); states 7/8 are the memory-card LOAD /
// SAVE pages (FUN_8007BF20, its own DAT_800bf84a-keyed SM, owned natively by
// tomba::scene::stepCardLoadMachine) — on accept, state 7
// calls reloadEntityPool() (was FUN_8007B3F4) and returns to sm[0x4a]==1 (back
// to the FIELD area machine), i.e. "confirm quit-to-title" unwinds to the
// SOP/field bridge, it does not spawn anything either. Rules out the last
// un-RE'd sibling of the sm[0x4c] area machine as a spawn candidate. NOTE
// (found during this pass, not fixed here — out of scope for a readability-only
// refactor): guest 0x80106478 descends a 24-byte frame (ra@+20, r16@+16,
// abi_extract --contract) that this native port never mirrors. Pre-existing gap
// (predates this refactor); flagged for a follow-up pass under the "MIRROR THE
// GUEST STACK" rule, not addressed here since adding the frame would be a
// behavior change (new guest-stack bytes), not a readability one.
void Engine::areaLoadState() {
  Core *c = core; // FUN_80106478
  TaskSm sm(c);
  switch (sm.stage4c()) {
  case 0: {
    psx::cpu::dispatchGuestToReturn0(
        *c, 0x8001CF2Cu, psx::cpu::ExecutionBudget::currentTurn(*c), __func__); // engine tick (substrate)
    c->mem_w16(0x800BE222u,
               0x47FFu); // FUN_80075CEC(0x47ff): fade target (inlined —
                         // same private leaf music_coord.cpp inlines;
                         // see BgSceneTransitionSm::audioFadeTarget)
    sm.setIntroTimer(0x14A);
    sm.setStage4c((uint16_t)(sm.stage4c() + 1));
    eng(c).audioDispatch.dispatch3Way(0x2C,
                                      0); // native — was FUN_800750D8(0x2c,0)
    psx::cpu::dispatchGuestToReturn0(*c,
                                     0x8004D8B0u,
                                     psx::cpu::ExecutionBudget::currentTurn(*c),
                                     __func__); // 128-byte zero-init (substrate; un-owned this pass)
    c->mem_w8(0x1F800206u, 0);
    break;
  }
  case 1: {
    int16_t v = (int16_t)(sm.introTimer() - 1);
    sm.setIntroTimer((uint16_t)v);
    if (v == 0 || (v < 0x10E && c->mem_r16(0x800E7E68u) != 0)) {
      sm.setStage4c((uint16_t)(sm.stage4c() + 1));
    }
    c->r[4] = 0xA0;
    c->r[5] = 0x70;
    c->r[6] = 0;
    c->r[7] = 0x17A;
    psx::cpu::dispatchGuestToReturn0(
        *c, 0x8007E8DCu, psx::cpu::ExecutionBudget::currentTurn(*c), __func__); // camera/view helper (substrate)
    break;
  }
  case 2:
    psx::cpu::dispatchGuestToReturn0(
        *c, 0x8001CF2Cu, psx::cpu::ExecutionBudget::currentTurn(*c), __func__); // engine tick (substrate)
    eng(c).audioDispatch.selectState(4);                                        // native — was FUN_800750A4(4)
    sm.setS4e(1);
    sm.setF6b(0);
    sm.setStage4c((uint16_t)(sm.stage4c() + 1));
    sm.setS50(0);
    [[fallthrough]];
  case 3: {
    c->r[4] = sm.s4e();
    psx::cpu::dispatchGuestToReturn0(
        *c, 0x8007ED5Cu, psx::cpu::ExecutionBudget::currentTurn(*c), __func__); // SAVE-prompt text render (substrate)
    if (c->mem_r16(0x800E7E68u) & 0x4000u) {
      uint16_t e = sm.s4e();
      if (e == 0) {
        psx::cpu::dispatchGuestToReturn0(
            *c, 0x80078824u, psx::cpu::ExecutionBudget::currentTurn(*c), __func__); // AREA START POS write (substrate)
        sm.setStage4c(8);
        c->mem_w8(0x800BF84Au, 0);
      } else if (e == 1) {
        sm.setStage4c((uint16_t)(sm.stage4c() + 1));
      }
      sm.setF6b(0);
      sm.setS4e(0);
      sm.setS50(0);
      eng(c).sfx.trigger(0x11, 0, 0); // native — was FUN_80074590(0x11,0,0)
    }
    if ((c->mem_r16(0x800E7E68u) & 0x10u) == 0) {
    areaload_joined_68f4:
      if ((c->mem_r16(0x800E7E68u) & 0x40u) == 0) {
        return;
      }
      if (sm.s4e() != 0) {
        return;
      }
      sm.setS4e(1);
    } else {
      if (sm.s4e() == 0) {
        return;
      }
      sm.setS4e((uint16_t)(sm.s4e() - 1));
    }
  areaload_lab_106918:
    eng(c).sfx.trigger(0x15, 0, 0); // native — was FUN_80074590(0x15,0,0)
    break;
  }
  case 4: {
    c->r[4] = sm.s4e();
    psx::cpu::dispatchGuestToReturn0(*c,
                                     0x8007EE74u,
                                     psx::cpu::ExecutionBudget::currentTurn(*c),
                                     __func__); // CONTINUE/LOAD/QUIT prompt render (substrate)
    if ((c->mem_r16(0x800E7E68u) & 0x4000u) == 0) {
      if (c->mem_r16(0x800E7E68u) & 0x2000u) {
        int16_t s = (int16_t)(sm.stage4c() - 1);
        sm.setS4e(0);
        sm.setF6b(0);
        sm.setStage4c((uint16_t)s);
        eng(c).sfx.trigger(0x14, -9, 0); // native — was FUN_80074590(0x14,-9,0)
        goto areaload_case4_edge;
      }
    } else {
      uint16_t e = sm.s4e();
      if (e == 1) {
        sm.setStage4c(7);
        c->mem_w8(0x800BF84Au, 0);
        psx::cpu::dispatchGuestToReturn0(
            *c, 0x8001CF2Cu, psx::cpu::ExecutionBudget::currentTurn(*c), __func__); // engine tick (substrate)
      } else if (e == 0) {
        sm.setTop(2);
        sm.setSubMode(1);
        sm.setStage4c(0);
        psx::cpu::dispatchGuestToReturn0(
            *c, 0x8001CF2Cu, psx::cpu::ExecutionBudget::currentTurn(*c), __func__); // engine tick (substrate; guest arg
                                                                                    // 0x11 unused by callee)
      }
      // e == 2: sm[0x4c]++ (no other write) falls straight through to the
      // shared tail below.
      if (e == 2) {
        sm.setStage4c((uint16_t)(sm.stage4c() + 1));
      }
      sm.setF6b(0);
      sm.setS4e(0);
      sm.setS50(0);
      eng(c).sfx.trigger(0x11, 0, 0); // native — was FUN_80074590(0x11,0,0)
    }
  areaload_case4_edge:
    if ((c->mem_r16(0x800E7E68u) & 0x10u) == 0) {
      if ((c->mem_r16(0x800E7E68u) & 0x40u) == 0) {
        return;
      }
      if (sm.s4e() > 1) {
        return;
      }
      sm.setS4e((uint16_t)(sm.s4e() + 1));
    } else {
      if (sm.s4e() == 0) {
        return;
      }
      sm.setS4e((uint16_t)(sm.s4e() - 1));
    }
    goto areaload_lab_106918;
  }
  case 5: {
    c->r[4] = sm.s4e();
    psx::cpu::dispatchGuestToReturn0(
        *c, 0x8007EF60u, psx::cpu::ExecutionBudget::currentTurn(*c), __func__); // QUIT-confirm render (substrate)
    if (c->mem_r16(0x800E7E68u) & 0x4000u) {
      uint16_t e = sm.s4e();
      if (e == 0) {
        sm.setStage4c((uint16_t)(sm.stage4c() + 1));
        psx::cpu::dispatchGuestToReturn0(
            *c, 0x8001CF2Cu, psx::cpu::ExecutionBudget::currentTurn(*c), __func__); // engine tick (substrate)
        return;
      }
      if (e != 1) {
        return;
      }
      int16_t s = (int16_t)(sm.stage4c() - 1);
      sm.setS4e(0);
      sm.setF6b(0);
      sm.setStage4c((uint16_t)s);
      eng(c).sfx.trigger(0x14, -9, 0); // native — was FUN_80074590(0x14,-9,0)
      return;
    }
    if (c->mem_r16(0x800E7E68u) & 0x10u) {
      if (sm.s4e() == 0) {
        return;
      }
      sm.setS4e((uint16_t)(sm.s4e() - 1));
      goto areaload_lab_106918;
    }
    goto areaload_joined_68f4;
  }
  case 6:
    sm.setSubMode(4);
    sm.setStage4c(0);
    sm.setS4e(0);
    break;
  case 7: {
    tomba::scene::stepCardLoadMachine(*c, 0u, 1u); // = jal 0x8007BF20(0,1): LOAD page, native+sync
    uint8_t result = sm.f6b();
    if (result == 7) {
      reloadEntityPool(); // native — was FUN_8007B3F4()
      c->mem_w8(0x1F800134u, 1);
      sm.setSubMode(1);
      sm.setStage4c(0);
      sm.setS4e(0);
      return;
    }
    if ((uint8_t)(result - 1u) > 1u) {
      return;
    }
    sm.setStage4c(4);
    sm.setS4e(1);
    break;
  }
  case 8: {
    tomba::scene::stepCardLoadMachine(*c, 0x81u, 1u); // = jal 0x8007BF20(0x81,1): SAVE page, native+sync
    uint8_t result = sm.f6b();
    if (result == 9) {
      sm.setF6b(0);
      sm.setStage4c(4);
      sm.setS4e(0);
      sm.setS50(0);
      return;
    }
    if ((uint8_t)(result - 1u) > 1u) {
      return;
    }
    sm.setStage4c(3);
    sm.setS4e(1);
    break;
  }
  default:
    break; // s4c >= 9 -> no-op (shared epilogue; matches the guest's no-default
           // switch)
  }
}

// ---- NATIVE PER-FRAME GAME LOOP (game_native path, mirrors DEMO demo_native)
// ------------------------ The GAME stage is owned as a native per-frame
// dispatcher instead of guest-continuationing into the guest loop 0x801063F4: each
// frame ov_game_frame runs ONE loop iteration natively (dispatch the sm[0x48]
// handler + bump the frame counter), and "yield" = return. This re-wires the
// previously-orphaned native handlers and descends ownership into gameplay (the
// SOP field-mode machine). Prereq landed (later-217b): the SOP area load is
// native+synchronous, so SOP state-0 never yields.

// (ov_sop_field_mode moved to Sop::fieldMode — eng(c).sop.fieldMode())
#include "camera/cutscene_camera.h" // class CutsceneCamera — resident driver 0x8006EC44 (native)
#include "render/screen_fade.h"     // class ScreenFade — the single fade driver
#include "sop.h"                    // class Sop — transitionAreaLoad (sync FIELD transition load)
                                    // menu + cursor/page-transition handling

// FUN_8010810C page-1 dim-fade branch (task+0x6B == 1, "draw main pause menu" —
// see game/ui/menu.cpp's RE of the same dispatcher). Disasm
// (authenticated executable/overlay evidence, label L_8010829C): while the
// pause-menu page-1 handler is selected, EVERY frame it runs an UNCONDITIONAL,
// NON-RAMPING flat-gray dim (FUN_8007E9C8(0x00808080, a1=0, weight=4) ->
// engine_fade_set) then falls through to FUN_801084F8 (menu draw + cursor/page
// nav, still guest instruction path). This is NOT the reference per-node fade SM (no ramp
// counter, no node state) — own JUST this page's shape here; the other 11 pages
// + the dispatcher's bounds-check/table jump stay guest instruction path via psx::cpu::callGuestNow(*c, __func__,
// 0x8010810cu) (own-caller-before-callee: the caller (ov_field_frame et al.) is already native, but the callee's other
// pages are unexplored, so full transcription is out of scope).
void Engine::submitPage810c() {
  Core *c = core;
  uint32_t task = c->mem_r32(0x1F800138u);
  if (task && c->mem_r8(task + 0x6Bu) == 1) {
    fade(c).applyLeafCall(0x808080u, ScreenFade::kSubtractive, 4);
    psx::cpu::dispatchGuestToReturn0(*c,
                                     0x801084F8u,
                                     psx::cpu::ExecutionBudget::currentTurn(*c),
                                     __func__); // still guest instruction path: menu draw + cursor/page transitions
    return;
  }
  psx::cpu::callGuestNow(*c, __func__, 0x8010810cu);
}

// (ov_objwalk moved to ObjectList::walkAll — eng(c).objectList.walkAll())
// (ov_disp_26c88 moved to ObjectTable::dispatch —
// eng(c).objectTable.dispatch()) (ov_list_walk_69b28 moved to
// ObjectList::walkAux — eng(c).objectList.walkAux()) (ov_arr8_dispatch_26368
// moved to Array8Dispatch::tick — eng(c).array8Dispatch.tick()) (submode0 /
// submode1 are now Engine methods — Engine::submode0() / Engine::submode1())
// (fieldTransition + workers moved to FieldTransition — game/scene/field_transition.{h,cpp} —
// eng(c).fieldTransition.step())

// sm[0x48]==2 RUNNING, per-frame variant: dispatch sm[0x4a] handler. handler[0]
// = the GAME->SOP bridge 0x8010882c (owned native, ov_game_submode0); the
// others stay typed runtime address dispatch leaves (synchronous; a not-yet-sync leaf that yields
// is contained by the scheduler setjmp = frame-done). GUEST FRAME MIRROR
// (abi_extract --contract 0x80108784: single epilogue label at L_8010881C,
// spill precedes the sm[0x4a]<6 check -> GuestFrame RAII is safe): sp-24,
// ra@+16.
void Engine::stageRunning() {
  Core *c = core;
  static const uint32_t handler[6] = {
      0x8010882cu,
      0x801088d8u,
      0x80106478u,
      0x80106a24u,
      0x801089c4u,
      0x80108a60u,
  };
  static const uint32_t jal_ra[6] = {
      // 80108784's per-case jal sites
      0x801087CCu,
      0x801087DCu,
      0x801087ECu,
      0x801087FCu,
      0x8010880Cu,
      0x8010881Cu,
  };
  static constexpr GuestFrameSpill kSpills[] = {{31, 16}};
  GuestFrame<24, 1> frame(c, kSpills);
  TaskSm sm(c);
  uint16_t s4a = sm.subMode();
  if (cfg_dbg("stage") && s4a != mLast4a) {
    cfg_logf("stage", "stageRunning: sm[0x4a]=%u sm[0x4c]=%u", s4a, sm.stage4c());
    mLast4a = s4a;
  }
  if (s4a < 6) {
    c->r[31] = jal_ra[s4a];
    if (s4a == 0) {
      eng(c).submode0();
    } else if (s4a == 1) {
      eng(c).submode1();
    } else if (s4a == 5) {
      eng(c).fieldTransition.step();
    } // native FUN_80108a60
    else if (s4a == 2) {
      eng(c).areaLoadState();
    } // native FUN_80106478
    else {
      psx::cpu::dispatchGuestToReturn0(*c, handler[s4a], psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
    }
  }
}

// GAME sub-mode-0 bridge 0x8010882c (sm[0x4c]/sm[0x4e] dispatch) — native.
// Faithful to the disasm: sm[0x4c]==0 & sm[0x4e]==0 -> input-reset 0x8005082c
// (sync leaf) + sm[0x50]=0, sm[0x4e]=1; sm[0x4e]==1
// -> run the native SOP field-mode machine; sm[0x4c]==1 -> sm[0x4c]=0,
// sm[0x4a]++. GUEST FRAME MIRROR (abi_extract --contract 0x8010882C: single
// epilogue label at L_801088C8 -> GuestFrame RAII is safe): sp-40; r16@+32,
// ra@+36; live r16=0x1F800000.
void Engine::submode0() {
  Core *c = core;
  static constexpr GuestFrameSpill kSpills[] = {{16, 32}, {31, 36}};
  GuestFrame<40, 2> frame(c, kSpills);
  c->r[16] = 0x1F800000u;
  TaskSm sm(c);
  if (sm.stage4c() == 0) {
    if (sm.s4e() == 0) {
      c->r[4] = 0;
      c->r[5] = 0;
      c->r[6] = 0;
      tomba::guest::dispatchJalToReturn(*c, 0x8005082cu, 0x8010888Cu); // input reset (leaf, no yield)
      TaskSm(c).setS50(0); // re-derive base: input reset may relocate the task record
      TaskSm(c).setS4e((uint16_t)(TaskSm(c).s4e() + 1));
    } else if (sm.s4e() == 1) {
      c->r[31] = 0x801088B0u;
      // 0x80109450 is the loaded MODE overlay's field-mode fn. Our native
      // machine is SOP-specific, so only use it when SOP is actually loaded
      // (signature = its first insn `lui v0,0x1f80` = 0x3C021F80); for any
      // other mode/field overlay, dispatch the guest fn (until that overlay is
      // owned natively too).
      if (c->mem_r32(0x80109450u) == 0x3C021F80u) {
        eng(c).sop.fieldMode(); // native SOP; unowned calls cross the runtime guest boundary
      } else {
        psx::cpu::dispatchGuestToReturn0(
            *c, 0x80109450u, psx::cpu::ExecutionBudget::currentTurn(*c), __func__); // other overlay -> guest
      }
    }
  } else if (sm.stage4c() == 1) {
    uint16_t s4a = sm.subMode();
    sm.setStage4c(0);
    sm.setSubMode((uint16_t)(s4a + 1));
  }
}

// GAME sm[0x4a]==1 handler 0x801088d8 — the FIELD area machine (the actual
// walkable field, loaded AFTER the SOP intro). Native: a jump-table switch on
// sm[0x4c] (table @0x80106334, 7 states). Faithful to the disasm:
//   state 0 (0x80108918): FUN_8005245c() (sound/CD setup, sync leaf), then the
//   area-DATA load
//     FUN_80044bd4(0x800452c0, *0x800bf870, 0, 2) — the COOPERATIVE
//     spawn-and-wait. We own it INLINE and SYNCHRONOUS via
//     native_transition_area_load (no task spawn, no yield) — the prereq for
//     the per-frame model (re-entering a yielding state-0 would re-spawn the
//     load forever).
//   state 1 (0x8010893c): 0x1f800234=0; sm[0x4c] = *(u8*)(0x80108f60 +
//   *0x800bf870)  (next-state table) states 2..6 (0x8010896c/7c/8c/9c/ac): jal
//   the field RUNNING sub-machine handler:
//     2->0x80106b98  3->0x801070b4  4->0x80107230  5->0x8010766c  6->0x80107790
//     These are YIELD-FREE (transitive jal-graph scan: no FUN_80051f80 once
//     CD/audio-busy are sync leaves), so they typed runtime address dispatch synchronously
//     per-frame and return = the frame's gameplay work.
//   sm[0x4c] >= 7 -> no-op (falls to the epilogue).

// Native FUN_80025588 — the field EVENT/COMMAND-QUEUE state machine (struct
// @0x800ed058). A 3-state top switch on base[2]: state 0 ARMS it (base[2]=1,
// base[9]=0, snapshot list head 0x800ecf58 into base[0x3c], clear 0x800bfa5c,
// run setup 0x80024e00) then FALLS THROUGH into the active body; state 1 is the
// active body; state >=2 is a no-op. Active body drains a small FIFO when
// base[0x14]==0 && base[0x15]!=0: entry 0 is dispatched via 0x80040aa4(id,kind)
// then 0x80074bf8(kind==0 ? 2 : 3, only for kind 0/1); the two parallel byte
// arrays base[0x16] (id) / base[0x1c] (kind) shift down one; base[0x15]--,
// base[0x14]++. Then 0x80024f18 always, and per the GAME phase 0x800bf870
// either 0x800251f0 (default) or a light-toggle of base[8] (phase 2/7, gated on
// 0x800bf816==0 && (0x800e7e68 & 0x0c00)); phases 3/20 do neither. Always ends
// with 0x80077b5c. All leaf callees stay substrate; only the control flow is
// native. Faithful to the guest instruction path; a direct child of ov_field_frame (was
// `psx::cpu::callGuestNow(*c, __func__, 0x80025588)`).
void Engine::sceneEventFifo() {
  Core *c = core;
  // strict replay check on this fork FAILED at 0x801FE954 (a leaf's own stack spill slot)
  // with registers (v0/v1/s0-s7/gp/sp/fp/ra/hi/lo) all MATCHING — i.e.
  // sceneEventFifoFaithful()'s own control flow/constants are not in question.
  // Re-derived guest 0x80025588 by hand against the mirror line-by-line
  // (frame/ra discipline, the kind==0/1/>=2 branch at L_80025610..30, the FIFO
  // shift-loop trip count, and the phase 3/20-nothing vs 2/7-light-toggle vs
  // else-0x800251f0 branch at L_80025694..728) and found it byte-identical —
  // this method is NOT the bug. The FIFO-drain calls 6 leaves that all "stay
  // substrate" (0x80024e00/40aa4/74bf8/24f18/251f0/ 77b5c) and none is proven
  // yield-free/side-effect-free: guest 0x80074bf8's (music/track-control) call
  // graph reaches guest 0x80086620, which dispatches through a RUNTIME
  // function-pointer slot (not a static jump table the recorded binary evidence could
  // enumerate, unlike every other indirect dispatch in this call graph, which
  // resolves to a closed case set) — a plausible current-BGM- handler callback.
  // strict replay check runs the whole call graph TWICE (native leg, then rewound
  // substrate leg) from one snapshot; any such leaf whose outcome depends on
  // state outside {RAM, scratchpad, GPRs, hi/lo} (audio/sequencer engine state,
  // an SPU voice cursor, ...) can legitimately produce a different
  // second-invocation result — exactly the documented gate limit ("host hw side
  // effects run twice while armed") already hit and handled the same way by
  // sceneRenderListBuilder() right below. Plain call, not strict replay check; SBS (true
  // single-invocation lockstep) is the correct gate for this leaf chain. Re-arm
  // once the leaves are proven side-effect-free or ported native.
  const uint32_t B = 0x800ed058u;
  uint8_t st = c->mem_r8(B + 2);
  if (st == 0) {
    c->mem_w8(B + 2, 1);
    c->mem_w8(B + 9, 0);
    uint32_t head = c->mem_r32(0x800ecf58u);
    c->mem_w8(0x800bfa5cu, 0);
    c->mem_w32(B + 0x3c, head);
    psx::cpu::callGuestNow(*c, __func__, 0x80024e00u, B);
    // fall through into the active body
  } else if (st != 1) {
    return; // st >= 2: guest jumps straight to the epilogue
  }
  if (c->mem_r8(B + 0x14) == 0 && c->mem_r8(B + 0x15) != 0) {
    psx::cpu::callGuestNow(*c, __func__, 0x80040aa4u, c->mem_r8(B + 0x16), c->mem_r8(B + 0x1c));
    uint8_t kind = c->mem_r8(B + 0x1c);
    if (kind == 0) {
      psx::cpu::callGuestNow(*c, __func__, 0x80074bf8u, 2);
    } else if (kind == 1) {
      psx::cpu::callGuestNow(*c, __func__, 0x80074bf8u, 3);
    }
    int n = (int)c->mem_r8(B + 0x15) - 1; // shift the FIFO down by one (drop entry 0)
    for (int i = 0; i < n; i++) {
      c->mem_w8(B + 0x16 + i, c->mem_r8(B + 0x16 + i + 1));
      c->mem_w8(B + 0x1c + i, c->mem_r8(B + 0x1c + i + 1));
    }
    c->mem_w8(B + 0x15, (uint8_t)(c->mem_r8(B + 0x15) - 1));
    c->mem_w8(B + 0x14, (uint8_t)(c->mem_r8(B + 0x14) + 1));
  }
  psx::cpu::callGuestNow(*c, __func__, 0x80024f18u, B);
  uint8_t phase = c->mem_r8(0x800bf870u);
  if (phase == 3 || phase == 20) {
    // neither the light-toggle nor 0x800251f0
  } else if (phase == 2 || phase == 7) {
    if (c->mem_r8(0x800bf816u) == 0 && (c->mem_r16(0x800e7e68u) & 0x0c00) != 0) {
      c->mem_w8(B + 8, (uint8_t)(1 - c->mem_r8(B + 8)));
    }
  } else {
    psx::cpu::callGuestNow(*c, __func__, 0x800251f0u, B);
  }
  psx::cpu::callGuestNow(*c, __func__, 0x80077b5cu, B);
}

// Native FUN_8004FE84 — a 2-phase scene/render-list builder driver (struct
// @0x800bf548). base[0] is the phase: 0 -> ARM (snapshot list ptr 0x800ecf64
// into base+0x2b0, +0x2b4 = ptr+0x10, +0x2b8 = ptr+0x10 +
// (*(u16)ptr << 1); base[1]=0, base[0]=1); 1 -> run sub-state base[1]
// (0->0x8004f430, 1->0x8004f474, 2->0x8004f514, 3->0x8004f6d0, >=4 none). After
// the sub-state, set bit0 of flag byte @0x800bf822 when (base[1]!=0 ||
// base[0x0a]!=0) else clear it. phase>=2 is a no-op. Leaf callees stay
// substrate. Faithful to the guest instruction path; a direct child of ov_field_frame (was
// `psx::cpu::callGuestNow(*c, __func__, 0x8004fe84)`).
