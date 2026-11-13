// An entity's frame as raw sprites (libgpu SPRT) between two draw-mode packets, the shape the resident screen-space
// drawer 0x8004DFA0 and area 4's group 0x8011DA28 write, culled at the record canvas.
#pragma once

#include "screen_cull.h"

#include <cstdint>

class Core;

namespace tomba1 {

class SpriteFrame {
public:
  // The frame named by the entity's animation slot `slot`, its parts offset from (x, y) with y raised by `raise`,
  // linked at the ordering-table entry `link`.
  struct Placement {
    std::uint32_t slot = 0;
    std::uint32_t x = 0;
    std::uint32_t y = 0;
    std::uint32_t raise = 0;
    std::uint32_t link = 0;
  };

  // The guest writes each part's place and size at the cursor before culling it; a copy the guest never draws
  // writes only the parts it keeps.
  enum class Writes : std::uint8_t {
    AsGuest,
    KeptOnly,
  };

  // The tpage halfword of the frame's first part and how many parts were kept.
  struct Drawn {
    std::uint32_t tpage = 0;
    std::uint32_t kept = 0;
  };

  // Every part of the frame the cull keeps.
  static Drawn drawParts(Core &core,
                         std::uint32_t entity,
                         const Placement &at,
                         const psx::present::ScreenCull &cull,
                         Writes writes = Writes::AsGuest);

  // SetDrawMode(packet, 0, 0, tpage, 0) at the cursor, linked at the ordering-table entry `link`.
  struct DrawMode {
    std::uint32_t link = 0;
    std::uint32_t tpage = 0;
  };
  static void linkDrawMode(Core &core, const DrawMode &mode);

  // The resident drawers' shape: a draw mode of tpage 0, the parts, then a draw mode of e+0x1E plus the first
  // part's tpage, all at `at.link`.
  static void draw(Core &core, std::uint32_t entity, const Placement &at, const psx::present::ScreenCull &cull);

  // The cull the 2D drawers' 0x141/0xF1 literals become: the record canvas's columns, 241 rows.
  static psx::present::ScreenCull cullFor(const Core &core);
};

} // namespace tomba1
