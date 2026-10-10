// game/scene/sop.cpp — PC-native ownership of the SOP field-gameplay-mode
// overlay (the per-area scene loop reached once the GAME stage enters
// gameplay). The SOP field-mode machine 0x80109450 (sm[0x50]
// LOAD->FADE->GAMEPLAY) and its per-frame handler are the gameplay-start engine
// systems; this file owns them top-down. Map: scratch/gameplay_start_flow_re.md
// + scratch/sop_mode_re.md.
//
// FIRST OWNED PIECE — the area-DATA load (LAB_80109164, SOP.BIN 0x80109164).
// In the PSX flow, SOP state-0 spawns this as a COOPERATIVE task in slot 1
// (FUN_80044bd4 -> FUN_80051f14(1, LAB_80109164)) and then BLOCKS on the byte
// *0x1f80019b until the task sets it. LAB_80109164 itself does only SYNCHRONOUS
// work — 4 CD reads via FUN_8001dc40 (= cd_dc40, the native synchronous sector
// read), an unpack (FUN_80044e84), a collision-grid load (FUN_80045258, itself
// sync), and an ecf58 reloc-patch loop — then sets *0x1f80019b = 1 and calls
// FUN_80051fb4 (task-complete/yield). We reimplement the BODY natively (the
// leaves stay dispatched as they are already sync) and DROP the task-complete
// yield: the native scheduler marks the slot done. This removes the load's
// cross-frame cooperative dependency (the prerequisite for owning the SOP
// machine as a native per-frame dispatcher) WITHOUT changing the observable
// result — *0x1f80019b ends 1 and ecf58[..] is patched exactly as the guest instruction path
// body leaves it. RE: scratch/sop_mode_re.md + the disasm of 0x80109164
// (faithful below, addresses annotated).

#include "core.h"
#include "core/entry/game_ctx.h"
#include "core/overrides/native_override_catalog.h"
#include "guest_call.h"

#include "cfg.h"
#include "game.h"
#include "sop.h"
#include <algorithm> // std::swap / std::min / std::max used by sceneGridGather
#include <stdio.h>

// dispatch a still-guest leaf with up to 3 args set (helpers for the
// SOP/transition machines).
// (ov_bg_scene_transition_sm moved to BgSceneTransitionSm::step —
// eng(c).bgSceneTransitionSm.step())
#include "camera/cutscene_camera.h" // class CutsceneCamera — SOP/BG cutscene camera (0x8006E3B0)
#include "core/assets/asset.h"      // class Asset — unpackGroup / loadTexgroup (static)
#include "render/screen_fade.h"     // class ScreenFade — the single fade driver
#include "world/graphics_bind.h"    // ov_obj_set_xformblk (FUN_8006CBD0)
#include "world/pool.h"             // ov_pool_init_run (FUN_8007B18C)
#include "world/spawn.h"            // class Spawn (eng(c).spawn.dispatch)

// Named guest-RAM addresses used by the SOP field-mode subsystem. Kept
// file-local — these are SOP-specific handles, not engine-wide primitives.
namespace SopAddr {
// -- scratchpad (SPAD) --
constexpr uint32_t TASK_SM_PTR = 0x1f800138u;     // u32: ptr to the SOP task's state-machine struct (the `sm`
                                                  // base — sm[0x50]=field-mode, sm[0x52]=intro/end-scroller
                                                  // phase, sm[0x60]=startup delay, ...)
constexpr uint32_t IN_FIELD_UPDATE = 0x1f800234u; // u8:  gate — 1 while Sop::fieldUpdate's per-frame body runs,
                                                  // 0 outside (read by leaves that behave differently
                                                  // mid-update)

// -- SOP scene state (main RAM) --
constexpr uint32_t SCENE_ENT_TABLE = 0x800f2418u; // 0x800F2418: SCENE_STATE / entity table (count byte @+6, grid
                                                  // limits @+8/+A, grid ptr @+C, u16 cell-id list @+0x10)
constexpr uint32_t SCENE_BEAT = 0x800bf9b4u;      // u8:  SOP intro-cutscene "beat" (0..N). Beat 5 = narration
                                                  // VOID (pure 2D swirl + text; no 3D world, no BG). Other beats
                                                  // draw the field/BG normally.

// -- BG layer sub-state machine (main RAM) --
constexpr uint32_t BG_LAYER_STATE = 0x800e8008u;  // u8:  state byte for the BG layer SM (0=init → 1=running)
constexpr uint32_t BG_LAYER_SUB = 0x800e806cu;    // u8:  running sub-state (0=snap-follow, 1=reset-to-0)
constexpr uint32_t BG_LAYER_TARGET = 0x800e8040u; // struct: CutsceneCamera snap-follow target

constexpr uint32_t PARALLAX_BG_SM = 0x800ed018u;
} // namespace SopAddr

