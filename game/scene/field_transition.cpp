// game/scene/field_transition.cpp — PC-native ownership of the sm[0x4a]==5 sub-scene / door / area
// FADE transition state machine. Moved out of game/core/engine.cpp as its own owner (god-file
// slice); see field_transition.h for the full responsibility doc comment. Pure relocation: every
// method body below is byte-identical to its prior Engine:: member.

#include "field_transition.h"

#include "core.h"
#include "core/entry/game_ctx.h" // eng(c) / fade(c)
#include "guest_call.h"          // psx::cpu::callGuestNow / dispatchGuestToReturn0 / ExecutionBudget
#include "render/screen_fade.h"  // class ScreenFade — the single fade driver
#include "sop.h"                 // class Sop — transitionAreaLoad (sync FIELD transition load)

// ---- NATIVE SUB-SCENE / DOOR TRANSITION (FUN_80108a60 + its 4 workers)
// ----------------------------- The sm[0x4a]==5 handler is the field's
// sub-scene / door transition machine (fade-out -> reload the area-DATA for the
// new sub-scene -> fade-in). The user enters a hut here: walking into the door
// sets the zone-change marker, ov_field_run state 1 routes to sm[0x4a]==5, and
// THIS machine performs the swap. Two PSX leaves are owned here instead of
// dispatched: (a) the SCREEN FADE FUN_8007e9c8(color,a1,4) -> engine_fade_set
// (the PC-native engine fade — fixes the "fade missing on hut entry" bug; the
// guest instruction path fade built a PSX OT rect the native renderer no longer draws); (b) the
// cooperative area-load FUN_80044bd4(0x800452c0, area, mode, 1) ->
// native_transition_area_load (sync, no task-spawn/yield — required by the
// native per-frame model, mirrors ov_game_submode1 state 0). Everything else is
// faithful control flow + sm field writes with the remaining leaves
// dynamically dispatched. Decomp: scratch/decomp/game/ transition.c
// (FUN_80108a60/80107afc/80107d3c/80107e20/80107f3c) + scratch/decomp/bd4.c
// (FUN_80044bd4: arg2->sm[0x6e]=area, arg3->sm[0x6d]=mode, clears 1f80019b,
// spawns slot-1 task 0x800452c0).

// Native replacement for FUN_80044bd4(0x800452c0, area, mode, 1): seed the sm
// fields the spawned load reads, clear the load-done flag, run the load
// SYNCHRONOUSLY (native_transition_area_load sets 1f80019b=1). The PSX phase-1
// wait-loop is gone (sync runtime). Faithful to bd4.c's pre-spawn writes.
void FieldTransition::areaLoadBd4(std::uint32_t area, std::uint32_t mode) {
  Core *c = core;
  uint32_t sm = c->mem_r32(0x1f800138u);
  c->mem_w8(sm + 0x6e, (uint8_t)area); // DAT_801fe0de = arg2 (area) == sm[0x6e]
  c->mem_w8(sm + 0x6d, (uint8_t)mode); // DAT_801fe0dd = arg3 (mode) == sm[0x6d]
  c->mem_w8(0x1f80019bu,
            0); // DAT_1f80019b = 0 (load-done flag, set back to 1 by the load)
  // NO RNG draw here: this is the FUN_80044bd4(...,flag=1) call. In
  // guest 0x80044BD4 the flag==1 branch jumps to the epilogue (`if (r19==1)
  // goto L_80044CB8`) BEFORE guest 0x8009A450 — zero RNG draws. Only flag!=1 (see
  // bd4Tail) draws the stamp. guest 0x80051F14 (spawnPrim) draws no RNG either.
  eng(c).sop.transitionAreaLoad();
}

