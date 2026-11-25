// game/scene/demo.h — class Demo, the DEMO / front-end MENU stage state machine, one per Core.
//
// Owns the substate machine (which of sm[0x48] runs and its field writes); the per-substate system work stays
// dispatched. Reached as `eng(c).demo.stageMain()` / `.frame()`; implementations in demo.cpp.
#pragma once
#include "scene/str_movie.h"

#include <cstdint>
#include <optional>
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
  // The save Continue loads, found when the title starts (s0).
  std::optional<std::uint8_t> continueFile;

  // Declares the title menu's sub-machine s2SubMachine (0x8010696C).
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
  void s7Phase(); // formerly ov_demo_s7_phase (a sub-phase of the s7 dispatch)

  // The title menu's sub-machine 0x8010696C: Up/Down move the selection (TitleMenu::Item in sm[0x68]), Confirm
  // returns 2, the idle timer returns 1 (attract). Does not write sm[0x48]; the s2 frame owns the transition.
  uint32_t s2SubMachine(); // FUN_8010696C

private:
  void s0Body(); // shared s0 case-0 body (see s0Native/s0Faithful above)
};

} // namespace tomba