// Owned synchronous area-DATA load (replaces the body of LAB_80109164
// 0x80109164). Runs in the slot-1 task register context; uses c->r[] for the
// dispatched leaves' args; writes guest RAM.
void Sop::areaLoad() {
  Core *c = core;
  uint32_t sm = c->mem_r32(0x1f800138u);
  c->mem_w8(sm + 0x6e, 3); // sm[0x6e] = 3 (area sub-index; 0x80109198)

  // LOAD 1 — FUN_8001dc40(0x800ef478, *0x800be0f0 + sm[0x6e], 2048)
  // (0x801091ac)
  uint8_t a6e = c->mem_r8(sm + 0x6e); // = 3 (re-read, as the disasm does)
  c->r[4] = 0x800ef478u;
  c->r[5] = c->mem_r32(0x800be0f0u) + a6e;
  c->r[6] = 2048;
  psx::cpu::dispatchGuestToReturn0(*c, 0x8001dc40u, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);

  // LOAD 2 — FUN_8001dc40(0x8018a000, *0x800be0f8 + (*0x800ef478>>11),
  // *0x800ef47c - *0x800ef478)
  uint32_t l2 = c->mem_r32(0x800ef478u); // (0x801091bc)
  c->r[4] = 0x8018a000u;
  tomba::native::retireOverlay(*c, eng(c).activeAreaOverlay);
  c->r[5] = c->mem_r32(0x800be0f8u) + (l2 >> 11);
  c->r[6] = c->mem_r32(0x800ef47cu) - l2;
  psx::cpu::dispatchGuestToReturn0(*c, 0x8001dc40u, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);

  // UNPACK — FUN_80044e84(0x8018a000, 0x1f8000)  (0x801091e4)
  eng(c).asset.unpackGroup(0x8018a000u, 0x001f8000u);

  // LOAD 4 — FUN_8001dc40(0x8018a000, *0x800be100 + (*0x800ef480>>11),
  // *0x800ef484 - *0x800ef480);
  //          *0x800a3ec8 = *0x800ef480>>11  (0x80109210/0x80109214)
  uint32_t l4 = c->mem_r32(0x800ef480u);
  c->mem_w32(0x800a3ec8u, l4 >> 11);
  c->r[4] = 0x8018a000u;
  c->r[5] = c->mem_r32(0x800be100u) + (l4 >> 11);
  c->r[6] = c->mem_r32(0x800ef484u) - l4;
  psx::cpu::dispatchGuestToReturn0(*c, 0x8001dc40u, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);

  // COLLISION GRID — FUN_80045258((area&0xf)<<1, 0x2f)  (0x80109228)
  uint16_t area = c->mem_r16(0x800bf89eu);
  eng(c).asset.loadDescriptorChunk((uint32_t)((area & 0xf) << 1), 0x2f);

  // RELOC PATCH — for i in 0..*0x800ef488: ecf58[w>>24] = 0x8018a000 + (w &
  // 0xffffff), w=*0x800ef48c[i]
  int32_t count = (int32_t)c->mem_r32(0x800ef488u); // (0x80109234; blez skip if <=0)
  for (int32_t i = 0; i < count; i++) {
    uint32_t w = c->mem_r32(0x800ef48cu + (uint32_t)i * 4);
    uint32_t idx = w >> 24;
    uint32_t off = w & 0x00ffffffu;
    c->mem_w32(0x800ecf58u + idx * 4, 0x8018a000u + off);
  }

  // LOAD DONE — *0x1f80019b = 1  (0x80109290). NB: the guest instruction path's FUN_80051fb4
  // task-complete/yield is intentionally DROPPED — the native scheduler marks
  // the slot done after this returns.
  c->mem_w8(0x1f80019bu, 1);
  cfg_logf("stage",
           "[sop] native area-load done: 1f80019b=1, ecf58 patched %d entries "
           "(area&0xf=%u)",
           count,
           (unsigned)(area & 0xf));
}

