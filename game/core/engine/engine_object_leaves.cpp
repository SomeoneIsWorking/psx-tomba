// game/core/engine_object_leaves.cpp — the small per-OBJECT behavior leaves and the GPU-state mutator. Each is a
// resident leaf a behavior handler dispatches to; none of them is part of the frame transaction.
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

void Engine::fieldFrame() {
  Core *c = core;
  // yields — SBS-gated: fieldFrameFaithful() drives the render orchestrator
  // (0x8003F9A8) and other callees that can scheduler_yield; strict replay check only
  // supports yield-free mirrors (strictCheck aborts on yield-while-inCheck) and
  // a rewind+replay of the substrate body here does not reproduce the same
  // interleaving as the live run, so the compare is bogus (10+ byte diffs at
  // 0x801FE8A0.. are an artifact of that double-run, not a real
  // native/substrate mismatch). Byte-exactness for this fork is proven by SBS
  // full (core A vs core B), not strict replay check.
  c->mem_w16(0x1f80017cu,
             (uint16_t)(c->mem_r16(0x1f80017cu) + 1)); // frame counter
  c->mem_w32(0x800bf878u, c->mem_r32(0x800bf878u) + 1);
  if (c->mem_r8(0x1f800136u) == 0) { // not paused: full gameplay update
    eng(c).frameStartTick();
    eng(c).objectList.walkAux(); // 0x80059d28/0x80069b28 NATIVE
    eng(c).array8Dispatch.tick();
    eng(c).objectList.walkAll(); // 0x80026368/0x8007a904 NATIVE
    eng(c).sceneEventFifo();
    eng(c).sceneRenderListBuilder(); // 0x80025588/0x8004fe84 NATIVE (Engine
                                     // methods)
    eng(c).objectTable.dispatch();   // 0x80026c88 NATIVE
    eng(c).modePerFrameDispatch();   // 0x80022a80 NATIVE
                                     // (Engine::modePerFrameDispatch)
    tomba::camera::CutsceneCamera(c, tomba::camera::kCameraObject)
        .update();             // 0x8006ec44 NATIVE (CutsceneCamera::update)
    eng(c).sceneStateStep();   // 0x80050de4 NATIVE (Engine::sceneStateStep)
    eng(c).areaModeDispatch(); // 0x8001cac0 NATIVE (Engine::areaModeDispatch)
  }
  if (c->mem_r8(0x1f800136u) < 2) {
    rend(c)->frame(); // 0x8003f9a8 render orchestrator
  }
  // No restorePre: the OT the orchestrator just filled must survive to drawOTag.
  eng(c).submitPage810c();       // render submit (page-1 dim-fade owned; other pages
                                 // guest instruction path)
  eng(c).postRenderTick();       // 0x80077d8c NATIVE (Engine::postRenderTick)
  eng(c).areaSlots.updateTail(); // 0x80075a80 NATIVE (AreaSlots::updateTail)
}

// -- Small per-object leaves shared across many behavior handlers. Ghidra
// decomp:
//    scratch/decomp/batch_leaves.c
//    ----------------------------------------------------------

// Object fields touched by the animTick/objMatrixCompose/walkStart leaf cluster
// below. Named locally rather than added to game/object/actor.h: several
// offsets are overloaded with a DIFFERENT meaning elsewhere (e.g. obj+0x46 is
// Actor::retryDelay() in another sub-behavior's state — see actor.h) so a
// shared accessor would misname one caller or the other.
namespace ObjAnimField {
constexpr uint32_t kAnimMode = 0x46u;   // u8:  current anim mode (walkStart's dedupe/set field)
constexpr uint32_t kAnimResult = 0x79u; // u8:  animTick's stashed VM return byte
} // namespace ObjAnimField

