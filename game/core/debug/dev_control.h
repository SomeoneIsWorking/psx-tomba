#pragma once

#include "debug/dev_flags.h"
#include "debug/dev_items.h"
#include "debug/dev_warp.h"

#include <cstdint>

class Core;

namespace tomba {

// The debug menu's warp, item and flag requests: armed between frames and written by the frame driver at its
// next frame boundary.
class DevControl {
public:
  DevWarp &warp() {
    return warp_;
  }
  DevItems &items() {
    return items_;
  }
  DevFlags &flags() {
    return flags_;
  }
  void applyArmed(Core &core, uint32_t frame);

private:
  static void giveAndFlag(Core &core, const Inventory::ItemGrantArgs &grant);

  DevWarp warp_;
  DevItems items_;
  DevFlags flags_;
};

} // namespace tomba
