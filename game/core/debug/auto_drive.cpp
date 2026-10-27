#include "debug/auto_drive.h"

#include "core.h"
#include "entry/session.h"
#include "game.h"
#include "host_input.h"
#include "sched/task_slot_layout.h"

#include <cstring>
#include <lucent/log.h>

namespace tomba {
namespace {

constexpr uint32_t kTask0 = sched::kTaskTableBase;
constexpr uint32_t kTaskEntryOffset = sched::kTaskEntry;
constexpr uint32_t kCutsceneActive = 0x1f800137u;
// Pad frames a press is held.
constexpr int kTapFrames = 6;

} // namespace

void AutoDrive::beforeFrame(Core &core, uint32_t frame) {
  Game &game = *core.game;

  if (skipPhase_ == SkipPhase::Uninitialized) {
    skipPhase_ = sessionOf(&core).settings.autoSkip ? SkipPhase::ReachGame : SkipPhase::Done;
    if (skipPhase_ == SkipPhase::ReachGame) {
      lucent::info("autoskip", "armed: drive into GAME free-roam");
    }
  }

  const uint32_t stage = core.mem_r32(kTask0 + kTaskEntryOffset);
  const bool cutsceneActive = core.mem_r8(kCutsceneActive) != 0;
  switch (skipPhase_) {
  case SkipPhase::ReachGame:
    if (stage != sched::kStageGameEntry) {
      if ((frame % 12u) == 0) {
        game.hostInput.padButtons(psx::input::kButtonCross, psx::input::InjectedPress::Tap, kTapFrames);
      }
    }
    break;
  case SkipPhase::AwaitCutscene:
    break;
  case SkipPhase::SkipCutscene:
    if (cutsceneActive) {
      cutsceneIdleFrames_ = 0;
      if ((frame % 40u) == 0) {
        game.hostInput.padButtons(psx::input::kButtonStart, psx::input::InjectedPress::Tap, kTapFrames);
      }
    } else if (++cutsceneIdleFrames_ >= 60) {
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
  const uint32_t stage = core.mem_r32(kTask0 + kTaskEntryOffset);
  const bool cutsceneActive = core.mem_r8(kCutsceneActive) != 0;

  if (skipPhase_ == SkipPhase::ReachGame && stage == sched::kStageGameEntry) {
    skipPhase_ = SkipPhase::AwaitCutscene;
    lucent::info("autoskip", "reached GAME at frame {}", frame);
  } else if (skipPhase_ == SkipPhase::AwaitCutscene && cutsceneActive) {
    skipPhase_ = SkipPhase::SkipCutscene;
    lucent::warn("autoskip", "intro cutscene up at frame {}; skipping (Start)", frame);
  }
}

} // namespace tomba
