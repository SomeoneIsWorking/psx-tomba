// game/player/pipe_area_tick.h — area 1's (the Pipe Area's) floor tick and the fluttering Bird's sink in one column.
#pragma once

class Core;

namespace tomba::native {
class NativeOverrideCatalog;
} // namespace tomba::native

namespace tomba {

// Area 1's Tomba tick: while event bit 2 of 0x800BFE56 is up, a death floor or a climb above y -0x8FB takes the exit
// FUN_8010C6EC picks; else a death floor is the fall (substate 5), and a fluttering Bird (0x20, SuitAbility::Flutter,
// G+0x179 set) in x 0x28B5..0x2D69 below y -0x1A8F sinks 0x20 a frame.
class PipeAreaTick {
public:
  // A01 FUN_8010C7BC (a0 Tomba).
  static void tick(Core *core);

  static void registerOverrides(native::NativeOverrideCatalog &natives);
};

} // namespace tomba
