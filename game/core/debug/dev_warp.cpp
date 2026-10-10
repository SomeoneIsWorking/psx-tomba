#include "debug/dev_warp.h"

#include "core.h"
#include "core/engine/task_sm.h"
#include "debug/dev_args.h"
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

std::string DevWarp::arm(Core &core, const char *line) {
  const auto words = DevArgs::words(line);
  uint32_t area = 0;
  uint32_t entry = 0;
  const bool parsed = (words.size() == 2 || words.size() == 3) && DevArgs::decimal(words[1], area) &&
                      (words.size() == 2 || DevArgs::decimal(words[2], entry));
  if (!parsed) {
    return "usage: warp <area> [entry]";
  }
  const int count = kAreaCount;
  if (area >= static_cast<uint32_t>(count)) {
    return lucent::format("refused: area {} is out of range, this game has {} areas (0..{})", area, count, count - 1);
  }
  if (entry > kMaxEntry) {
    return lucent::format("refused: entry {} is out of range (0..{})", entry, kMaxEntry);
  }
  if (phase(core) == Phase::Unavailable) {
    return "refused: a warp is legal only while the field is running";
  }
  area_ = area;
  entry_ = entry;
  armed_ = true;
  return lucent::format("ok: warp armed for area {} entry {}", area_, entry_);
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
  core.mem_w16(kDestination, static_cast<uint16_t>((area_ << 8) | entry_));
  const bool exitCode = TaskSm(&core).stage4c() >= kExitCodeMachine;
  core.mem_w8(kPendingTransition, exitCode ? kHandlerExitRequest : kDoorRequest);
  lucent::info("warp", "area {} entry {} requested at f{}", area_, entry_, frame);
}

} // namespace tomba
