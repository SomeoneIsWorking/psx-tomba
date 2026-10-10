// game/core/engine_field_run.cpp — the field RUN and FRAME-X families — the per-frame field work and its guest-faithful
// mirror, plus the running sub-mode's own two cases.
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

void Engine::fieldRun() {
  Core *c = core;
  // fieldRun can yield transitively (case 0's substrate init chain descends
  // into loaders) — plain call, not strict replay check: fieldRunFaithful is not
  // byte-exact yet (12+ diffs at 0x801FE8xx / v0 / v1,
  // scratch/logs/mv_tdd.log), and strict replay check's synchronous compare is wrong
  // across a yield boundary regardless. SBS-gated; fix fieldRunFaithful and
  // re-arm strict replay check once it's byte-exact and known yield-free (or drop this
  // call for a plain one permanently if it always yields).
  uint32_t sm = c->mem_r32(0x1f800138u);
  uint16_t s4e = c->mem_r16(sm + 0x4e);
  switch (s4e) {
  case 0:
    eng(c).pool.init();                  // OWNED native (game/world/pool.cpp) — replaces
                                         // typed runtime address dispatch(0x8007b18c)
    eng(c).pool.resetControlBlock();     // OWNED native (game/world/pool.cpp) —
                                         // replaces typed runtime address dispatch(0x800796dc)
    eng(c).pool.seedAreaObjects();       // OWNED native (game/world/pool.cpp) —
                                         // replaces typed runtime address dispatch(0x800263e8)
    eng(c).placement.placeAreaObjects(); // OWNED native
                                         // (game/world/placement.cpp) —
                                         // replaces typed runtime address dispatch(0x80072a78)
    eng(c).pool.reset75240();            // OWNED native (game/world/pool.cpp) — replaces
                                         // typed runtime address dispatch(0x80075240)
    eng(c).pool.setupViewScroll();       // OWNED native (game/world/pool.cpp) —
                                         // replaces typed runtime address dispatch(0x800783dc)
    eng(c).pool.finalViewInit();         // OWNED native (game/world/pool.cpp) —
                                         // replaces typed runtime address dispatch(0x80078610)
    sm = c->mem_r32(0x1f800138u);
    c->mem_w16(sm + 0x4e, 1);
    c->mem_w8(sm + 0x6b, 0);
    if (c->mem_r8(0x800bf89cu) == 2) {
      c->mem_w16(sm + 0x4e, 9);
    } else if (c->mem_r8(0x800bf870u) == 8) {
      psx::cpu::callGuestNow(*c, __func__, 0x80114b90u);
    }
    // HALFWORD read, matching gen (overlay guest 0x80106B98: `mem_r16((r4 +
    // -1936))`) and the faithful mirror above. It was `mem_r32(...) == 0x15`,
    // which also covers bf872/73 and so was effectively never true — this
    // transition never fired under native_sync. (A first pass "fixed" it to
    // mem_r8 on a bad audit hit; gen settles it: r16.)
    else if (c->mem_r16(0x800bf870u) == 21) {
      c->mem_w16(sm + 0x4e, 0xb);
      return;
    }
    eng(c).pool.selectStateIndex(c->mem_r8(0x800bf870u)); // OWNED native — replaces the guest call 0x80074f24(area)
    break;
  case 2:
    eng(c).gStateMutate(0x800E7E80u,
                        0xC); // native — was typed runtime address dispatch 0x80058304(G, 0xC)
    sm = c->mem_r32(0x1f800138u);
    c->mem_w16(sm + 0x4e, (uint16_t)(c->mem_r16(sm + 0x4e) + 1));
    /* fallthrough */
  case 3:
    eng(c).audioDispatch.settleField(); // native — was typed runtime address dispatch 0x80074BC4
    if (c->mem_r8(0x800bf870u) == 8) {
      psx::cpu::callGuestNow(*c, __func__, 0x80114b90u);
    }
    sm = c->mem_r32(0x1f800138u);
    c->mem_w16(sm + 0x4a, 2);
    c->mem_w16(sm + 0x4c, 0);
    c->mem_w16(sm + 0x4e, 0);
    eng(c).modeStateArm.arm(); // native — was typed runtime address dispatch 0x8005082C(0,0,0)
    break;
  case 4:
    psx::cpu::callGuestNow(*c, __func__, 0x8006c7c4u);
    eng(c).modeStateArm.armFromAreaTable(); // native — was typed runtime address dispatch
                                            // 0x800508A8
    c->mem_w16(c->mem_r32(0x1f800138u) + 0x4e, 1);
    /* fallthrough */
  case 1: {
    eng(c).fieldFrame(); // native field per-frame update (0x80108b0c)
    sm = c->mem_r32(0x1f800138u);
    if (c->mem_r8(0x800bf80du) == 3) { // (signed byte) special mode 3
      if (c->mem_r8(0x800bf80fu) == 0) {
        eng(c).audioDispatch.settleField(); // native — was typed runtime address dispatch
                                            // 0x80074BC4
        sm = c->mem_r32(0x1f800138u);
        if (c->mem_r16s(0x800e7feeu) == 0) { // halfword: guest uses lh here, never lw
          c->mem_w16(sm + 0x4e,
                     (uint16_t)(c->mem_r16(sm + 0x4e) + 1)); // LAB_80106f48
        } else {
          c->mem_w8(0x800bf880u, 1);
          c->mem_w16(0x1f800194u, (uint16_t)c->mem_r16(0x800e7feeu)); // halfword store, per gen
          c->mem_w16(sm + 0x4e, 0);
        }
      }
    } else if (c->mem_r8(0x800bf839u) != 0 && c->mem_r8(0x800bf80fu) == 0) {
      if (c->mem_r8(0x800bf839u) == 8) {
        c->mem_w16(sm + 0x4a, 3);
        c->mem_w16(sm + 0x4c, 0);
        c->mem_w16(sm + 0x4e, 0);
      } else {
        if (c->mem_r8(0x1f800236u) > 4) {
          psx::cpu::callGuestNow(*c, __func__, 0x80050894u, 0); // LAB_80106fac join
        }
        sm = c->mem_r32(0x1f800138u);
        c->mem_w16(sm + 0x4a, 1);
        c->mem_w16(sm + 0x4c, 2);
        c->mem_w16(sm + 0x4e, 6);
      }
    }
    break;
  }
  case 5:
    if (c->mem_r8(0x800bf870u) == 7) {
      psx::cpu::callGuestNow(*c, __func__, 0x801128bcu);
      eng(c).modeStateArm.armFromAreaTable();
    }
    c->mem_w16(c->mem_r32(0x1f800138u) + 0x4e, 1);
    eng(c).fieldFrame();
    break;
  case 6: {
    if (c->mem_r16s(0x800e7feeu) != 0) {
      c->mem_w8(0x800bf880u, 1);
      c->mem_w16(0x1f800194u, (uint16_t)c->mem_r16(0x800e7feeu));
    }
    eng(c).audioDispatch.settleField(); // native — was typed runtime address dispatch 0x80074BC4
    // _DAT_800bf870 = CONCAT11(...) & 0x3f1f  — the decomp's byte-swap-and-mask
    // of *0x800bf83a into bf870
    uint16_t b83a = c->mem_r16(0x800bf83au);
    uint32_t v = (((uint32_t)(b83a & 0xff) << 8) | (b83a >> 8)) & 0x3f1f;
    // TWO BYTE stores, not a word: bf870 is a byte field and bf872/73 are live
    // state a 32-bit store would zero. (Documented in fieldRunFaithful's
    // header; confirmed by the guest only ever using sb here.)
    c->mem_w8(0x800bf870u, (uint8_t)(v & 0xff));
    c->mem_w8(0x800bf871u, (uint8_t)((v >> 8) & 0xff));
    if (c->mem_r8(0x800bf839u) == 7) {
      psx::cpu::callGuestNow(*c, __func__, 0x80114b90u);
      c->mem_w8(0x800bf839u, 3);
    }
    sm = c->mem_r32(0x1f800138u);
    if (c->mem_r8(0x800bf839u) == 3) {
      psx::cpu::callGuestNow(*c, __func__, 0x8005245cu);
      sm = c->mem_r32(0x1f800138u);
      c->mem_w16(sm + 0x48, 2);
      c->mem_w16(sm + 0x4a, 1);
      c->mem_w16(sm + 0x4c, 1);
      c->mem_w16(sm + 0x4e, 0);
    } else {
      c->mem_w16(sm + 0x48, 2);
      uint8_t b = c->mem_r8(0x1f800236u);
      c->mem_w16(sm + 0x4a, 5);
      c->mem_w16(sm + 0x4e, 0);
      c->mem_w16(sm + 0x4c, b);
    }
    break;
  }
  case 7: {
    psx::cpu::callGuestNow(*c, __func__, 0x80045580u, 1);
    if (c->r[2] == 0) {
      return;
    }
    sm = c->mem_r32(0x1f800138u);
    c->mem_w16(sm + 0x4e,
               (uint16_t)(c->mem_r16(sm + 0x4e) + 1)); // goto LAB_80106f48
    break;
  }
  case 8:
    if (c->mem_r8(0x1f80019bu) == 0) {
      return;
    }
    c->mem_w8(0x800bf89cu, 4);
    c->mem_w8(0x1f800236u, 0);
    c->mem_w8(0x800bf839u, 3);
    c->mem_w16(0x800bf83au, 0);
    sm = c->mem_r32(0x1f800138u);
    c->mem_w16(sm + 0x4a, 1);
    c->mem_w16(sm + 0x4c, 2);
    c->mem_w16(sm + 0x4e, 6); // LAB_80106fac
    break;
  case 9:
    eng(c).fieldFrame();
    sm = c->mem_r32(0x1f800138u);
    if (c->mem_r8(0x800bf89cu) == 2 && (c->mem_r16(0x800e7e68u) & 8) != 0) {
      c->mem_w16(sm + 0x4e, (uint16_t)(c->mem_r16(sm + 0x4e) + 1));
      c->mem_w8(0x800bf809u, 1);
      c->mem_w8(sm + 0x6e, 0x1f);
    }
    break;
  case 10: {
    eng(c).fieldFrame();
    sm = c->mem_r32(0x1f800138u);
    uint32_t u = ((uint32_t)c->mem_r8(sm + 0x6e) * (uint32_t)-8) & 0xff;
    cfg_logf("fadesites", "[fadesite] fieldRun-case10 u=%02x sm6e=%u", u, c->mem_r8(sm + 0x6e));
    fade(c).applyLeafCall((u << 16) | (u << 8) | u, ScreenFade::kSubtractive, 0);
    uint8_t nv = (uint8_t)(c->mem_r8(sm + 0x6e) - 1);
    c->mem_w8(sm + 0x6e, nv);
    if (nv == 0) {
      c->mem_w16(sm + 0x4e, 7);
      psx::cpu::callGuestNow(*c, __func__, 0x8001cf2cu);
    }
    break;
  }
  case 0xb:
    eng(c).areaFadeSequencer.step(0x800e8008u);
    break;
  default:
    break;
  }
}

