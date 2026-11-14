// screen_skip.h — Start ends Tomba! 1's input-less screens through their own terminals.
#pragma once

#include <cstdint>

class Core;

namespace tomba1 {

// The screens before the intro movie take no input in retail: the SCEA card (task 0 at 0x800191E0, state 0xB),
// the WHOOPEE CAMP logo movie (stream 0x15) and its held logo (task 0 at 0x80019844, state 0); Start or Cross arms
// each one's own terminal for the task pass that follows, which the guest then runs, and in a selector preview they
// end unpressed. The intro movie and the title skip on the guest's own check (0x80019844, state > 2).
// The life-lost respawn holds machine 0 0x8001B0A4 state 6 for its countdown (+0x5E, 120 frames) with no pad test;
// Start ends the countdown, and state 6 runs its own terminal: the section's machine (0x8001CF7C) or the reload.
class ScreenSkip {
public:
  // Once per frame, before the scheduled tasks run.
  void beforeTasks(Core &core);

private:
  void bootScreens(Core &core, bool pressed);
  static void respawnHold(Core &core, bool pressed);

  std::uint16_t heldBefore_ = 0;
  bool pending_ = false;
};

} // namespace tomba1
