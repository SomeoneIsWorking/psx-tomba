#pragma once

#include <cstdint>

class Core;

namespace tomba {

// Tomba! 2's title-state-aware automated input (PSXPORT_AUTO_SKIP): this class alone interprets Tomba's
// stage and screen state to decide which pad taps to drive, pressing each screen's own skip button.
class AutoDrive final {
public:
  void beforeFrame(Core &core, uint32_t frame);

private:
  enum class Screen : std::uint8_t {
    Boot,            // START stage
    OpeningMovie,    // DEMO s1: MOVIE/OP.STR, skipped by a held Start
    FrontEnd,        // DEMO menus and pages, confirmed by Cross
    ScriptedOpening, // GAME with the scripted opening running, skipped by Start
    FreeRoam,        // GAME in ordinary play
  };

  static Screen screenOf(Core &core);

  bool armed_ = false;
  bool done_ = false;
  Screen last_ = Screen::Boot;
};

} // namespace tomba
