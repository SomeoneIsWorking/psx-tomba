#pragma once

#include "debug/dev_ap_popup.h"
#include "debug/dev_camera_place.h"
#include "debug/dev_debris.h"
#include "debug/dev_flags.h"
#include "debug/dev_items.h"
#include "debug/dev_warp.h"

#include <cstdint>

class Core;

namespace tomba {

// The debug menu's warp, item, flag, camera re-place, debris and AP popup requests: armed between frames and written by
// the frame driver at its next frame boundary.
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
  DevCameraPlace &cameraPlace() {
    return cameraPlace_;
  }
  DevDebris &debris() {
    return debris_;
  }
  DevApPopup &apPopup() {
    return apPopup_;
  }
  void applyArmed(Core &core, uint32_t frame);

private:
  static void giveAndFlag(Core &core, const Inventory::ItemGrantArgs &grant);

  DevWarp warp_;
  DevItems items_;
  DevFlags flags_;
  DevCameraPlace cameraPlace_;
  DevDebris debris_;
  DevApPopup apPopup_;
};

} // namespace tomba