// FUN_80107afc — the MAIN door/sub-scene transition (sm[0x4c]==1..4). sm[0x4e]:
// 0 teardown+fade-clear+load, 1 FADE-OUT (to black), 2 await load, 3 FADE-IN, 4
// done->return to field. Cases 1/2/3 run the per-frame update tail (fade frames
// keep the world ticking). Faithful to transition.c.
void FieldTransition::main() {
  Core *c = core;
  uint32_t sm = c->mem_r32(0x1f800138u);
  uint16_t st = c->mem_r16(sm + 0x4e);
  switch (st) {
  case 0:
    c->mem_w8(0x1f800234u, 1);
    psx::cpu::callGuestNow(*c, __func__, 0x8007a810u);
    psx::cpu::callGuestNow(*c, __func__, 0x800798f8u);
    psx::cpu::callGuestNow(*c, __func__, 0x8007b0f0u);
    psx::cpu::callGuestNow(*c, __func__, 0x801079acu); // scene teardown
    sm = c->mem_r32(0x1f800138u);
    c->mem_w8(sm + 0x6b, 0x1f);
    c->mem_w16(sm + 0x4e, (uint16_t)(c->mem_r16(sm + 0x4e) + 1));
    fade(c).applyLeafCall(0xffffffu, ScreenFade::kSubtractive, 4);
    areaLoadBd4(c->mem_r8(0x800bf870u),
                0); // FUN_80044bd4(0x800452c0,bf870,0,1)
    return;
  case 1: { // FADE-OUT — subtractive ramp
    uint32_t u = (uint32_t)c->mem_r8(sm + 0x6b) & 0x1f;
    fade(c).applyLeafCall((u << 19) | (u << 11) | (u << 3), ScreenFade::kSubtractive, 4);
    uint8_t v = (uint8_t)(c->mem_r8(sm + 0x6b) - 1);
    c->mem_w8(sm + 0x6b, v);
    if (v == 0) {
      c->mem_w16(sm + 0x4e, (uint16_t)(c->mem_r16(sm + 0x4e) + 1));
    }
    break;
  }
  case 2: // await sync load (1f80019b set by load)
    if (c->mem_r8(0x1f80019bu) != 0) {
      c->mem_w8(sm + 0x6b, 0x1f);
      c->mem_w16(sm + 0x4e, (uint16_t)(c->mem_r16(sm + 0x4e) + 1));
    }
    break;
  case 3: { // FADE-IN — additive ramp
    uint32_t u = ((uint32_t)c->mem_r8(sm + 0x6b) * (uint32_t)-8) & 0xff;
    fade(c).applyLeafCall((u << 16) | (u << 8) | u, ScreenFade::kAdditive, 4);
    uint8_t v = (uint8_t)(c->mem_r8(sm + 0x6b) - 1);
    c->mem_w8(sm + 0x6b, v);
    if (v == 0) {
      c->mem_w16(sm + 0x4e, (uint16_t)(c->mem_r16(sm + 0x4e) + 1));
      psx::cpu::callGuestNow(*c, __func__, 0x80050894u, 0);
    }
    break;
  }
  case 4: // done -> back to the field area machine
    psx::cpu::callGuestNow(*c, __func__, 0x80074e48u);
    sm = c->mem_r32(0x1f800138u);
    c->mem_w16(sm + 0x48, 2);
    c->mem_w16(sm + 0x4a, 1);
    c->mem_w16(sm + 0x4c, 1);
    c->mem_w16(sm + 0x4e, 0);
    return;
  default:
    return;
  }
  psx::cpu::callGuestNow(*c, __func__, 0x80059c60u);
  psx::cpu::callGuestNow(*c, __func__, 0x8006ef38u);
  psx::cpu::callGuestNow(*c, __func__, 0x8007b008u);
  psx::cpu::callGuestNow(*c, __func__, 0x8003fa1cu); // per-frame update (fade frames)
}

// FUN_80107d3c — transition variant (sm[0x4c]==5/6). sm[0x4e]: 0 load, 1 effect
// 0x8003fb84, 2 await->done.
void FieldTransition::d3c() {
  Core *c = core;
  uint32_t sm = c->mem_r32(0x1f800138u);
  uint16_t st = c->mem_r16(sm + 0x4e);
  if (st == 1) {
    psx::cpu::callGuestNow(*c, __func__, 0x8003fb84u);
    c->mem_w16(sm + 0x4e, (uint16_t)(c->mem_r16(sm + 0x4e) + 1));
  } else if (st == 0) {
    c->mem_w8(0x1f800234u, 1);
    c->mem_w16(sm + 0x4e, (uint16_t)(c->mem_r16(sm + 0x4e) + 1));
    areaLoadBd4(c->mem_r8(0x800bf870u),
                0); // FUN_80044bd4(0x800452c0,bf870,0,1)
  } else if (st == 2) {
    if (c->mem_r8(0x1f80019bu) == 0) {
      psx::cpu::callGuestNow(*c, __func__, 0x8003ea88u);
    } else {
      c->mem_w16(sm + 0x48, 2);
      c->mem_w16(sm + 0x4a, 1);
      c->mem_w16(sm + 0x4c, 1);
      c->mem_w16(sm + 0x4e, 0);
    }
  }
}

// FUN_80107e20 — transition variant (sm[0x4c]==7). sm[0x4e]: 0 setup+load, 1
// effect 0x8003e264, 2 await->done.
void FieldTransition::e20() {
  Core *c = core;
  uint32_t sm = c->mem_r32(0x1f800138u);
  uint16_t st = c->mem_r16(sm + 0x4e);
  if (st == 1) {
    psx::cpu::callGuestNow(*c, __func__, 0x8003e264u);
    c->mem_w16(sm + 0x4e, (uint16_t)(c->mem_r16(sm + 0x4e) + 1));
  } else if (st == 0) {
    psx::cpu::callGuestNow(*c, __func__, 0x80074bf8u, 9);
    psx::cpu::callGuestNow(*c, __func__, 0x8003e264u);
    c->mem_w16(sm + 0x4e, (uint16_t)(c->mem_r16(sm + 0x4e) + 1));
    c->mem_w8(0x1f800234u, 1);
    areaLoadBd4(c->mem_r8(0x800bf870u),
                0); // FUN_80044bd4(0x800452c0,bf870,0,1)
  } else if (st == 2) {
    psx::cpu::callGuestNow(*c, __func__, 0x8003e894u); // always runs, then checks the flag
    if (c->mem_r8(0x1f80019bu) != 0) {
      psx::cpu::callGuestNow(*c, __func__, 0x80074e48u);
      c->mem_w16(sm + 0x48, 2);
      c->mem_w16(sm + 0x4a, 1);
      c->mem_w16(sm + 0x4c, 1);
      c->mem_w16(sm + 0x4e, 0);
    }
  }
}

