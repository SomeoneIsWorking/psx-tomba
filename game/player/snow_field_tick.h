// game/player/snow_field_tick.h — area 4's floor tick and its winged-suit gate.
#pragma once

class Core;

namespace tomba::native {
class NativeOverrideCatalog;
} // namespace tomba::native

namespace tomba {

// Area 4's Tomba tick: entry 7's death floor ends the area, elsewhere a death floor is the fall (substate 5), and off
// hazard floors a winged suit (0x60, SuitAbility::AirControl) with G+0x2A >= 0x27 arms 0x800BF819 = 4, the countdown
// Engine_frameStartTick (0x80059D28) spends wrapping two angle words to 12 bits.
class SnowFieldTick {
public:
  // A04 FUN_8010B024 (a0 Tomba).
  static void tick(Core *core);

  static void registerOverrides(native::NativeOverrideCatalog &natives);
};

} // namespace tomba
