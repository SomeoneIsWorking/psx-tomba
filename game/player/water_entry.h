// game/player/water_entry.h — where Tomba meets water: swimming with the swim bit, the water fall without it.
#pragma once

class Core;

namespace tomba::native {
class NativeOverrideCatalog;
} // namespace tomba::native

namespace tomba {

// A water floor (attribute bit 0x8000, type 0x100 or 0x200) or an area's water edge is a swim only with the Swimming
// Pig Suit's bit 0x100, asked as SuitAbility::Swim, so an owned suit swims when its setting is on. Without
// it the water is the hazard fall (decomp docs/notes/hazard-falls.md).
class WaterEntry {
public:
  // Tomba_waterLanding 0x800665F4 (a0 Tomba) -> 1 on water: the area's swim start, or mode 4 substate 0xB.
  static void landing(Core *core);
  // A00 TombaTick_hazardFloor 0x8010E408 (a0 Tomba): area 0's floor tick, its water floors and the sea edges of
  // entries 0x1D and 0x1F.
  static void seasideTick(Core *core);

  static void registerOverrides(native::NativeOverrideCatalog &natives);
};

} // namespace tomba
