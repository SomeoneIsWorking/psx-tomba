// Area 4's backdrop ring: eight 2D column entities whose screen x wraps every 512 columns as the camera moves. The
// guest places a column at x = u or u - 512 from its ring phase u and hides the one whose phase falls in 384..448;
// the canvas draws that column and the ring's other congruent positions too.
#pragma once

#include "mesh_draw.h"
#include "screen_cull.h"

#include <cstdint>
#include <functional>

class Core;

namespace tomba1 {

class BackdropRing {
public:
  // A guest screen position, as the packets carry it.
  struct Point {
    std::uint16_t x = 0;
    std::uint16_t y = 0;
  };

  // A column entity drawn at `at` instead of its own e+0x12/e+0x16.
  using Draw = std::function<void(std::uint32_t entity, Point at)>;

  // Binds the column update 0x8011C894 when `content` is the area 4 overlay; false when refused.
  static bool install(const InstallOverride &install, std::uint64_t content);

  // After the guest draws ring column `entity`, every other ring position reaching `columns`: its own
  // congruent copies, and the hidden column's when `entity` is the first placed column of the pool.
  static void
  drawBeyondPicture(Core &core, std::uint32_t entity, const psx::present::CullWindow &columns, const Draw &draw);
};

} // namespace tomba1