// Synchronous TRANSITION area-DATA load — replaces the cooperative
// spawn-and-wait of FUN_80044bd4(0x800452c0, *0x800bf870, 0, 2) used by the
// field area machine's state-0 (GAME.BIN 0x80108918). In the PSX flow
// FUN_80044bd4 kills slot 2, clears 1f80019b, spawns 0x800452c0 in slot 1, and
// YIELD-waits on 1f80019b; the task body 0x800452c0 does the load and ends with
// FUN_80051fb4 (task-complete, longjmp-yield). We can NOT typed runtime address dispatch
// 0x800452c0 wholesale because (a) its closing FUN_80051fb4 longjmps out (=
// frame done mid-state, sm[0x4c] never advances) and (b) its CD-settle /
// audio-busy waits would yield. So we transcribe the BODY natively (faithful to
// 0x800452c0), DROP the FUN_80051fb4 task-completes and the settle/busy waits
// (no-ops in our synchronous CD/audio runtime), and typed runtime address dispatch the leaf
// callees (CD read, collision grid, unpack, BGM trigger — all synchronous).
// Ends by writing 1f80019b=1, exactly as the guest instruction path leaves it. Mirrors
// native_sop_area_load for the SOP intro load.
void Sop::transitionAreaLoad() {
  Core *c = core;
  uint32_t sm = c->mem_r32(0x1f800138u);
  cfg_logf("stage",
           "[sop] XLOAD enter: sm6d=%u sm6e=%u bf870=%u bf872=%u bf838=%u "
           "bf839=%u bf83a=0x%04X "
           "1f8001ff=%u bfe56=0x%04X 1f800278=0x%04X",
           c->mem_r8(sm + 0x6d),
           c->mem_r8(sm + 0x6e),
           c->mem_r8(0x800bf870u),
           c->mem_r8(0x800bf872u),
           c->mem_r8(0x800bf838u),
           c->mem_r8(0x800bf839u),
           c->mem_r16(0x800bf83au),
           c->mem_r8(0x1f8001ffu),
           c->mem_r16(0x800bfe56u),
           c->mem_r16(0x1f800278u));
  // --- early/quick path test (0x800452d8-0x8004531c): sm[0x6d]==0 AND
  // *1f8001ff==sm[0x6e] AND
  //     (*0x800bfe56 & (1<<sm[0x6e])) == (*0x1f800278 & (1<<sm[0x6e])) ---
  uint8_t s6d = c->mem_r8(sm + 0x6d);
  if (s6d == 0) {
    uint8_t s6e = c->mem_r8(sm + 0x6e);
    uint32_t mask = 1u << s6e;
    uint32_t a2 = (uint32_t)c->mem_r16(0x800bfe56u) & mask;
    uint8_t s1ff = c->mem_r8(0x1f8001ffu);
    uint32_t v0 = (uint32_t)c->mem_r16(0x1f800278u) & mask;
    if (s1ff == s6e && a2 == v0) {
      // QUICK PATH (0x80045320-0x80045344): collision grid + done, no DMA load.
      eng(c).asset.loadDescriptorChunk((uint32_t)((c->mem_r16(0x800bf89eu) & 0xf) << 1), 47);
      c->mem_w8(0x1f800206u, 0);
      c->mem_w8(0x1f80019bu, 1);
      cfg_logf("stage", "[sop] native transition area-load (quick path) done");
      return;
    }
  }
  // --- MAIN LOAD PATH (0x80045350+) ---
  psx::cpu::callGuestNow(*c, __func__, 0x8001cf2cu); // kill slot-2 task / settle CD (sync; settle-wait dropped)
  sm = c->mem_r32(0x1f800138u);
  c->mem_w8(sm + 0x6d, 2); // sm[0x6d] = 2
  uint8_t s6e = c->mem_r8(sm + 0x6e);
  c->mem_w16(0x1f800278u, c->mem_r16(0x800bfe56u)); // *1f800278 = *0x800bfe56
  c->mem_w8(0x1f8001ffu, s6e);                      // *1f8001ff = sm[0x6e]
  c->mem_w8(0x800bf872u, s6e);                      // *0x800bf872 = sm[0x6e]
  // *0x800bf870 = sm[0x6e]  (stored in the jal's DELAY SLOT = the OLD v0, i.e.
  // sm[0x6e], NOT the FUN_80045080 return); then FUN_80045080(0x80108f9c,
  // (sm[0x6e]+3)&0xff) loads the next-area file (its return is discarded).
  c->mem_w8(0x800bf870u, s6e);
  psx::cpu::callGuestNow(*c, __func__, 0x80045080u, 0x80108f9cu, (uint32_t)((s6e + 3) & 0xff));
  // FUN_80045080 has now populated the fixed MODE slot with the selected A00..A0L code image.
  // Publish that exact byte range before any overlay function pointer can be dispatched.
  tomba::native::activateModeOverlay(*c, eng(c).activeModeOverlay, static_cast<uint32_t>(s6e) + 3u);
  // FUN_8007566c(*0x800bf870, *0x1f80022c)   — area BGM/asset trigger
  psx::cpu::callGuestNow(*c, __func__, 0x8007566cu, c->mem_r8(0x800bf870u), c->mem_r32(0x1f80022cu));
  eng(c).asset.loadTexgroup(); // 0x80044F58 — native (sync texgroup load)
  // FUN_8001dc40(0x8018a000, *0x800be100 + (*0x800ef480>>11), *0x800ef484 -
  // *0x800ef480);
  //   *0x800a3ec8 = *0x800ef480>>11    (the area-asset overlay DMA load)
  uint32_t l = c->mem_r32(0x800ef480u);
  c->mem_w32(0x800a3ec8u, l >> 11);
  tomba::native::retireOverlay(*c, eng(c).activeAreaOverlay);
  psx::cpu::callGuestNow(
      *c, __func__, 0x8001dc40u, 0x8018a000u, c->mem_r32(0x800be100u) + (l >> 11), c->mem_r32(0x800ef484u) - l);
  // if (*0x800bf89c == 2) FUN_80045558(0)
  if (c->mem_r8(0x800bf89cu) == 2) {
    tomba::native::loadAreaSlotFile(*c, eng(c).activeAreaOverlay, 0u); // = FUN_80045558(0): OPN
  }
  // FUN_80045258((*0x800bf89e & 0xf)<<1, 47)   — collision grid
  eng(c).asset.loadDescriptorChunk((uint32_t)((c->mem_r16(0x800bf89eu) & 0xf) << 1), 47);
  // RELOC PATCH (0x80045468-0x800454b0): for i in 0..*0x800ef488:
  //   w=*(0x800ef48c + i*4); ecf58[w>>24] = 0x8018a000 + (w & 0xffffff)
  int32_t count = (int32_t)c->mem_r32(0x800ef488u);
  for (int32_t i = 0; i < count; i++) {
    uint32_t w = c->mem_r32(0x800ef48cu + (uint32_t)i * 4);
    c->mem_w32(0x800ecf58u + (w >> 24) * 4, 0x8018a000u + (w & 0x00ffffffu));
  }
  // tail (0x800454b4-0x80045538): if *0x800bf870 == 8: bonus-area asset bump
  // (0x800ecf94 += 0x1000;
  //   FUN_80045258(idx, 8) by *0x800bf871 bracket); then *0x1f800206 = 1;
  //   *1f80019b = 1.
  if (c->mem_r8(0x800bf870u) == 8) {
    c->mem_w32(0x800ecf94u, c->mem_r32(0x800ecf94u) + 0x1000);
    uint8_t b = c->mem_r8(0x800bf871u);
    uint32_t idx = (b < 9) ? 34 : (b < 16) ? 38 : (b < 21) ? 40 : 36;
    eng(c).asset.loadDescriptorChunk(idx, 8);
    c->mem_w8(0x800bfe60u, (uint8_t)idx);
  }
  c->mem_w8(0x1f800206u, 1);
  c->mem_w8(0x1f80019bu, 1);
  cfg_logf("stage",
           "[sop] native transition area-load (main path) done: 1f80019b=1, "
           "ecf58 patched %d, "
           "bf870=%u",
           count,
           (unsigned)c->mem_r8(0x800bf870u));
}

