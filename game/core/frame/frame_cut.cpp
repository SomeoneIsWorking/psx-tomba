#include "core/frame/frame_cut.h"

#include "core.h"

#include <lucent/log.h>

namespace tomba {

void FrameCut::notePassEnded(Core &core) {
  const SceneIdentity current{
      .stage = core.mem_r32(kStagePointer),
      .area = core.mem_r8(kAreaByte),
      .subScene = core.mem_r8(kSubSceneByte),
      .introShot = core.mem_r8(kIntroShotByte),
  };
  const bool sceneChanged = !previous_.has_value() || *previous_ != current;
  cut_ = sceneChanged || cameraPlaced_;
  lucent::debug("cut",
                "cut={} scene={} camera={} stage={:08X} area={} sub={} shot={}",
                cut_,
                sceneChanged,
                cameraPlaced_,
                current.stage,
                current.area,
                current.subScene,
                current.introShot);
  previous_ = current;
  cameraPlaced_ = false;
}

} // namespace tomba
