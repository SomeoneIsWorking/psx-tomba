#include "boot_skip.h"

#include "core.h"
#include "game.h"
#include "guest_task_slots.h"
#include "host_input.h"

#include <lucent/log.h>

namespace tomba1 {
namespace {

constexpr std::uint32_t kSceaCardTask = 0x800191E0u;
constexpr std::uint32_t kTitleTask = 0x80019844u;
// Task-record words both tasks keep their screen in.
constexpr std::uint32_t kState = 0x48u;
constexpr std::uint32_t kSubState = 0x4Au;
constexpr std::uint32_t kCountdown = 0x58u;
constexpr std::uint32_t kFade = 0x5Au;
// 0x800191E0 state 0xB: the card; sub-state 3 ends it when the fade it counts down reaches 1.
constexpr std::uint16_t kSceaCard = 0xBu;
constexpr std::uint16_t kSceaFadeOut = 3u;
// 0x80019844 state 0 (0x800199B8): sub-state 3 waits out stream 0x15, sub-state 5 holds the logo until the
// countdown ends.
constexpr std::uint16_t kTitleBoot = 0u;
constexpr std::uint16_t kLogoMovie = 3u;
constexpr std::uint16_t kLogoHold = 5u;
// Non-zero while the stream task 0x8001F1C0 plays; its stop request, which it answers with its own teardown.
constexpr std::uint32_t kStreamState = 0x1F8001CCu;
constexpr std::uint32_t kStreamStop = 0x1F8001D3u;
// The buttons 0x80019844 skips the intro movie on.
constexpr std::uint16_t kSkipButtons = psx::input::kButtonStart | psx::input::kButtonCross;

} // namespace

void BootSkip::beforeTasks(Core &core) {
  Game &game = *core.game;
  const auto held = static_cast<std::uint16_t>(~game.pad.buttons & kSkipButtons);
  const bool pressed = (held & ~heldBefore_) != 0u;
  heldBefore_ = held;

  const std::uint32_t task = taskRecordAddress(0);
  const std::uint32_t entry = core.mem_r32(task + kTaskRecordEntry);
  const std::uint16_t state = core.mem_r16(task + kState);
  const bool bootScreens = entry == kSceaCardTask || (entry == kTitleTask && state == kTitleBoot);
  // A press on a screen's load is kept for the screen it loads.
  pending_ = bootScreens && (pending_ || pressed);
  if (!pending_ && !game.session.preview()) {
    return;
  }
  const std::uint16_t sub = core.mem_r16(task + kSubState);
  if (entry == kSceaCardTask && state == kSceaCard) {
    core.mem_w16(task + kSubState, kSceaFadeOut);
    core.mem_w16(task + kFade, 1u);
    lucent::info("bootskip", "SCEA card ends at its fade-out terminal");
  } else if (entry == kTitleTask && state == kTitleBoot && sub == kLogoMovie && core.mem_r8(kStreamState) != 0u &&
             core.mem_r8(kStreamStop) == 0u) {
    core.mem_w8(kStreamStop, 1u);
    lucent::info("bootskip", "logo movie stops through the stream task's teardown");
  } else if (entry == kTitleTask && state == kTitleBoot && sub == kLogoHold) {
    core.mem_w16(task + kCountdown, 1u);
    lucent::info("bootskip", "logo hold ends at its countdown terminal");
  } else {
    return;
  }
  pending_ = false;
}

} // namespace tomba1
