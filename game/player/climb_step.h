// game/player/climb_step.h — Tomba's step along a climbable and the winged suits' boost off a moving one.
#pragma once

class Core;

namespace tomba::native {
class NativeOverrideCatalog;
} // namespace tomba::native

namespace tomba {

// FUN_80062A30 (a0 Tomba, a1 the climbable, a2 the mode bits) moves G+0x156, his place down the climbable, by the
// step table 0x800A4700 (mode bit 1 down, clear up; the held speed G+0x4A for mode bit 2). Past the bottom end of a
// moving one he drops off; off a kind 0x17 rope (A01's pulleys) he takes its velocity, and a winged suit (0x60,
// SuitAbility::AirControl) starts (velocity & 0x1F00) >> 5 lower. Mode 3 only lets go on the ground; mode 0x85 turns
// him round when he lands.
class ClimbStep {
public:
  static void step(Core *core);

  static void registerOverrides(native::NativeOverrideCatalog &natives);
};

} // namespace tomba
