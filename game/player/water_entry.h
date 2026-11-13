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
  // A06 FUN_8010B334 (a0 Tomba): area 6's floor tick; its water swims only once the area flag 0x800BFE56 & 0x40 is set.
  static void area6Tick(Core *core);
  // A08 FUN_8010B328 (a0 Tomba): area 8's floor tick; a swim start keeps a hang (mode 0x2A) or a held mode 3 in place.
  static void area8Tick(Core *core);
  // A0E FUN_8010967C (a0 Tomba): area 14's floor tick, after holding Tomba against the actor he rides.
  static void area14Tick(Core *core);

  static void registerOverrides(native::NativeOverrideCatalog &natives);

private:
  // water_area_ticks.cpp: the overlay areas' floor ticks.
  static void registerAreaTicks(native::NativeOverrideCatalog &natives);
};

} // namespace tomba
