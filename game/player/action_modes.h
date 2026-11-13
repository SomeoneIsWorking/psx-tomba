// game/player/action_modes.h — Tomba's action modes 3, 6 and 0x10, the ones that ask the glide bit mid-jump.
#pragma once

class Core;

namespace tomba::native {
class NativeOverrideCatalog;
} // namespace tomba::native

namespace tomba {

// FUN_80058918 runs G+5's mode each frame; each mode steps its own substate G+6. Modes 3 and 0x10 hold a target
// G+0x10 (sounds 0x20 and 0x21, pose 0x26; mode 3's substate 0xC hands over to mode 0x10); mode 6 is the circle-button
// jump. In each, the Squirrel Clothes' glide bit 0x40, asked through SuitAbilities, keeps the apex latch G+0x144 clear
// after the airborne step (modes 3 and 0x10) or arms the glide at once (mode 6, G+0x178 = 0xC).
class ActionModes {
public:
  static void mode3(Core *core);  // FUN_8005B63C
  static void mode6(Core *core);  // FUN_8005D16C
  static void mode16(Core *core); // FUN_8005C26C

  static void registerOverrides(native::NativeOverrideCatalog &natives);
};

} // namespace tomba
