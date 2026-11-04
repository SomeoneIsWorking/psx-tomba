// game/render/camera_lattice.h — the 2048-unit cube of world points that A08's rain and A0B's dust draw from.
//
// Its corner trails the camera position (0x1F8000D2/D6/DA) by 0x400 on each axis, led by half the view's third row
// (0x1F800104..108). A node seeds an LCG at +0x50 and sits at +0x2C; each point is three LCG draws offset by the node's
// distance from the corner and wrapped into the cube, so a point stays put in the world until the cube wraps past it.
#pragma once

#include "emit_memory.h"

#include <cstdint>

namespace tomba::render {

class CameraLattice {
public:
  static constexpr std::uint32_t kCameraMatrix = 0x1F8000F8u; // MATRIX, the scene camera
  static constexpr std::uint32_t kCameraBytes = 0x20u;
  static constexpr std::uint32_t kCameraPosition = 0x1F8000D2u; // s16 x at +0, y at +4, z at +8
  static constexpr std::uint32_t kNodeSeed = 0x50u;
  static constexpr std::uint32_t kNodeOrigin = 0x2Cu; // s16 x, y, z
  static constexpr std::uint32_t kExtent = 0x7FFu;
  static constexpr std::uint32_t kCell = 0x800u;

  struct Point {
    std::int16_t x;
    std::int16_t y;
    std::int16_t z;
  };

  // The cube's corner under the current camera.
  static Point corner(const psx::present::EmitMemory &memory);
  struct Offset {
    std::int32_t x;
    std::int32_t y;
    std::int32_t z;
  };

  // The node's position from the corner: what each LCG draw is offset by before it wraps.
  static Offset nodeOffset(const psx::present::EmitMemory &memory, std::uint32_t node, Point corner);
  // Loads the camera rotation and the corner's translation into the GTE, as SetRotMatrix, ApplyRotMatrix into the
  // scratch matrix 0x1F800000's translation plus the camera's, and SetTransMatrix do in the guest.
  static void loadView(const psx::present::EmitMemory &memory, Point corner);
  // One LCG step with the multiplier word at `multiplier`.
  static std::uint32_t step(const psx::present::EmitMemory &memory, std::uint32_t seed, std::uint32_t multiplier);
};

} // namespace tomba::render
