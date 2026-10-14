#pragma once

struct Core;
class Game;

// Authored child-oscillator loop for the multi-part assembly at 0x8012EB54.
namespace tomba::native {
class NativeOverrideCatalog;
} // namespace tomba::native

namespace tomba {

class SubstateEdgeLeaves {
public:
  // A00 entry 0x801316CC, a0 = assembly node. Bit1 selects paired slots and is
  // re-read after each call: the child can change the remaining iteration count.
  static void tickChildOscillators(Core *core);

  static void registerOverrides(native::NativeOverrideCatalog &natives);
};

} // namespace tomba
