// game/player/freeze_touch.h — area 4's contact loop and the ice monster's freezing touch.
#pragma once

class Core;

namespace tomba::native {
class NativeOverrideCatalog;
} // namespace tomba::native

namespace tomba {

// Area 4's Tomba tick runs one loop over the actors he touches this frame. A kind 0x28 touch (the ice monster)
// freezes him in light clothes (suit word 0x30: no suit or the Bird); heavier suits are knocked back instead. An
// owned heavier suit keeps the freeze off when SuitEffect::FreezeGuard is on.
class FreezeTouch {
public:
  // A04 FUN_8010F1CC (a0 Tomba): the contact loop over *0x1F80013C, count 0x1F800144.
  static void contacts(Core *core);

  static void registerOverrides(native::NativeOverrideCatalog &natives);
};

} // namespace tomba