void Engine::fieldFrameX() {
  Core *c = core;
  c->mem_w16(0x1f80017cu,
             (uint16_t)(c->mem_r16(0x1f80017cu) + 1)); // frame counter
  c->mem_w32(0x800bf878u, c->mem_r32(0x800bf878u) + 1);
  if (c->mem_r8(0x1f800136u) == 0) { // not paused: reduced gameplay update
    eng(c).frameStartTick();
    eng(c).objectList.walkAux();
    psx::cpu::callGuestNow(*c, __func__, 0x80026368u);
    eng(c).transitionState3.walkOnce(); // 0x80059d28/0x80069b28/0x8007b04c NATIVE
    eng(c).sceneEventFifo();
    eng(c).sceneRenderListBuilder();
    eng(c).objectTable.dispatch();
    eng(c).modePerFrameDispatch(); // 25588/4fe84/26c88/22a80 NATIVE
    tomba::camera::CutsceneCamera(c, tomba::camera::kCameraObject)
        .update(); // 0x8006ec44 NATIVE (CutsceneCamera::update)
  }
  if (c->mem_r8(0x1f800136u) < 2) {
    rend(c)->frameX(); // 0x8003fa44 — NATIVE render orchestrator twin
  }
  eng(c).submitPage810c();       // render submit (page-1 dim-fade owned; other pages
                                 // guest instruction path)
  eng(c).postRenderTick();       // 0x80077D8C NATIVE (was d0)
  eng(c).areaSlots.updateTail(); // 0x80075a80 NATIVE
}

