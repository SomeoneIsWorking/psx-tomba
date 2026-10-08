#pragma once

#include <cstdint>

class Core;

namespace tomba {

// Tomba! 2's title-state-aware automated input. The framework owns generic REPL/frame budgeting;
// this class alone interprets Tomba's stage and cutscene state to decide which pad taps to drive.
class AutoDrive final {
public:
  void beforeFrame(Core &core, uint32_t frame);
  void afterFrame(Core &core, uint32_t frame);

private:
  enum class SkipPhase {
    Uninitialized,
    ReachGame,
    AwaitCutscene,
    SkipCutscene,
    Done,
  };

  SkipPhase skipPhase_ = SkipPhase::Uninitialized;
  int cutsceneIdleFrames_ = 0;
};

} // namespace tomba
