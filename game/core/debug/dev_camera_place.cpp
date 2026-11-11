#include "debug/dev_camera_place.h"

#include "camera/camera_state.h"
#include "camera/cutscene_camera.h"
#include "core.h"
#include "debug/dev_warp.h"
#include "guest_call.h"

#include <lucent/log.h>

namespace tomba {

std::string DevCameraPlace::arm() {
  armed_ = true;
  return "ok: camera re-place armed";
}

void DevCameraPlace::applyArmed(Core &core, std::uint32_t frame) {
  if (!armed_) {
    return;
  }
  armed_ = false;
  if (DevWarp::phase(core) != DevWarp::Phase::Running) {
    lucent::error("camplace", "camera re-place dropped at f{}: the field is not running", frame);
    return;
  }
  psx::cpu::callGuestNow(core, __func__, camera::CutsceneCamera::kSnapToMaster);
  core.mem_w8(camera::kCameraMode, 0u);
  lucent::info("camplace", "camera re-placed at f{}", frame);
}

} // namespace tomba