// FUN_80040CDC is NOT owned here — it is ScriptInterp::init
// (game/scene/script_interp.cpp), the cutscene-script bytecode init (sets
// obj[0x7C]=tableA / obj[0x46]=0xFF, clears 0x10/0x70/0x78, loads the first
// entry, derives obj[0x71] from the op flags). A dead, mis-named duplicate
// lived here as Engine::animEnvInit ("animation-env init") — the fields it
// wrote were the SCRIPT machine's, not an anim env's. It had no callers and was
// registered nowhere; deleted after `codemap.py
// --conflicts` surfaced the dual-ownership (see docs/findings/scene.md).

// Engine::animTick — FUN_8004190C. Ticks the animation VM (native
// Animation::step, which is the full port of FUN_80076D68 — its 3 frame
// sub-leaves stay substrate) and stashes its return byte into obj+0x79. Returns
// 1 (matches guest instruction path v0).
uint32_t Engine::animTick(uint32_t obj) {
  Core *c = core;
  using namespace ObjAnimField;
  // GUEST FRAME MIRROR (abi_extract --contract, single epilogue label -> RAII
  // safe): sp-24; r16@+16, ra@+20; live r16=obj, r31=0x80041920 at the
  // FUN_80076D68 call. stepFramed pushes 76D68's own 40-byte frame exactly like
  // the gen callee does — step() (frameless) left every downstream substrate
  // spill 24+40 bytes high vs core B (SBS watch-cut f218, 2026-07-10).
  static constexpr GuestFrameSpill kSpills[] = {{16, 16}, {31, 20}};
  GuestFrame<24, 2> frame(c, kSpills);
  c->r[16] = obj;
  c->r[31] = 0x80041920u;
  eng(c).animation.stepFramed(obj); // native, mirrors 76D68's frame
  c->mem_w8(obj + kAnimResult, (uint8_t)c->r[2]);
  c->r[2] = 1;
  return 1;
}

// Engine::announcerCue — FUN_8004ED94. `id` sign-extended s16, then times-2
// index into u16 table at *DAT_800BF7FC. That u16 offset is added to base
// *DAT_800BF800, then FUN_8004FA38 fires.
void Engine::announcerCue(uint32_t id, uint8_t flag) {
  Core *c = core;
  const int32_t id32 = (int32_t)(int16_t)(uint16_t)id; // (id << 16) >> 15 in the decomp = sext16 << 1
  const uint32_t idxOff = (uint32_t)(id32 * 2);
  const uint32_t tblPtr = c->mem_r32(0x800BF7FCu);
  const uint16_t entry = c->mem_r16(tblPtr + idxOff);
  const uint32_t base = c->mem_r32(0x800BF800u) + (uint32_t)entry;
  c->r[4] = base;
  c->r[5] = 0xFFFFFFFFu;
  c->r[6] = flag;
  psx::cpu::dispatchGuestToReturn0(
      *c, 0x8004FA38u, psx::cpu::ExecutionBudget::currentTurn(*c), __func__); // announcer-cue queue push (substrate)
}

// FUN_800518FC is NOT owned here — it is NodeXform::buildWithOffset
// (game/render/node_xform.cpp), now the SOLE owner (registered as the
// 0x800518FC override, MIRROR_VERIFY byte-exact to substrate across 23k+
// passes). A duplicate lived here as Engine::objMatrixCompose (same guest fn
// via substrate leaves 0x80085480/84110/84470/51128); it + its 4 SOP-intro
// callers were retired onto buildWithOffset after `codemap.py --conflicts`
// surfaced the dual-ownership (see docs/findings/render.md).

