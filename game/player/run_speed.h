// game/player/run_speed.h — Tomba's horizontal speed step FUN_80055FBC and its suit gates.
#pragma once

class Core;

namespace tomba::native {
class NativeOverrideCatalog;
} // namespace tomba::native

namespace tomba {

// G+0x44 is Tomba's signed x speed, driven by the direction byte a1 (low two bits: 0 right, 1 left, 2 and 3 none;
// 8 a held brake). Speeds come from the 8-byte row {accel, step, cap, brake} at 0x800A455C by G+0x6D. Two suit gates:
// the winged suits (0x60, Bird or Squirrel) steer eight times harder in the air instead of twice, and with no suit or
// the Bird (0x30) snow (floor type 0x500) cuts the start to an eighth and the brake to half. Under the
// permanent-suits mod an owned winged suit steers, and any owned suit other than the Bird grips on snow.
class RunSpeed {
public:
  // FUN_80055FBC (a0 Tomba, a1 direction): one frame of x speed.
  static void step(Core *core);

  static void registerOverrides(native::NativeOverrideCatalog &natives);
};

} // namespace tomba
