#include "debug/dev_subscreen.h"

#include "core.h"
#include "debug/dev_warp.h"
#include "guest_call.h"
#include "ui/items_tab.h"

#include <lucent/log.h>

namespace tomba {
namespace {

constexpr std::uint32_t kStageTask = 0x1F800138u;
constexpr std::uint32_t kTaskPage = 0x6Bu;     // GAME page step: 0 field, 6 the subscreen
constexpr std::uint32_t kTaskPageStep = 0x50u; // the page's own step
constexpr std::uint32_t kPageGate = 0x1F800136u;
constexpr std::uint32_t kPageGateValue = 0x800E7E84u; // what FUN_8010810C's field arm copies into the gate
constexpr std::uint8_t kSubscreenPage = 6u;
constexpr std::uint16_t kSubscreenRunning = 3u;
constexpr std::uint32_t kGiveUpFrames = 120u;

} // namespace

std::string DevSubscreen::arm(std::uint32_t item) {
  item_ = item;
  step_ = Step::Open;
  return lucent::format("ok: subscreen on item {} armed", item);
}

void DevSubscreen::applyArmed(Core &core, std::uint32_t frame) {
  using namespace subscreen;
  if (step_ == Step::Open) {
    const std::uint32_t task = core.mem_r32(kStageTask);
    if (DevWarp::phase(core) != DevWarp::Phase::Running || core.mem_r8(task + kTaskPage) != 0u) {
      step_ = Step::None;
      lucent::error("subscreen", "subscreen dropped at f{}: the field is not running with no page open", frame);
      return;
    }
    // FUN_8010810C's field arm: the page, the gate and the page's step.
    core.mem_w8(task + kTaskPage, kSubscreenPage);
    core.mem_w8(kPageGate, core.mem_r8(kPageGateValue));
    core.mem_w16(task + kTaskPageStep, 0u);
    step_ = Step::Enter;
    armedAt_ = frame;
    lucent::info("subscreen", "subscreen opened at f{}", frame);
    return;
  }
  if (step_ != Step::Enter) {
    return;
  }
  // FUN_80108624 opens the block on its step 2 and runs it from step 3.
  const std::uint32_t task = core.mem_r32(kStageTask);
  if (core.mem_r16(task + kTaskPageStep) != kSubscreenRunning || core.mem_r8(kState + kPhase) != kRunning) {
    if (frame - armedAt_ > kGiveUpFrames) {
      step_ = Step::None;
      lucent::error("subscreen", "subscreen never ran by f{}", frame);
    }
    return;
  }
  step_ = Step::None;
  const std::uint32_t rows = core.mem_r8(kState + kRows);
  for (std::uint32_t row = 0; row < rows; ++row) {
    for (std::uint32_t column = 0; column < 2u; ++column) {
      if (psx::cpu::callGuestNowResult(core, __func__, kItemAt, kState, row, column) != item_) {
        continue;
      }
      const std::uint32_t scroll = row < kVisibleRows ? 0u : row - (kVisibleRows - 1u);
      core.mem_w8(kState + kEntered, 1u);
      core.mem_w8(kState + kStep, 0u);
      core.mem_w8(kState + kRow, static_cast<std::uint8_t>(row));
      core.mem_w8(kState + kColumn, static_cast<std::uint8_t>(column));
      core.mem_w8(kState + kScroll, static_cast<std::uint8_t>(scroll));
      core.mem_w8(kState + kScreenRow, static_cast<std::uint8_t>(row - scroll));
      lucent::info("subscreen", "Items tab on item {} (row {} column {}) at f{}", item_, row, column, frame);
      return;
    }
  }
  lucent::error("subscreen", "item {} is not in the Items tab at f{}", item_, frame);
}

} // namespace tomba