// FUN_80107f3c — transition variant (sm[0x4c]==8), a 7-state machine. NB case 0
// uses a DIFFERENT loader: FUN_80044bd4(0x80044f58, (DAT_1f800240+0x1a)&0xff,
// 0) where 0x80044f58 = ov_load_texgroup (a SYNC leaf, also called directly in
// sop.cpp). So we run it inline + set 1f80019b=1 (faithful to the cooperative
// spawn's net effect: texgroup loaded, load-done flag raised). Case 4 uses the
// normal 0x800452c0 loader.
void FieldTransition::f3c() {
  Core *c = core;
  uint32_t sm = c->mem_r32(0x1f800138u);
  uint16_t st = c->mem_r16(sm + 0x4e);
  switch (st) {
  case 0:
    psx::cpu::callGuestNow(*c, __func__, 0x80074bf8u, 9);
    psx::cpu::callGuestNow(*c, __func__, 0x8003e264u);
    c->mem_w8(0x1f800234u, 1);
    c->mem_w16(sm + 0x4e, (uint16_t)(c->mem_r16(sm + 0x4e) + 1));
    // FUN_80044bd4(0x80044f58, (DAT_1f800240+0x1a)&0xff, 0): spawn
    // ov_load_texgroup (sync) -> run inline.
    c->mem_w8(0x1f80019bu, 0);
    psx::cpu::callGuestNow(*c, __func__, 0x80044f58u, (uint32_t)((c->mem_r32(0x1f800240u) + 0x1a) & 0xff));
    c->mem_w8(0x1f80019bu, 1);
    break;
  case 1:
  case 5:
    psx::cpu::callGuestNow(*c, __func__, 0x8003e264u);
    c->mem_w16(sm + 0x4e,
               (uint16_t)(c->mem_r16(sm + 0x4e) + 1)); // LAB_8010809c
    break;
  case 2:
    psx::cpu::callGuestNow(*c, __func__, 0x8003e894u);
    if (c->mem_r8(0x1f80019bu) != 0) {
      c->mem_w16(sm + 0x4e, (uint16_t)(c->mem_r16(sm + 0x4e) + 1));
      psx::cpu::callGuestNow(*c, __func__, 0x80074e48u);
      eng(c).audioDispatch.zoneTransitionSetup(9); // native, FUN_8001D71C
      psx::cpu::callGuestNow(*c, __func__, 0x8003fb94u);
    }
    break;
  case 3:
    psx::cpu::callGuestNow(*c, __func__, 0x8003ebe0u);
    if (c->r[2] == 0) {
      return; // still running -> stay
    }
    psx::cpu::callGuestNow(*c, __func__, 0x8001cf2cu);
    c->mem_w16(sm + 0x4e,
               (uint16_t)(c->mem_r16(sm + 0x4e) + 1)); // LAB_8010809c
    break;
  case 4:
    psx::cpu::callGuestNow(*c, __func__, 0x80074bf8u, 8);
    psx::cpu::callGuestNow(*c, __func__, 0x8003e264u);
    c->mem_w16(sm + 0x4e, (uint16_t)(c->mem_r16(sm + 0x4e) + 1));
    areaLoadBd4(c->mem_r8(0x800bf870u),
                0); // FUN_80044bd4(0x800452c0,bf870,0,1)
    break;
  case 6:
    psx::cpu::callGuestNow(*c, __func__, 0x8003e894u);
    if (c->mem_r8(0x1f80019bu) != 0) {
      psx::cpu::callGuestNow(*c, __func__, 0x80074e48u);
      c->mem_w16(sm + 0x48, 2);
      c->mem_w16(sm + 0x4a, 1);
      c->mem_w16(sm + 0x4c, 1);
      c->mem_w16(sm + 0x4e, 0);
    }
    break;
  default:
    return;
  }
}

// FUN_80108a60 — sm[0x4a]==5 transition dispatcher on sm[0x4c]. 0/9 = done
// (return to the field area machine: sm[0x48]=2, sm[0x4a]=1, sm[0x4c]=0,
// sm[0x4e]=0); 1-4 main; 5/6 d3c; 7 e20; 8 f3c.
void FieldTransition::step() {
  Core *c = core;
  uint32_t sm = c->mem_r32(0x1f800138u);
  uint16_t s4c = c->mem_r16(sm + 0x4c);
  switch (s4c) {
  case 0:
  case 9:
    c->mem_w16(sm + 0x48, 2);
    c->mem_w16(sm + 0x4a, 1);
    c->mem_w16(sm + 0x4c, 0);
    c->mem_w16(sm + 0x4e, 0);
    break;
  case 1:
  case 2:
  case 3:
  case 4:
    main();
    break;
  case 5:
  case 6:
    d3c();
    break;
  case 7:
    e20();
    break;
  case 8:
    f3c();
    break;
  default:
    break;
  }
}
