// boot_skip.h — Start or Cross ends Tomba! 1's boot screens through their own terminals.
#pragma once

#include <cstdint>

class Core;

namespace tomba1 {

// The screens before the intro movie take no input in retail: the SCEA card (task 0 at 0x800191E0, state 0xB),
// the WHOOPEE CAMP logo movie (stream 0x15) and its held logo (task 0 at 0x80019844, state 0). A press arms each
// one's own terminal for the task pass that follows, which the guest then runs; in a selector preview they end
// unpressed. The intro movie and the title skip on the guest's own check (0x80019844, state > 2).
class BootSkip {
public:
  // Once per frame, before the scheduled tasks run.
  void beforeTasks(Core &core);

private:
  std::uint16_t heldBefore_ = 0;
  bool pending_ = false;
};

} // namespace tomba1
