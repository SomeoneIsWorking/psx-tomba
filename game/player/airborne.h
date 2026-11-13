// game/player/airborne.h — Tomba's airborne step FUN_800574E0, his ledge drop FUN_80056C00 and their suit gates.
#pragma once

#include <cstdint>

class Core;

namespace tomba::native {
class NativeOverrideCatalog;
} // namespace tomba::native

namespace tomba {

// G+7 is the airborne phase: 0 the take-off, 1 the rise, 2 and 3 the fall. Its suit gates ask SuitAbilities: the Bird's
// flutter (0x20), the winged air control (0x60), the Squirrel's glide (0x40), the Pig suits' jump and stomp (0x180).
// With the glide a fall pose turns into the glide-ready pose 0xCA; holding jump there opens the glide 0xCB, which caps
// the fall at 0x300 until its animation ends.
class Airborne {
public:
  // FUN_800574E0 (a0 Tomba, a1 mode): one airborne frame.
  static void step(Core *core);
  // FUN_80056C00 (a0 Tomba, a1 keep grounded): the drop off a ledge before the fall, the Squirrel's softened start.
  static void drop(Core *core);
  // FUN_80055F48 (a0 Tomba, a1 a jump): the take-off speed, the Pig suits' (0x180) slower start.
  static void launch(Core *core);
  // FUN_80057C08 (a0 Tomba, a1 mode): the air moves (the Pig suits' stomp, the air attack) and the landing.
  static void airActions(Core *core);

  static void registerOverrides(native::NativeOverrideCatalog &natives);
};

} // namespace tomba
