// class FieldTransition — PC-native ownership of the sm[0x4a]==5 sub-scene / door / area FADE
// transition state machine (guest FUN_80108A60 dispatcher + its 4 FUN_80107xxx workers).
//
// PROPER OOP: one instance per Core, embedded as `Core::engine::fieldTransition`. Back-pointer
// wired once by TombaCtx's constructor (game/core/game_ctx.cpp). Callers reach it through the
// object graph:
//
//     eng(c).fieldTransition.step();          // per-frame entry, was Engine::fieldTransition()
//
// No `extern "C"` shim, no free function, no static, no Core-as-first-arg. Same shape as Demo,
// Sop, SceneTransition, BgSceneTransitionSm.
//
// ---- NATIVE SUB-SCENE / DOOR TRANSITION (FUN_80108a60 + its 4 workers) ------------------------
// The sm[0x4a]==5 handler is the field's sub-scene / door transition machine (fade-out -> reload
// the area-DATA for the new sub-scene -> fade-in). The user enters a hut here: walking into the
// door sets the zone-change marker, ov_field_run state 1 routes to sm[0x4a]==5, and THIS machine
// performs the swap. Two PSX leaves are owned here instead of dispatched: (a) the SCREEN FADE
// FUN_8007e9c8(color,a1,4) -> engine_fade_set (the PC-native engine fade — fixes the "fade missing
// on hut entry" bug; the guest instruction path fade built a PSX OT rect the native renderer no
// longer draws); (b) the cooperative area-load FUN_80044bd4(0x800452c0, area, mode, 1) ->
// areaLoadBd4 (sync, no task-spawn/yield — required by the native per-frame model, mirrors
// ov_game_submode1 state 0). Everything else is faithful control flow + sm field writes with the
// remaining leaves dynamically dispatched. Decomp: scratch/decomp/game/ transition.c
// (FUN_80108a60/80107afc/80107d3c/80107e20/80107f3c) + scratch/decomp/bd4.c (FUN_80044bd4:
// arg2->sm[0x6e]=area, arg3->sm[0x6d]=mode, clears 1f80019b, spawns slot-1 task 0x800452c0).
//
// Formerly the ov_field_transition / ov_transition_main / ov_transition_d3c / ov_transition_e20 /
// ov_transition_f3c free statics in engine.cpp, then Engine::fieldTransition() + its 4 workers.
// Moved to its own owner (engine.cpp god-file slice) with no behavior change: every method here is
// a byte-identical relocation of the prior Engine member body.
#pragma once
#include <cstdint>
class Core;

class FieldTransition {
public:
  Core *core = nullptr;

  // step — the sm[0x4a]==5 per-frame entry (guest FUN_80108A60). Dispatches on sm[0x4c]: 0/9 =
  // done (return to the field area machine: sm[0x48]=2, sm[0x4a]=1, sm[0x4c]=0, sm[0x4e]=0); 1-4
  // main; 5/6 d3c; 7 e20; 8 f3c.
  void step();

private:
  // FUN_80107AFC — the MAIN door/sub-scene transition (sm[0x4c]==1..4). sm[0x4e]: 0
  // teardown+fade-clear+load, 1 FADE-OUT (to black), 2 await load, 3 FADE-IN, 4 done->return to
  // field. Cases 1/2/3 run the per-frame update tail (fade frames keep the world ticking).
  // Faithful to transition.c.
  void main();

  // FUN_80107D3C — transition variant (sm[0x4c]==5/6). sm[0x4e]: 0 load, 1 effect 0x8003fb84, 2
  // await->done.
  void d3c();

  // FUN_80107E20 — transition variant (sm[0x4c]==7). sm[0x4e]: 0 setup+load, 1 effect 0x8003e264,
  // 2 await->done.
  void e20();

  // FUN_80107F3C — transition variant (sm[0x4c]==8), a 7-state machine. NB case 0 uses a
  // DIFFERENT loader: FUN_80044bd4(0x80044f58, (DAT_1f800240+0x1a)&0xff, 0) where 0x80044f58 =
  // ov_load_texgroup (a SYNC leaf, also called directly in sop.cpp). So we run it inline + set
  // 1f80019b=1 (faithful to the cooperative spawn's net effect: texgroup loaded, load-done flag
  // raised). Case 4 uses the normal 0x800452c0 loader.
  void f3c();

  // Native replacement for FUN_80044bd4(0x800452c0, area, mode, 1): seed the sm fields the
  // spawned load reads, clear the load-done flag, run the load SYNCHRONOUSLY
  // (native_transition_area_load sets 1f80019b=1). The PSX phase-1 wait-loop is gone (sync
  // runtime). Faithful to bd4.c's pre-spawn writes. Was the free function `native_area_load_bd4`.
  void areaLoadBd4(std::uint32_t area, std::uint32_t mode);
};
