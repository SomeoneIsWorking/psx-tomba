// game/player/cold_field.h — area 5's cold field: its Tomba tick and the ice crust, and their cold-immunity gates.
#pragma once

class Core;

namespace tomba::native {
class NativeOverrideCatalog;
} // namespace tomba::native

namespace tomba {

// Past Z 0x1928 in area 5 the cold reaches Tomba unless the suit word carries 0x640 (Squirrel Clothes, Clear Fruit,
// Evil Flame Pig Robe), asked as SuitAbility::ColdImmunity. With G+0x177 bit 1 clear the area tick freezes him
// (hazard substate 6, its phase 1 with Cold Powder or no damage left to take); with it set the ice crust actor grows
// on him in up to three layers and breaks off when he leaves the field or gains the immunity.
class ColdField {
public:
  // A05 FUN_8010A854 (a0 Tomba): the area's Tomba tick (water mode, death floor, the cold field).
  static void tick(Core *core);
  // A05 FUN_8012E454 (a0 the crust actor): states 0 armed, 1 growing, 2 breaking, 3 freed.
  static void iceCrust(Core *core);

  static void registerOverrides(native::NativeOverrideCatalog &natives);
};

} // namespace tomba