// sceneGridGather — native port of guest FUN_8010A3AC (Ghidra decomp
// scratch/decomp/ sop_a0e0_a3ac.c). Scanline TRIANGLE-RASTER over a 2D grid:
// given the 3 (X, Y) corners of a frustum-triangle in grid-cell coords, walk
// every cell the triangle covers and append its u16 entry from the grid backing
// store into SCENE_STATE's u16 list (table+0x10, count byte @+6).
//
// Grid layout (per table pointer @+0xC): a 2-u16 header (X-limit, Y-limit)
// followed by cell entries at +4. Cells are stored COLUMN-MAJOR — cell(x, y) =
// grid_base[x*height + y] — so the inner loop steps horizontally in X by adding
// `height` to the current pointer each iteration. Value 0xFFFF (u16 -1) marks
// an empty cell (skipped).
//
// Faithful to the guest instruction path: same Y-sort of 3 vertices (3 conditional swaps),
// same 16.16 fixed- point edge stepping, same cross-product LEFT/RIGHT edge
// choice, same X-clamp against the grid's X-limit, same Y-guard (skip rows
// outside [0, Y-limit)), same 254-entry cap on the output list. Signed integer
// division matches the MIPS `div` semantics for the ranges the guest instruction path actually
// feeds it (the trap paths at div-by-zero / -0x80000000/-1 are unreachable with
// the values scenePrepass produces).
void Sop::sceneGridGather(uint32_t table, int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t x2, int32_t y2) {
  Core *c = core;
  // Skip degenerate (all-3-Ys-equal) triangle — matches the guest instruction path's early-exit
  // guard.
  if (y1 == y0 && y1 == y2) {
    return;
  }

  // Y-sort three vertices (v0, v1, v2) by Y ascending, tracking their X
  // companions.
  int32_t v0x = x0, v0y = y0;
  int32_t v1x = x1, v1y = y1;
  int32_t v2x = x2, v2y = y2;
  if (v1y < v0y) {
    std::swap(v0x, v1x);
    std::swap(v0y, v1y);
  }
  if (v2y < v0y) {
    std::swap(v0x, v2x);
    std::swap(v0y, v2y);
  }
  if (v2y < v1y) {
    std::swap(v1x, v2x);
    std::swap(v1y, v2y);
  }

  const int32_t dy01 = v1y - v0y; // short edge Y-span (top half height)
  const int32_t dy02 = v2y - v0y; // long  edge Y-span (full triangle height)

  // Edge accumulators (16.16 fixed): edgeA = "long" v0→v2 edge; edgeB = "short"
  // v0→v1 then v1→v2.
  int32_t edgeA, edgeB, stepA;
  if (dy01 == 0) {
    // Flat-top: no top half; both edges start on the same horizontal line at
    // min(v0x,v1x)..max.
    edgeA = std::max(v0x, v1x) << 16;
    edgeB = std::min(v0x, v1x) << 16;
    stepA = 0;
  } else {
    edgeA = v0x << 16; // both edges begin at v0.X
    edgeB = edgeA;
    stepA = ((v1x - v0x) << 16) / dy01; // slope of v0→v1
  }
  edgeA += 0x10000; // guest instruction path: `iVar5 = iVar5 + 0x10000;` (post-init +1 in 16.16)

  int32_t stepC; // slope of v1→v2 (used in the bottom half)
  const int32_t dy12 = v2y - v1y;
  if (dy12 == 0) {
    stepC = 0;
  } else {
    stepC = ((v2x - v1x) << 16) / dy12;
  }

  const int32_t stepB = ((v2x - v0x) << 16) / dy02; // slope of v0→v2 (always defined; dy02>0 since not degenerate)

  // Cross product picks which edge is LEFT vs RIGHT: `−dy02*(v1x−v0x) +
  // (v2x−v0x)*dy01`.
  const int32_t cross = -dy02 * (v1x - v0x) + (v2x - v0x) * dy01;
  const bool crossNegOrZero = (cross < 1); // guest instruction path: `< 1`, i.e. <=0

  const int32_t xLimit = (int32_t)(int16_t)c->mem_r16(table + 8u);  // grid X-limit (clamp)
  const int32_t yLimit = (int32_t)(int16_t)c->mem_r16(table + 10u); // grid Y-limit (row guard)
  const uint32_t grid = c->mem_r32(table + 0xCu) + 4u;              // cell array (skip 2-u16 header)

  auto gatherRow = [&](int32_t row, int32_t edgeR16, int32_t edgeL16) {
    if (row < 0 || row >= yLimit) {
      return;
    }
    int32_t rightX = edgeR16 >> 16;
    int32_t leftX = edgeL16 >> 16;
    if (xLimit <= rightX) {
      rightX = xLimit - 1;
    }
    if (leftX < 0) {
      leftX = 0;
    }
    // Column-major: cell(x, y) at grid + 2*(x*height + y).
    uint32_t cellPtr = grid + 2u * ((uint32_t)leftX * (uint32_t)yLimit + (uint32_t)row);
    for (int32_t x = leftX; x <= rightX; x++) {
      const uint16_t cell = c->mem_r16(cellPtr);
      if (cell != 0xFFFFu) {
        const uint8_t cnt = c->mem_r8(table + 6u);
        if (cnt < 0xFEu) {
          c->mem_w8(table + 6u, (uint8_t)(cnt + 1));
          c->mem_w16(table + 0x10u + (uint32_t)cnt * 2u, cell);
        }
      }
      cellPtr += 2u * (uint32_t)yLimit; // step one column right (column-major)
    }
  };

  // Top half of the triangle: y ∈ [v0y, v1y). Bottom half: y ∈ [v1y, v2y].
  int32_t y = v0y;
  if (crossNegOrZero) {
    // stepA on the LEFT edge (edgeB), stepB on the RIGHT edge (edgeA).
    for (; y < v1y; y++) {
      gatherRow(y, edgeA, edgeB);
      edgeA += stepA;
      edgeB += stepB;
    }
    for (; y <= v2y; y++) {
      gatherRow(y, edgeA, edgeB);
      edgeB += stepB;
      edgeA += stepC;
    }
  } else {
    // Mirrored assignment (stepA/stepC on the right, stepB on the left).
    for (; y < v1y; y++) {
      gatherRow(y, edgeA, edgeB);
      edgeB += stepA;
      edgeA += stepB;
    }
    for (; y <= v2y; y++) {
      gatherRow(y, edgeA, edgeB);
      edgeB += stepC;
      edgeA += stepB;
    }
  }
}