void Engine::fieldRunX() {
  Core *c = core;
  uint32_t sm = c->mem_r32(0x1f800138u);
  uint16_t s4e = c->mem_r16(sm + 0x4e);
  if (s4e >= 2) {
    if (s4e == 2) {
      c->mem_w16(sm + 0x4e, 1); // 0x8010721c: re-arm to running
    }
    return;
  }
  if (s4e == 0) { // 0x80107100: init
    c->mem_w16(sm + 0x4e, 1);
    // 0x8006c77c — the per-area transition-ENTER hook (every area's, 0..21).
    // Its cooperative FUN_80044BD4 spawn-and-wait completes on this leg too:
    // PcScheduler::spawnAndWait runs the spawned body inline when the waiting
    // task cannot suspend (kanban #50).
    c->r[31] = 0x80107108u;
    psx::cpu::dispatchGuestToReturn0(*c, 0x8006c77cu, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
    psx::cpu::callGuestNow(*c, __func__, 0x8005082cu, 0, 0, 0); // input reset
    // fall through to state 1
  }
  eng(c).fieldFrameX();              // 0x80108be4 per-frame (state 1, 0x80107118)
  if (c->mem_r8(0x800bf80du) == 3) { // mode-3 exit (0x80107138)
    if (c->mem_r8(0x800bf80fu) != 0) {
      return;
    }
    eng(c).audioDispatch.settleField(); // native — was typed runtime address dispatch 0x80074BC4
    sm = c->mem_r32(0x1f800138u);
    c->mem_w16(sm + 0x4c, 2); // back to normal running handler
    int16_t e_s = c->mem_r16s(0x800e7feeu);
    uint16_t e_u = c->mem_r16(0x800e7feeu);
    if (e_s != 0) {
      c->mem_w8(0x800bf880u, 1);
      c->mem_w16(0x1f800194u, e_u); // sh (16-bit per disasm)
      c->mem_w16(sm + 0x4e, 0);
    } else {
      c->mem_w16(sm + 0x4e, 2);
    }
    return;
  }
  // not mode-3 (0x80107194): area-change request via bf839
  uint8_t bf839 = c->mem_r8(0x800bf839u);
  if (bf839 == 0) {
    return;
  }
  if (c->mem_r8(0x800bf80fu) != 0) {
    return;
  }
  if (bf839 == 8) { // 0x801071bc
    sm = c->mem_r32(0x1f800138u);
    c->mem_w16(sm + 0x4a, 1);
    c->mem_w16(sm + 0x4c, 2);
    c->mem_w16(sm + 0x4e, 3);
    return;
  }
  if (c->mem_r8(0x1f800236u) >= 5) {
    psx::cpu::callGuestNow(*c, __func__, 0x80050894u, 0); // 0x801071f0
  }
  sm = c->mem_r32(0x1f800138u);
  c->mem_w16(sm + 0x4a, 1);
  c->mem_w16(sm + 0x4c, 2);
  c->mem_w16(sm + 0x4e, 6);
}

void Engine::submode1Case0Native() {
  Core *c = core;
  psx::cpu::dispatchGuestToReturn0(*c, 0x8005245cu, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
  // The owned load is synchronous: retain FUN_80044BD4's flag-2 RNG stamp, then
  // continue without manufacturing a wait frame or loading-screen service.
  c->game->pcSched.completeSyncWait(c->mem_r32(0x1f800138u), /*flag=*/2);
  fieldTransition.areaLoadBd4(c->mem_r8(0x800bf870u), 0); // FUN_80044BD4(0x800452C0, area, 0, 2): destination = bf870
}

void Engine::submode1() {
  Core *c = core;
  uint32_t sm = c->mem_r32(0x1f800138u);
  uint16_t s4c = c->mem_r16(sm + 0x4c);
  if (s4c <= 1) {
    cfg_logf("stage",
             "submode1 case %u: bf870=%u nexttab[bf870]=%u",
             s4c,
             c->mem_r8(0x800bf870u),
             c->mem_r8(0x80108f60u + c->mem_r8(0x800bf870u)));
  }
  switch (s4c) {
  case 0:
    submode1Case0Native();
    /* fallthrough */
  case 1: {
    c->mem_w8(0x1f800234u, 0);
    uint8_t area = c->mem_r8(0x800bf870u);
    uint8_t next = c->mem_r8(0x80108f60u + area);
    c->mem_w16(sm + 0x4c, next);
    break;
  }
  case 2:
    eng(c).fieldRun();
    break; // field RUNNING sub-machine (sm[0x4e]) — native
  case 3:
    eng(c).fieldRunX();
    break; // mid-transition running sub-machine 0x801070b4 — native
  case 4:
    tomba::guest::dispatchHandlerToReturnResuming(*c, 0x80107230u, __func__);
    break;
  case 5:
    tomba::guest::dispatchHandlerToReturnResuming(*c, 0x8010766cu, __func__);
    break;
  case 6:
    tomba::guest::dispatchHandlerToReturnResuming(*c, 0x80107790u, __func__);
    break;
  default:
    break; // >=7: no-op
  }
}

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