// Engine::walkStart — FUN_80054D14.
uint32_t Engine::walkStart(uint32_t obj, uint32_t mode, int16_t subMode) {
  Core *c = core;
  using namespace ObjAnimField;
  // GUEST FRAME MIRROR (abi_extract --contract, single epilogue label -> RAII
  // safe): sp-32; spills r16@+16, r17@+20, r18@+24, ra@+28; live r16=obj,
  // r17=mode, r18=subMode; r31 constants per call site. The gen spills BEFORE
  // the early-exit test, so the mirror must too: the early-exit path still
  // leaves the caller's ra at sp+28 (SBS watch-cut f747 diverged on exactly
  // that slot when the mirror sat below the test — stale byte vs B's spilled
  // 0x8010A990).
  static constexpr GuestFrameSpill kSpills[] = {{16, 16}, {17, 20}, {18, 24}, {31, 28}};
  GuestFrame<32, 4> frame(c, kSpills);
  c->r[16] = obj;
  c->r[17] = mode;
  c->r[18] = (uint32_t)(int32_t)subMode;
  const uint8_t cur = c->mem_r8(obj + kAnimMode);
  if ((uint32_t)cur == (mode & 0xFFu)) {
    c->r[2] = 0;
    return 0;
  }
  c->mem_w8(obj + kAnimMode, (uint8_t)mode);
  tomba::guest::dispatchJalToReturn(*c, 0x80054790u, 0x80054D58u, obj, mode); // pre-hook (substrate)
  // guest 0x80054D14 passes FOUR args: a3 = subMode (sext16).
  // guest 0x80077CFC consumes it as the anim PHASE SEED (obj+0x0E = a3 +
  // 0x1000) and as the frame-seek arg for the stream decoder
  // (FUN_80075FF8/75F0C a2). Leaving a3 stale seeked the decoder to a garbage
  // frame — Tomba's wrong walk pose + Charles' narration-scene vertex explosion
  // (2026-07-10).
  const uint32_t subMode32 = (uint32_t)(int32_t)subMode;
  if (subMode == 0) {
    tomba::guest::dispatchJalToReturn(*c, 0x80077C40u, 0x80054D78u, obj, 0x80017FE8u, mode, subMode32);
  } else {
    tomba::guest::dispatchJalToReturn(*c, 0x80077CFCu, 0x80054D90u, obj, 0x80017FE8u, mode, subMode32);
  }
  c->r[2] = 1;
  return 1;
}

// Engine::playerGrowthStep moved to ActorTomba::growthStep
// (game/player/actor_tomba.cpp).

// Engine::uploadModeSprites — native ownership of FUN_80067DA8 (Ghidra decomp
// scratch/decomp/fun_80067da8.c). Stages a RECT struct (X=0x1F0, Y=<per-strip>,
// W=0x10, H=1) on the guest stack and hands it to the substrate LoadImage leaf
// FUN_80081218 for each of the 5 mode-selected sprite patterns. Kept as a
// per-frame VRAM upload — no PC-native texture cache hookup yet, since the
// patterns are consumed by still-substrate UI code that reads back VRAM.
void Engine::uploadModeSprites() {
  Core *c = core;
  const uint8_t mode = c->mem_r8(0x800BF88Du);
  uint32_t p0, p1, p2, p3, p4;
  switch (mode) {
  case 0:
    p0 = 0x800A4800u;
    p1 = 0x800A4820u;
    p2 = 0x800A48C0u;
    p3 = 0x800A48E0u;
    p4 = 0x800A4980u;
    break;
  case 1:
    p0 = 0x800A4840u;
    p1 = 0x800A4860u;
    p2 = 0x800A4900u;
    p3 = 0x800A4920u;
    p4 = 0x800A49A0u;
    break;
  case 2:
    p0 = 0x800A4880u;
    p1 = 0x800A48A0u;
    p2 = 0x800A4940u;
    p3 = 0x800A4960u;
    p4 = 0x800A49C0u;
    break;
  default:
    return; // guest instruction path: any other value early-exits
  }

  // Stage the shared RECT on the guest stack (X, Y, W, H = u16 × 4). Y is
  // patched per strip. The guest instruction path allocates a 0x30-byte frame; we mirror it so
  // LoadImage's arg1 pointer + any deep stack use falls in the same window.
  const uint32_t sp_save = c->r[29];
  const uint32_t ra_save = c->r[31];
  c->r[29] = sp_save - 0x30u;
  const uint32_t rect = c->r[29] + 0x10u; // sp+0x10..sp+0x18 = the RECT struct
  c->mem_w16(rect + 0u, 0x1F0);           // X = 496
  c->mem_w16(rect + 4u, 0x10);            // W = 16
  c->mem_w16(rect + 6u, 1);               // H = 1

  auto upload = [&](uint16_t y, uint32_t data) {
    c->mem_w16(rect + 2u, y); // patch Y
    c->r[4] = rect;
    c->r[5] = data;
    psx::cpu::dispatchGuestToReturn0(*c,
                                     0x80081218u,
                                     psx::cpu::ExecutionBudget::currentTurn(*c),
                                     __func__); // LoadImage(rect, data) — substrate leaf
  };
  upload(0x1E2, p0);
  upload(0x1E5, p1);
  upload(0x1C9, p2);
  upload(0x1D0, p3);
  upload(0x1B3, p4);

  c->r[29] = sp_save;
  c->r[31] = ra_save;
}

