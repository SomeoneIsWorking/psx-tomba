// game/player/updraft_contact.h — the updrafts that lift Tomba and the Squirrel's longer reach into them.
#pragma once

class Core;

namespace tomba::native {
class NativeOverrideCatalog;
} // namespace tomba::native

namespace tomba {

// An updraft's extents (+0x84/+0x86) grow by 200, or by 400 in the Squirrel (0x40, SuitAbility::Updraft), while
// Tomba is tested against them, and an overlap kicks him up by twice its depth. A01's steam vents (kind 0x12) and
// A0B's updraft share the lift; in A0B's, 60 frames inside freeze him (hazard substate 6) unless the Evil Flame Pig
// Robe (0x400, SuitAbility::FreezeImmunity) is worn.
class UpdraftContact {
public:
  // A01 FUN_8010D90C (a0 Tomba, a1 the vent): a shape 8 vent pushes him down instead.
  static void steamVent(Core *core);
  // A0B FUN_8010A93C (a0 Tomba, a1 the updraft).
  static void area11Updraft(Core *core);

  static void registerOverrides(native::NativeOverrideCatalog &natives);
};

} // namespace tomba
