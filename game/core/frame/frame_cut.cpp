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
  sceneChanged_ = sceneChanged;
  cut_ = sceneChanged || placements_ != 0;
  lucent::debug("cut",
                "cut={} scene={} camera={} stage={:08X} area={} sub={} shot={}",
                cut_,
                sceneChanged,
                placements_,
                current.stage,
                current.area,
                current.subScene,
                current.introShot);
  previous_ = current;
  endedPlacements_ = placements_;
  placements_ = 0;
}

} // namespace tomba
