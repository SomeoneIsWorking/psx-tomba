#include "debug/auto_drive.h"

#include "core.h"
#include "debug/dev_warp.h"
#include "entry/session.h"
#include "game.h"
#include "host_input.h"
#include "sched/task_slot_layout.h"

#include <lucent/log.h>

namespace tomba {
namespace {

// DEMO's stage state word while s1 plays the opening movie.
constexpr uint16_t kDemoOpeningMovieState = 1;
// Pad frames a press is held, and frames between presses.
constexpr int kTapFrames = 6;
constexpr uint32_t kTapInterval = 40;

constexpr const char *kScreenNames[] = {"boot", "opening movie", "front end", "scripted opening", "free roam"};

} // namespace

AutoDrive::Screen AutoDrive::screenOf(Core &core) {
  const uint32_t task0 = sched::taskSlotBase(0);
  const uint32_t stage = core.mem_r32(task0 + sched::kTaskEntry);
  if (stage == sched::kStageDemoEntry) {
    return core.mem_r16(task0 + sched::kStateMachine) == kDemoOpeningMovieState ? Screen::OpeningMovie
                                                                                : Screen::FrontEnd;
  }
  if (stage == sched::kStageGameEntry) {
    return core.mem_r8(DevWarp::kLoadMode) == DevWarp::kOrdinaryPlay ? Screen::FreeRoam : Screen::ScriptedOpening;
  }
  return Screen::Boot;
}

void AutoDrive::beforeFrame(Core &core, uint32_t frame) {
  if (!armed_) {
    armed_ = true;
    done_ = !sessionOf(&core).settings.autoSkip;
    if (!done_) {
      lucent::info("autoskip", "armed: drive into GAME free-roam");
    }
  }
  if (done_) {
    return;
  }
  const Screen screen = screenOf(core);
  if (screen != last_) {
    lucent::info("autoskip", "{} at frame {}", kScreenNames[static_cast<int>(screen)], frame);
    last_ = screen;
  }
  uint16_t button = 0;
  switch (screen) {
  case Screen::Boot:
  case Screen::FrontEnd:
    button = psx::input::kButtonCross;
    break;
  case Screen::OpeningMovie:
  case Screen::ScriptedOpening:
    button = psx::input::kButtonStart;
    break;
  case Screen::FreeRoam:
    done_ = true;
    return;
  }
  if ((frame % kTapInterval) == 0) {
    core.game->pad.injectButtons(button, psx::input::InjectedPress::Tap, kTapFrames);
  }
}

} // namespace tomba
