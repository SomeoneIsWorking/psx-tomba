// game/core/engine_scene_frame.cpp — the per-FRAME field transaction's render half: the scene render-list builder in
// both its native and guest-faithful forms, and the dev-teleport apply the follow camera used to consume here.
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

// Native FUN_8004FE84 — a 2-phase scene/render-list builder driver (struct
// @0x800bf548). base[0] is the phase: 0 -> ARM (snapshot list ptr 0x800ecf64
// into base+0x2b0, +0x2b4 = ptr+0x10, +0x2b8 = ptr+0x10 +
// (*(u16)ptr << 1); base[1]=0, base[0]=1); 1 -> run sub-state base[1]
// (0->0x8004f430, 1->0x8004f474, 2->0x8004f514, 3->0x8004f6d0, >=4 none). After
// the sub-state, set bit0 of flag byte @0x800bf822 when (base[1]!=0 ||
// base[0x0a]!=0) else clear it. phase>=2 is a no-op. Leaf callees stay
// substrate. Faithful to the guest instruction path; a direct child of ov_field_frame (was
// `psx::cpu::callGuestNow(*c, __func__, 0x8004fe84)`).
void Engine::sceneRenderListBuilder() {
  Core *c = core;
  // sceneRenderListBuilderFaithful dispatches through typed runtime address dispatch to substrate
  // leaves (0x8004F430/74/514/6D0, selected by base[1]); those leaves are not
  // proven yield-free, so strict replay check's synchronous compare can observe residual
  // v0/v1 across a yield boundary and misreport it as a divergence (mv_tdd.log
  // 2026-07-08: 0x8004FE84 FAILED (2+ diffs) at v0/v1 — not a real yield-abort,
  // a mismatch abort from this). Plain call, not strict replay check — yields — SBS-gated;
  // re-arm once the leaves are proven yield-free or ported native.
  const uint32_t B = 0x800bf548u;
  uint8_t phase = c->mem_r8(B + 0);
  if (phase == 0) {
    uint32_t p = c->mem_r32(0x800ecf64u);
    c->mem_w32(B + 0x2b0, p);
    c->mem_w32(B + 0x2b4, p + 0x10);
    uint16_t h = c->mem_r16(p);
    c->mem_w8(B + 1, 0);
    c->mem_w8(B + 0, 1);
    c->mem_w32(B + 0x2b8, (p + 0x10) + ((uint32_t)h << 1));
    return;
  }
  if (phase != 1) {
    return; // phase >= 2: epilogue only
  }
  switch (c->mem_r8(B + 1)) {
  case 0:
    psx::cpu::callGuestNow(*c, __func__, 0x8004f430u, B);
    break;
  case 1:
    psx::cpu::callGuestNow(*c, __func__, 0x8004f474u, B);
    break;
  case 2:
    psx::cpu::callGuestNow(*c, __func__, 0x8004f514u, B);
    break;
  case 3:
    psx::cpu::callGuestNow(*c, __func__, 0x8004f6d0u, B);
    break;
  default:
    break; // sub >= 4: no sub-handler (guest j 8004ff60)
  }
  uint32_t flag = 0x800bf822u;
  uint8_t v = c->mem_r8(flag);
  if (c->mem_r8(B + 1) != 0 || c->mem_r16s(B + 0x0a) != 0) {
    c->mem_w8(flag, (uint8_t)(v | 1));
  } else {
    c->mem_w8(flag, (uint8_t)(v & 0xfe));
  }
}

// FIELD PER-FRAME UPDATE 0x80108b0c — native control flow (the field frame's
// work driver, called by the running states of the sm[0x4e] machine). Faithful
// to the disasm: bump *0x1f80017c and *0x800bf878; if NOT paused
// (*0x1f800136==0) run the 11-call gameplay-update block; if *0x1f800136 < 2
// run 0x8003f9a8; then always the render-submit 0x8010810c + 0x80077d8c +
// per-frame area update 0x80075a80. The object-walk 0x8007a904 and display
// 0x80026c88 now run as the NATIVE ov_objwalk / ov_disp_26c88 (direct C calls —
// the previously-orphan bodies wired into the live field frame); the remaining
// callees stay typed runtime address dispatch leaves until owned in turn. NOT yield-free
// (0x8003F9A8 render orchestrator + other callees can scheduler_yield) — the
// earlier "yield-free (transitive jal scan)" claim here was wrong; the fork
// below is a plain call, proven by SBS full, not strict replay check (mv_tdd.log
// 2026-07-08: 0x80108B0C FAILED 10+ diffs — a double-run/rewind artifact, not a
// real bug). pc_faithful field per-frame update — mirror of
// overlay guest 0x80108B0C. Guest frame (sp-24, r16@+16, ra@+20, r16=0x1F800000
// live for callee spills) + jal-site ras on every child. The children run their
// native owners (the ported path — byte-exactness is each owner's own gate);
// the audio-command-queue tail 0x80075A80 is dispatched substrate per the f11
// lib-fallback recipe). NO dualviewSnapshot capture/restore here: with the
// substrate render orchestrator executing underneath (Render::frame), its guest
// writes ARE faithful state — rewinding them would diverge from B. DEV TELEPORT
// (`tp X Y Z`) — write Tomba's master position, once. Called from
// Engine::frameUpdate (game_tomba2.cpp), the one per-frame body
// native_step_frame runs on EVERY exec path.
//
// It is not CutsceneCamera's, because the camera is the WRONG owner: trackXZ
// runs only in the follow-camera mode, so an armed teleport was simply never
// consumed in any area whose camera is in another mode, and the command
// reported success anyway. Measured 2026-08-06 on tomba2_port at HEAD: area 0
// logged one `[tp] Tomba ->` line and Tomba moved X 3940 -> 6020; areas 13, 14
// and 20 logged ZERO such lines over 300 frames each and his position never
// changed.
//
// Y and Z are still the game's to overrule — the terrain/collision step
// re-derives ground height and the area's walkable Z after this write (area 0:
// tp Y -1500 settled to -1124, tp Z 2600 settled to 3963). That is the game
// doing its job, not the teleport failing; a dev teleport asks for a position,
// it does not suspend collision.
void Engine::devTeleportApply() {
  if (!mCamTpPending) {
    return;
  }
  Core *c = core;
  mCamTpPending = false;
  c->mem_w32(tomba::camera::kMasterX,
             (uint32_t)mCamTpX << 16); // 16.16 fixed; hi16 = world int
  c->mem_w32(tomba::camera::kMasterY, (uint32_t)mCamTpY << 16);
  c->mem_w32(tomba::camera::kMasterZ, (uint32_t)mCamTpZ << 16);
  c->mem_w32(tomba::camera::kCamGlobal + 0x44, 0); // master speed — land stopped
  lucent::info("tp", "Tomba -> ({},{},{})", mCamTpX, mCamTpY, mCamTpZ);
}
