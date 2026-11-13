// game/scene/demo.h — class Demo, the DEMO / front-end MENU stage state machine, one per Core.
//
// Owns the substate machine (which of sm[0x48] runs and its field writes); the per-substate system work stays
// dispatched. Reached as `eng(c).demo.stageMain()` / `.frame()`; implementations in demo.cpp.
#pragma once
#include "scene/str_movie.h"

#include <cstdint>
class Core;
class Game;

namespace tomba::native {
class NativeOverrideCatalog;
} // namespace tomba::native

namespace tomba {

class Demo {
public:
  // The title's draws, both sub-machines: the two edge sprites, then the menu items and cursor (a0 menu, a1 item).
  inline static constexpr std::uint32_t kTitleEdges = 0x80106690u;
  inline static constexpr std::uint32_t kTitleItems = 0x80106824u;

  Core *core = nullptr;
  StrMovie openingMovie; // s1's MOVIE/OP.STR

  // Wire s3SubMachine (0x80106AC4) into the override registry
  // (tomba::native::declareOverride) — called from Runtime::registerOverrides through
  // register_engine_overrides(). §9-verified 2026-07-10 (docs/
  // fleet-workflow.md §9): re-checked instruction-by-instruction against
  // authenticated executable/overlay evidence; only defect found was the missing
  // guest-stack-frame mirror (now fixed in demo.cpp).
  static void registerOverrides(native::NativeOverrideCatalog &natives);

  // Called by Scheduler's DEMO stanza.
  void stageMain(); // one-time prologue + s0 (formerly ov_demo_stage_main)
  void frame();     // one per-frame substate dispatch + tail (formerly ov_demo_frame)

  // Substate handlers (called by frame() based on the current sm[0x48]
  // substate).
  void s0(); // formerly ov_demo_s0

  // The pre/post halves of DEMO s0's FUN_80044BD4 call site, around the synchronous preload.
  void s0PreYield();
  void s0PostYield();
  // Case 0 with the host-owned preload completed synchronously.
  void s0Native();
  void s1();      // formerly ov_demo_s1
  void s2();      // formerly ov_demo_s2
  void s3();      // formerly ov_demo_s3
  void s6();      // formerly ov_demo_s6
  void s7Phase(); // formerly ov_demo_s7_phase (a sub-phase of the s7 dispatch)

  // DRAFT (UNWIRED, wide-RE fleet wave) — native port of the main-menu title
  // cursor sub-machine 0x80106AC4 that s3()/demo_frame_s3() still typed runtime address dispatch.
  // Returns v0 semantics identical to the guest function (0/1/2/3); does not
  // itself write sm[0x48] (callers own the transition). See demo.cpp for the
  // full RE + confidence notes. Not called from s3()/demo_frame_s3() yet —
  // those keep typed runtime address dispatch(c, 0x80106ac4u) until this is SBS-verified.
  uint32_t s3SubMachine(); // FUN_80106AC4

  // Native port of the TITLE main-menu cursor sub-machine 0x8010696C that s2()
  // dynamically dispatches — the s3SubMachine twin, minus the Circle/back outcome (the
  // title menu can't cancel). Returns v0 (0/1/2); does not write sm[0x48] (s2()
  // owns the transition). Byte-exact frame (32; r31/r17/r16 @ sp+24/20/16).
  uint32_t s2SubMachine(); // FUN_8010696C

private:
  void s0Body(); // shared s0 case-0 body (see s0Native/s0Faithful above)
};

} // namespace tomba
