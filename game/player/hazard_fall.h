// game/player/hazard_fall.h — Tomba's hazard-floor falls: the death floor's substate and the pit void-out mod.
#pragma once

#include <cstdint>

class Core;

namespace tomba::native {
class NativeOverrideCatalog;
} // namespace tomba::native

namespace tomba {

// G+4 == 2 is Tomba's hazard state, dispatched on G+5 by FUN_80067CA4. A hazard floor (floor attribute bit 0x8000)
// enters it from the area's own Tomba tick, or from the hit knockback FUN_80066878: water (0x100/0x200) as
// substate 4 (FUN_8006762C: one point of damage, the fall, the fade, the respawn entry from FUN_800662D8), a death
// floor (0x700) as substate 5 (FUN_800678E8: vitality zeroed, the same fade and respawn pick, then game over). Each
// entry site writes the same G+6 for both, so under the mod substate 5 is substate 4 from its first step.
class HazardFall {
public:
  // FUN_800678E8: substate 5, or substate 4 when ModSettings::pitVoidOut is on.
  static void deathFloorStep(Core *core);

  static void registerOverrides(native::NativeOverrideCatalog &natives);
};

} // namespace tomba