// Engine::gStateMutate — native ownership of FUN_80058304 (Ghidra decomp
// scratch/decomp/ fieldrun_s2_init.c). See engine.h for the semantics of each
// case. Two guest leaves stay substrate: FUN_8004ED94(id, 0x41) (the
// announcer/UI cue queue) and FUN_800310F4(0x25, 0) (case 1's inventory
// refresh) — neither has a native equivalent yet. Sfx::trigger is native (Sfx
// class) so the alt-cue path routes there directly.
void Engine::gStateMutate(uint32_t G, uint8_t op) {
  Core *c = core;
  auto cue = [&](uint32_t id) {
    announcerCue(id, 0x41);
  }; // native FUN_8004ED94
  const uint8_t f174 = c->mem_r8(G + 0x174u);
  const uint8_t f0D = c->mem_r8(G + 0x0Du);
  uint8_t n174 = f174, n0D = f0D;

  switch (op) {
  case 0:
    if (f174 & 0x08) {
      break; // already-set fast exit
    }
    cue(0x3A);
    n174 = f174 | 0x08;
    n0D = f0D | 0x42;
    c->mem_w8(G + 0x174u, n174);
    c->mem_w8(G + 0x0Du, n0D);
    break;

  case 1: {
    if ((f174 & 0x08) == 0) {
      break;
    }
    c->r[4] = 0x25;
    c->r[5] = 0;
    psx::cpu::dispatchGuestToReturn0(*c, 0x800310F4u, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
    n174 = f174 & 0xF7;
    n0D = f0D & 0xBD;
    c->mem_w8(G + 0x174u, n174);
    c->mem_w8(G + 0x0Du, n0D);
    cue(0x3B);
    break;
  }

  case 2:
    if ((f174 & 0x08) == 0) {
      break;
    }
    cue(0x3B);
    n174 = f174 & 0xF7;
    n0D = f0D & 0xBD;
    c->mem_w8(G + 0x174u, n174);
    c->mem_w8(G + 0x0Du, n0D);
    break;

  case 3:
  case 4: {
    const uint8_t bit = (op == 3) ? 0x20u : 0x10u;
    n174 = f174 | bit;
    n0D = f0D | 0x12;
    c->mem_w8(G + 0x174u, n174);
    c->mem_w8(G + 0x0Du, n0D);
    cue(0x4B);
    c->mem_w8(0x1F800247u, 0);
    break;
  }

  case 5:
    n174 = f174 & 0xCF;
    n0D = f0D & 0xED;
    c->mem_w8(G + 0x174u, n174);
    c->mem_w8(G + 0x0Du, n0D);
    cue(0x4C);
    break;

  case 6:
  case 7: {
    if (op == 6) {
      n174 = f174 | 0x04;
    } else {
      n174 = f174 & 0xFB;
    }
    c->mem_w8(G + 0x174u, n174);
    eng(c).actorTomba.growthStep((op == 6) ? 1 : 0); // native ActorTomba::growthStep (FUN_80057DC0)
    c->mem_w16(0x800BF89Eu, c->mem_r16(G + 0x17Eu));
    cue(op == 6 ? 0x49u : 0x4Au);
    break;
  }

  case 8:
  case 0xD: {
    if (f174 & 0x01) {
      c->mem_w8(0x800BF881u, f174);
      return;
    }
    n174 = (uint8_t)((f174 & 0xFD) | 0x01);
    c->mem_w8(G + 0x174u, n174);
    cue(0x45);
    if (op == 0xD) {
      eng(c).sfx.trigger(0x39, 0, 0); // Sfx::trigger — native (FUN_80074590 alt-cue path)
    } else {
      c->mem_w8(G + 5, 0x38);
      c->mem_w8(G + 6, 2);
    }
    break;
  }

  case 9:
  case 0xE: {
    if (f174 & 0x02) {
      c->mem_w8(0x800BF881u, f174);
      return;
    }
    n174 = (uint8_t)((f174 & 0xFE) | 0x02);
    c->mem_w8(G + 0x174u, n174);
    cue(0x47);
    if (op == 0xE) {
      eng(c).sfx.trigger(0x3A, 0, 0);
    } else {
      c->mem_w8(G + 5, 0x39);
      c->mem_w8(G + 6, 2);
    }
    break;
  }

  case 10: {
    n174 = f174 & 0xFC;
    c->mem_w8(G + 0x174u, n174);
    const uint8_t bf881 = c->mem_r8(0x800BF881u);
    if (bf881 & 0x01) {
      cue(0x46);
    } else if (bf881 & 0x02) {
      cue(0x48);
    }
    break;
  }

  case 0xB: {
    if (f174 & 0x20) {
      cue(0x4C);
    }
    n174 = f174 & 0xDF;
    c->mem_w8(G + 0x174u, n174);
    if ((f174 & 0x10) == 0) {
      n0D = f0D & 0xED;
      c->mem_w8(G + 0x0Du, n0D);
    }
    break;
  }

  case 0xC: {
    c->mem_w8(G + 0x174u, 0);
    n174 = 0;
    c->mem_w8(G + 0x0Du, 0);
    n0D = 0;
    const int16_t f17E = (int16_t)c->mem_r16(G + 0x17Eu);
    if ((uint16_t)f17E & 0x8000u) {
      const uint16_t masked = (uint16_t)(f17E & 0x7FFF);
      c->mem_w16(0x800BF89Eu, masked);
      c->mem_w16(G + 0x17Eu, masked);
    }
    if ((c->mem_r16(G + 0x17Eu) & 0x200u) != 0) {
      c->mem_w8(G + 0x6Fu, 0);
      c->mem_w8(0x800BF88Fu, 0);
      c->mem_w16(G + 0x17Eu, 0x10);
      c->mem_w16(0x800BF89Eu, 0x10);
    }
    break;
  }

  default:
    break; // op >= 0xF: no-op
  }

  c->mem_w8(0x800BF881u,
            n174); // shared tail — mirror the post-mutation G+0x174
}

// FIELD RUNNING sub-machine 0x80106b98 — native control flow + state bodies
// (decomp: scratch/decomp/game/80106b98.c). A 12-way switch on sm[0x4e]; the
// running states call the native ov_field_frame (0x80108b0c) and the heavy leaf
// callees typed runtime address dispatch. NB the guest fall-throughs are faithful: case 2 -> 3,
// case 4 -> 1 (no break). sm[0x4e] >= 12 = no-op. This anchors the field frame
// natively; the leaf callees (object-placement FUN_80072a78 etc.) are the next
// descent. pc_faithful FIELD RUNNING sub-machine — exact mirror of
// overlay guest 0x80106B98 (12 states on sm[0x4e]). Guest frame (sp-24, ra@+20,
// r16@+16, live values) + every leaf dispatched at its RE'd jal site so callee
// spills byte-match core B. overlay guest 0x80108B0C (the field per-frame update)
// runs the native owner Engine::fieldFrame with the gen's r31. Notable gen
// details the rebuilt fieldRun below got WRONG (kept there for native_sync,
// fixed here): 0x1F800194 is a HALFWORD store of u16(0x800E7FEE) (not w32), the
// state-0 area check is mem_r16(0x800BF870)==21 (not r32==0x15), and state 6
// writes 0x800BF870/71 as TWO byte stores of the swapped/masked halves.
