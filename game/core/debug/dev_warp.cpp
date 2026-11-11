#include "debug/dev_warp.h"

#include "core.h"
#include "core/engine/task_sm.h"
#include "debug/dev_gate.h"

#include <lucent/log.h>

namespace tomba {

namespace {

// Field machine states (TaskSm: top, sub-mode, area machine, sub-state).
constexpr uint16_t kRunningTop = 2;
constexpr uint16_t kIntroSubMode = 0;
constexpr uint16_t kFieldSubMode = 1;
// Area machines (sm[0x4c]): 2 is the field run, 3 the mid-transition run, 4..6 the GAME-image area handlers.
constexpr uint16_t kFieldRunMachine = 2;
constexpr uint16_t kExitCodeMachine = 5; // machines 5 and 6 leave on DevWarp::kHandlerExitRequest
constexpr uint16_t kLastAreaMachine = 6;
constexpr uint16_t kFieldRunning = 1;
constexpr uint16_t kOpeningScript = 9;
constexpr uint16_t kAreaChange = 6;
// 0x1F800236: scene-transition type; 1 runs FieldTransition::main (teardown, fade, song stop, load).
constexpr uint32_t kTransitionType = 0x1f800236u;
constexpr uint8_t kFullTransition = 1;

} // namespace

DevWarp::Phase DevWarp::phase(Core &core) {
  if (!DevGate::inGameStage(core)) {
    return Phase::Unavailable;
  }
  const TaskSm sm(&core);
  if (sm.top() != kRunningTop) {
    return Phase::Unavailable;
  }
  const bool opening = core.mem_r8(kLoadMode) == kScriptedOpening;
  const bool fieldMachine = sm.subMode() == kFieldSubMode && sm.stage4c() == kFieldRunMachine;
  if (opening && (sm.subMode() == kIntroSubMode || (fieldMachine && sm.s4e() == kOpeningScript))) {
    return Phase::Opening;
  }
  const bool running =
      sm.subMode() == kFieldSubMode && sm.stage4c() >= kFieldRunMachine && sm.stage4c() <= kLastAreaMachine;
  if (!opening && running && sm.s4e() == kFieldRunning) {
    return Phase::Running;
  }
  return Phase::Unavailable;
}

std::string DevWarp::arm(Core &core, Destination destination) {
  const uint32_t area = destination.area;
  const uint32_t entry = destination.entry;
  const uint32_t entries = AreaEntries::count(core, area);
  if (entries == 0u) {
    return lucent::format("refused: area {} has no entries", area);
  }
  if (entry >= entries) {
    return lucent::format("refused: area {} has entries 0-{}", area, entries - 1u);
  }
  area_ = area;
  entry_ = entry;
  armed_ = true;
  fade_ = false;
  return lucent::format("ok: warp armed for area {} entry {}", area_, entry_);
}

std::string DevWarp::armThroughFade(Core &core, Destination destination) {
  std::string reply = arm(core, destination);
  fade_ = armed_;
  return reply;
}

void DevWarp::applyArmed(Core &core, uint32_t frame) {
  if (!armed_) {
    return;
  }
  armed_ = false;
  const Phase now = phase(core);
  if (now == Phase::Unavailable) {
    lucent::error("warp", "area {} entry {} dropped at f{}: the field is no longer running", area_, entry_, frame);
    return;
  }
  if (now == Phase::Opening) {
    TaskSm sm(&core);
    core.mem_w8(kLoadMode, kOrdinaryPlay);
    core.mem_w8(kTransitionType, kFullTransition);
    sm.setSubMode(kFieldSubMode);
    sm.setStage4c(kFieldRunMachine);
    sm.setS4e(kAreaChange);
  }
  if (fade_) {
    core.mem_w8(kTransitionType, kFullTransition);
  }
  core.mem_w16(kDestination, static_cast<uint16_t>((area_ << 8) | entry_));
  const bool exitCode = TaskSm(&core).stage4c() >= kExitCodeMachine;
  core.mem_w8(kPendingTransition, exitCode ? kHandlerExitRequest : kDoorRequest);
  lucent::info("warp", "area {} entry {} requested at f{}", area_, entry_, frame);
}

} // namespace tomba
