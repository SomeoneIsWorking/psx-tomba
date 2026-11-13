// game/player/airborne.h — Tomba's airborne step FUN_800574E0 and the suit gates in it.
#pragma once

#include <cstdint>

class Core;

namespace tomba::native {
class NativeOverrideCatalog;
} // namespace tomba::native

namespace tomba {

// G+7 is the airborne phase: 0 the take-off, 1 the rise, 2 and 3 the fall. Each suit gate asks SuitAbilities, so
// under the permanent-suits mod an owned suit acts as worn: the Bird (0x20) flutter, the Squirrel (0x40) glide, and
// the Pig suits' (0x180) slower rise. With the glide a fall pose turns into the glide-ready pose 0xCA; holding jump
// there opens the glide 0xCB, which caps the fall at 0x300 until its animation ends.
class Airborne {
public:
  // FUN_800574E0 (a0 Tomba, a1 mode): one airborne frame.
  static void step(Core *core);

  static void registerOverrides(native::NativeOverrideCatalog &natives);
};

} // namespace tomba
