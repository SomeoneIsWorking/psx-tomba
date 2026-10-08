#include "debug/auto_drive.h"

#include "cfg.h"
#include "core.h"
#include "game.h"

#include <cstring>
#include <lucent/log.h>

namespace tomba {
namespace {

constexpr uint32_t kTask0 = 0x801fe000u;
constexpr uint32_t kTaskEntryOffset = 0x0cu;
constexpr uint32_t kCutsceneActive = 0x1f800137u;
constexpr uint16_t kTapCross = 0xbfffu;
constexpr uint16_t kTapStart = 0xfff7u;

bool autoSkipRequested() {
  const char *value = cfg_str("PSXPORT_AUTO_SKIP");
  return value && std::strcmp(value, "0") != 0;
}

} // namespace

void AutoDrive::beforeFrame(Core &core, uint32_t frame) {
  Game &game = *core.game;
  const GameConfig &cfg = *core.cfg;

  if (skipPhase_ == SkipPhase::Uninitialized) {
    skipPhase_ = autoSkipRequested() ? SkipPhase::ReachGame : SkipPhase::Done;
    if (skipPhase_ == SkipPhase::ReachGame) {
      lucent::info("autoskip", "armed: drive into GAME free-roam");
    }
  }

  const uint32_t stage = core.mem_r32(kTask0 + kTaskEntryOffset);
  const bool cutsceneActive = core.mem_r8(kCutsceneActive) != 0;
  switch (skipPhase_) {
  case SkipPhase::ReachGame:
    if (stage != cfg.stageGame) {
      if ((frame % 12u) == 0) {
        game.pad.driveTap(kTapCross, 6);
      }
    }
    break;
  case SkipPhase::AwaitCutscene:
    break;
  case SkipPhase::SkipCutscene:
    if (cutsceneActive) {
      cutsceneIdleFrames_ = 0;
      if ((frame % 40u) == 0) {
        game.pad.driveTap(kTapStart, 6);
      }
    } else if (++cutsceneIdleFrames_ >= 60) {
      game.pad.driveRelease();
      skipPhase_ = SkipPhase::Done;
      lucent::info("autoskip", "free-roam reached at frame {} (cutscene ended)", frame);
    }
    break;
  case SkipPhase::Uninitialized:
  case SkipPhase::Done:
    break;
  }
}

void AutoDrive::afterFrame(Core &core, uint32_t frame) {
  Game &game = *core.game;
  const GameConfig &cfg = *core.cfg;
  const uint32_t stage = core.mem_r32(kTask0 + kTaskEntryOffset);
  const bool cutsceneActive = core.mem_r8(kCutsceneActive) != 0;

  if (skipPhase_ == SkipPhase::ReachGame && stage == cfg.stageGame) {
    skipPhase_ = SkipPhase::AwaitCutscene;
    lucent::info("autoskip", "reached GAME at frame {}", frame);
  } else if (skipPhase_ == SkipPhase::AwaitCutscene && cutsceneActive) {
    skipPhase_ = SkipPhase::SkipCutscene;
    lucent::warn("autoskip", "intro cutscene up at frame {}; skipping (Start)", frame);
  }
}

} // namespace tomba