// SOP scene cam-frustum prepass — native ownership of FUN_8010A0E0 (Ghidra
// decomp scratch/decomp/sop_a0e0_a3ac.c). Called every field frame from
// fieldUpdate BEFORE the list-2 walk. Computes 3 view-space rays (forward,
// forward-halfFOV, forward+halfFOV) at fixed view distance 0x5780, pitch-tilts
// them into the ground plane, scales to grid cells (÷0x280), then hands the
// triangle to sceneGridGather (native port of FUN_8010A3AC, above) which
// scanline- rasters it into SCENE_STATE.count / SCENE_STATE.list at
// table+6/+0x10.
//
// Two engine globals are set every call: 0x800A3F90=0x5780 (view distance),
// 0x800A3F94=0x1C7 (half-FOV in 12-bit angle units, ≈40°). The header at
// (table+8, table+0xA) is copied from
// *(u16[2]*)(table+0xC) — the grid width/height limits used by
// sceneGridGather's clamps.
void Sop::scenePrepass(uint32_t table) {
  Core *c = core;
  // Header + engine globals (identical bytes, same order as the guest instruction path).
  const uint32_t hdrPtr = c->mem_r32(table + 0xCu);
  c->mem_w16(table + 8u, c->mem_r16(hdrPtr + 0u));   // grid width limit
  c->mem_w32(0x800A3F90u, 0x5780u);                  // view distance
  c->mem_w16(table + 0xAu, c->mem_r16(hdrPtr + 2u)); // grid height limit
  c->mem_w32(0x800A3F94u, 0x1C7u);                   // half-FOV (12-bit)

  const int32_t camX = (int32_t)c->mem_r16s(0x1F8000D2u);
  const int32_t camZ = (int32_t)c->mem_r16s(0x1F8000DAu);
  const int32_t yaw = (int32_t)c->mem_r16s(0x1F8000F2u); // sign-extended 16-bit view of the 12-bit yaw
  const int32_t pitch = (int32_t)c->mem_r16s(0x1F8000F0u);
  const int32_t D5780 = (int32_t)c->mem_r32(0x800A3F90u); // = 0x5780
  const int32_t D1C7 = (int32_t)c->mem_r32(0x800A3F94u);  // = 0x1C7

  auto trig = [&](uint32_t fn, uint32_t arg) -> int32_t {
    c->r[4] = arg;
    psx::cpu::dispatchGuestToReturn0(*c, fn, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
    return (int32_t)c->r[2];
  };

  // Ray A: 12-bit yaw shifted by -halfFOV
  const uint32_t yA = ((uint32_t)(-yaw - D1C7)) & 0xFFFu;
  const int32_t sinA = trig(0x80083E80u, yA);
  const int32_t X1 = camX + (int32_t)((sinA * D5780) >> 12);
  const int32_t cosA = trig(0x80083F50u, yA);
  const int32_t Z1 = camZ + (int32_t)((cosA * D5780) >> 12);

  // Ray B: 12-bit yaw shifted by +halfFOV (guest instruction path reads D1C7 fresh here —
  // mirror that)
  const uint32_t yB = ((uint32_t)(D1C7 - yaw)) & 0xFFFu;
  const int32_t sinB = trig(0x80083E80u, yB);
  const int32_t X2 = camX + (int32_t)((sinB * D5780) >> 12);
  const int32_t cosB = trig(0x80083F50u, yB);
  const int32_t Z2 = camZ + (int32_t)((cosB * D5780) >> 12);

  // Pitch tilt: |rsin(pitch)| × (rsin(-yaw), rcos(-yaw)) → (ry,rx), scaled
  // ×5×D5780/65536.
  int32_t pAbs = trig(0x80083E80u, (uint32_t)pitch);
  if (pAbs < 0) {
    pAbs = -pAbs;
  }
  const int32_t s2 = trig(0x80083E80u, (uint32_t)(-yaw));
  const int32_t c2 = trig(0x80083F50u, (uint32_t)(-yaw));
  const int32_t ry = ((s2 * pAbs) >> 12) * D5780 * 5 >> 16;
  const int32_t rx = ((c2 * pAbs) >> 12) * D5780 * 5 >> 16;

  // Subtract the pitch drift and rescale to grid cells (÷640 signed).
  const int32_t X0d = (camX - ry) / 0x280;
  const int32_t X1d = (X1 - ry) / 0x280;
  const int32_t X2d = (X2 - ry) / 0x280;
  const int32_t Z0d = (camZ - rx) / 0x280;
  const int32_t Z1d = (Z1 - rx) / 0x280;
  const int32_t Z2d = (Z2 - rx) / 0x280;

  // Reset the SCENE_STATE cell-list count (was the guest instruction path's `sb $zero, 6($a0)`
  // delay-slot write).
  c->mem_w8(table + 6u, 0);

  // Rasterize the frustum triangle into the scene grid — appends cell ids to
  // table+0x10 / +6.
  sceneGridGather(table, X0d, Z0d, X1d, Z1d, X2d, Z2d);
}

// SOP per-frame FIELD UPDATE — native ownership of FUN_801092b4 (decomp
// scratch/decomp/sop/801092b4.c). The running-gameplay frame: a startup-delay
// countdown (sm[0x60]), then BG scene SM + entity update + Tomba update + BG
// layer SM + entity render + GPU submit, then the sm[0x52] intro/end-scroller
// tail. Control flow + every field write owned native; the heavy callees stay
// dynamically dispatched (engine subsystems to own next: entity update 0x8010a0e0 /
// render 0x80109fe0, Tomba update 0x8007b008; and the per-scene content).
// Called from ov_sop_field_mode states 1/2/3.
void Sop::fieldUpdate() {
  Core *c = core;
  using namespace SopAddr;
  uint32_t sm = c->mem_r32(TASK_SM_PTR);
  int16_t delay = c->mem_r16s(sm + 0x60);
  if (delay != 0) {
    c->mem_w16(sm + 0x60,
               (uint16_t)(delay - 1)); // startup delay: just count down
  } else {
    // BG scene transition SM (native, FUN_8002655c) — the intro-cutscene fade
    // manager.
    eng(c).bgSceneTransitionSm.step();

    scenePrepass(SCENE_ENT_TABLE);

    // Tomba/list-2 walk (guest FUN_8007B008): dispatches each list-2 node's
    // +0x1c handler; Tomba is one of those nodes, so this is the top-down layer
    // immediately above Tomba's per-frame tick. Enable `debug behhist` to
    // enumerate the handler addrs firing here (Tomba included).
    eng(c).objectList.walkList2();

    c->mem_w8(IN_FIELD_UPDATE, 1);

    // BG layer sub-SM — init once, then per-frame follow the camera onto
    // BG_LAYER_TARGET.
    uint8_t bg = c->mem_r8(BG_LAYER_STATE);
    if (bg == 0) {
      c->mem_w8(BG_LAYER_STATE, 1);
      c->mem_w8(BG_LAYER_SUB, 0);
    } else if (bg == 1) {
      uint8_t sub = c->mem_r8(BG_LAYER_SUB);
      if (sub == 0) {
        // native CutsceneCamera (was 0x8006e3b0)
        tomba::camera::CutsceneCamera(c, BG_LAYER_STATE).snapFollow(BG_LAYER_TARGET);
      } else if (sub == 1) {
        c->mem_w8(BG_LAYER_SUB, 0);
      }
    }

    eng(c).areaSlots.updateTail(); // 0x80075a80 NATIVE

    // BG DRAW GATE — SOP scene-beat byte SCENE_BEAT selects the current beat
    // within the SOP intro cutscene. Beat 5 is the narration VOID (pure 2D
    // swirl effect + text over black; no 3D world, no BG); for every other beat
    // we tick the parallax BG state SM and draw the BG tiles. See
    // docs/findings/render.md.
    const bool bgVisible = (c->mem_r8(SCENE_BEAT) != 5);
    if (bgVisible) {
      // Parallax BG state machine (native, FUN_8010BFFC) — class ParallaxBg on
      // Engine.
      eng(c).parallaxBg.step();
    }
    psx::cpu::callGuestNow(*c, __func__, 0x80109FE0u, 0x800F2418u);
    psx::cpu::callGuestNow(*c, __func__, 0x8003C048u);
    if (bgVisible) {
      psx::cpu::callGuestNow(*c, __func__, 0x8010c26cu, PARALLAX_BG_SM);
    }
    c->mem_w8(IN_FIELD_UPDATE, 0);
  }
  // tail — sm[0x52]: 0 = intro zone setup, 1 = end-of-area text scroller, 2+ =
  // done
  sm = c->mem_r32(TASK_SM_PTR);
  uint16_t s52 = c->mem_r16(sm + 0x52);
  if (s52 == 1) {
    psx::cpu::callGuestNow(*c, __func__, 0x8010c79cu); // end-of-area scroller
    if (c->r[2] == 0) {
      return; // still running -> stay
    }
  } else {
    if (s52 != 0) {
      return; // s52 >= 2: done
    }
    c->mem_w16(sm + 0x62, 0);
    eng(c).audioDispatch.zoneTransitionSetup(0xE); // native, FUN_8001D71C — zone/area transition setup
  }
  c->mem_w16(sm + 0x52, (uint16_t)(c->mem_r16(sm + 0x52) + 1));
}

// SOP FIELD-MODE MACHINE — native ownership of FUN_80109450 (decomp
// scratch/decomp/sop/80109450.c). Owns the sm[0x50] switch + every field write;
// dispatches the heavy per-state callees (those not yet owned native).
// CRITICAL: state 0 does NOT call FUN_80044bd4 (which clears 1f80019b, spawns
// the slot-1 load task, and yields-waits — fatal to re-enter per-frame). It
// calls native_sop_area_load INLINE. Called from the native bridge
// ov_game_submode0 (per frame) once the GAME loop is native per-frame.
void Sop::fieldMode() {
  Core *c = core;
  uint32_t sm = c->mem_r32(0x1f800138u);
  uint16_t st = c->mem_r16(sm + 0x50);
  switch (st) {
  case 0: { // LOAD — complete host-owned synchronous work in this invocation.
    fade(c).applyLeafCall(0xffffffu, ScreenFade::kSubtractive, 0);
    c->game->pcSched.completeSyncWait(sm, /*flag=*/3);
    eng(c).sop.areaLoad();                                          // INLINE sync load (replaces FUN_80044bd4) ->
                                                                    // 1f80019b=1
    eng(c).pool.init();                                             // 0x8007B18C — native (via LIVE gated entry)
    eng(c).pool.resetControlBlock();                                // 0x800796DC — native (via LIVE gated entry)
    eng(c).pool.finalViewInit();                                    // 0x80078610 — native (via LIVE gated entry)
    psx::cpu::callGuestNow(*c, __func__, 0x8010a8d4u, 0x800f2418u); // SOP bg-ptr setup
    // 3 scene objects: spawn + stamp fields from the SOP overlay tables
    // @0x8010c98c (stride 12).
    for (int i = 0; i < 3; i++) {
      eng(c).spawn.dispatch(/*cls=*/3,
                            /*type=*/3,
                            /*list=*/1); // FUN_8007A980 — native

      uint32_t node = c->r[2];
      uint32_t t = 0x8010c98cu + (uint32_t)i * 12;
      c->mem_w16(node + 0x2e, c->mem_r16(t + 0));
      c->mem_w16(node + 0x32, c->mem_r16(t + 2));
      c->mem_w16(node + 0x36, c->mem_r16(t + 4));
      c->mem_w32(node + 0x1c, c->mem_r32(t + 8)); // per-scene handler (content)
    }
    c->r[4] = 0x800e8008u;
    c->r[5] = 0x8010c95cu;
    eng(c).graphicsBind.setXformBlk();                                     // BG xform setup — native (was 0x8006cbd0)
    tomba::camera::CutsceneCamera(c, 0x800e8008u).snapFollow(0x800e8040u); // BG init (native class CutsceneCamera; was
                                                                           // 0x8006e3b0)
    sm = c->mem_r32(0x1f800138u); // (callees don't move sm, but reload defensively)
    c->mem_w16(sm + 0x50, 1);
    eng(c).pool.reset75240(); // 0x80075240 — native (via LIVE gated entry)
    c->mem_w16(sm + 0x60, 0x1e);
    c->mem_w16(sm + 0x52, 0);
    c->mem_w16(sm + 0x54, 0);
    c->mem_w8(sm + 0x6c, 0x1f);
    c->mem_w8(0x1f800137u, 1);
    break;
  }
  case 1: { // FADE-IN
    uint32_t u = (uint32_t)(c->mem_r8(sm + 0x6c) << 3) & 0xff;
    fade(c).applyLeafCall((u << 16) | (u << 8) | u, ScreenFade::kSubtractive, 0);
    uint8_t nv = (uint8_t)(c->mem_r8(sm + 0x6c) - 1);
    c->mem_w8(sm + 0x6c, nv);
    if (nv == 0) {
      c->mem_w8(sm + 0x6c, 0x1f);
      c->mem_w16(sm + 0x50, (uint16_t)(c->mem_r16(sm + 0x50) + 1)); // advance to state 2 (GAMEPLAY)
    }
    fieldUpdate(); // native per-frame field update
    break;
  }
  case 2: { // GAMEPLAY
    fieldUpdate();
    if (c->mem_r8(0x800bf839u) != 0 || (c->mem_r32(0x800e7e68u) & 8) != 0) {
      c->mem_w16(sm + 0x50, (uint16_t)(c->mem_r16(sm + 0x50) + 1));
    }
    break;
  }
  case 3: { // FADE-OUT
    uint32_t u = ((uint32_t)c->mem_r8(sm + 0x6c) * (uint32_t)-8) & 0xff;
    fade(c).applyLeafCall((u << 16) | (u << 8) | u, ScreenFade::kSubtractive, 0);
    uint8_t nv = (uint8_t)(c->mem_r8(sm + 0x6c) - 1);
    c->mem_w8(sm + 0x6c, nv);
    if (nv == 0) {
      c->mem_w16(sm + 0x50, (uint16_t)(c->mem_r16(sm + 0x50) + 1));
    }
    fieldUpdate();
    break;
  }
  case 4: {                                            // RESET -> next area
    psx::cpu::callGuestNow(*c, __func__, 0x8001cf2cu); // kill load task slot 2 (settle CD)
    c->mem_w8(0x1f800137u, 0);
    int16_t s4c = c->mem_r16s(sm + 0x4c);
    c->mem_w16(sm + 0x4e, 0);
    c->mem_w16(sm + 0x50, 0);
    c->mem_w16(sm + 0x52, 0);
    c->mem_w16(sm + 0x54, 0);
    c->mem_w16(sm + 0x4c, (uint16_t)(s4c + 1));
    c->mem_w8(0x800bf9b4u, 0);
    break;
  }
  default:
    return;
  }
}

// pc_faithful SOP area-load task body — mirror of overlay guest 0x80109164 (see
// sop.h).
void Sop::areaLoadFaithful() {
  Core *c = core;
  c->r[29] -= 32;
  const uint32_t sp = c->r[29];
  c->mem_w32(sp + 24, c->r[18]);
  c->r[18] = 0x800F0000u;
  c->mem_w32(sp + 20, c->r[17]);
  c->r[17] = 0x800EF478u;
  c->mem_w32(sp + 16, c->r[16]);
  c->r[16] = 0x800BE0F0u;
  c->mem_w32(sp + 28, c->r[31]);
  uint32_t sm = c->mem_r32(0x1f800138u);
  c->mem_w8(sm + 110, 3); // sm[0x6E] = file index 3
  c->r[4] = 0x800EF478u;  // 1. SOP descriptor sector
  c->r[5] = c->mem_r32(0x800BE0F0u) + c->mem_r8(c->mem_r32(0x1f800138u) + 110);
  c->r[6] = 2048;
  c->r[31] = 0x801091B4u;
  psx::cpu::dispatchGuestToReturn0(*c, 0x8001DC40u, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
  uint32_t lo = c->mem_r32(0x800EF478u);
  c->r[4] = 0x8018A000u; // 2. compressed archive
  c->r[5] = c->mem_r32(0x800BE0F8u) + (lo >> 11);
  c->r[6] = c->mem_r32(0x800EF47Cu) - lo;
  c->r[31] = 0x801091D8u;
  psx::cpu::dispatchGuestToReturn0(*c, 0x8001DC40u, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
  c->r[4] = 0x8018A000u;
  c->r[5] = 0x001F8000u;
  c->r[31] = 0x801091ECu;
  psx::cpu::dispatchGuestToReturn0(
      *c, 0x80044E84u, psx::cpu::ExecutionBudget::currentTurn(*c), __func__); // 3. unpack -> VRAM
  uint32_t lo2 = c->mem_r32(0x800EF480u);
  c->r[4] = 0x8018A000u; // 4. DAT payload
  c->r[5] = c->mem_r32(0x800BE100u) + (lo2 >> 11);
  c->r[6] = c->mem_r32(0x800EF484u) - lo2;
  c->mem_w32(0x800A3EC8u, lo2 >> 11);
  c->r[31] = 0x80109218u;
  psx::cpu::dispatchGuestToReturn0(*c, 0x8001DC40u, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
  c->r[31] = 0x80109230u;
  eng(c).asset.loadDescriptorChunk(((uint32_t)c->mem_r16(0x800BF89Eu) & 15u) << 1,
                                   47);               // 5. per-area table stamp
  const int32_t n = (int32_t)c->mem_r32(0x800EF488u); // 6. relocation table
  c->r[4] = 0;
  for (int32_t i = 0; i < n; i++) {
    uint32_t word = c->mem_r32(0x800EF48Cu + i * 4);
    c->r[4] += 1;
    c->mem_w32(0x800ECF58u + (word >> 24) * 4, (word & 0x00FFFFFFu) + 0x8018A000u);
  }
  c->mem_w8(0x1F80019Bu, 1); // done_flag -> spawn-and-wait exits
  c->r[31] = 0x8010929Cu;
  c->game->pcSched.selfClose(); // FUN_80051FB4 — does not return on a task
  c->r[31] = c->mem_r32(sp + 28);
  c->r[18] = c->mem_r32(sp + 24);
  c->r[17] = c->mem_r32(sp + 20);
  c->r[16] = c->mem_r32(sp + 16);
  c->r[29] += 32;
}
