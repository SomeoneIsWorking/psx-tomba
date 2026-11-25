// game/render/ground_footprint.h — the ground grid cells each area draws: the view triangle its scene prepass hands to
// its grid gather, opened to the draw window's width.
#pragma once

#include <cstdint>

class Core;

namespace tomba::native {
class NativeOverrideCatalog;
} // namespace tomba::native

namespace tomba::render {

class GroundFootprint {
public:
  // A triangle corner in grid cells (0x280 world units), as the prepasses store it: x, then z.
  struct Cell {
    std::int32_t x = 0;
    std::int32_t z = 0;
  };

  // Each area's prepass (the scene RUN table, 0x80015A98) casts its two far corners at the camera's yaw -/+ a
  // half-angle sized for the 4:3 frustum (0x800A3F94). A wide canvas widens the frustum's half-width at every depth
  // by (160 + margin) / 160, so the far corners move apart from their midpoint by that ratio, rounded outward.
  static void open(Core *c, Cell &left, Cell &right);

  // Every area's grid gather (A00 FUN_8013F4DC and its copies; a0 table, a1 apex, a2/a3 far corners) opens its
  // triangle, then runs.
  static void registerOverrides(native::NativeOverrideCatalog &natives);
};

} // namespace tomba::render
