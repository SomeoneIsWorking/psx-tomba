#include "dev_life_loss.h"

#include "core.h"
#include "dev_warp.h"
#include "guest_call.h"

#include <lucent/log.h>

namespace tomba1 {

std::string DevLifeLoss::refusal(Core &core) {
  const std::uint32_t record = core.mem_r32(DevWarp::kTaskRecordPointer);
  if (record == 0u || core.mem_r16(record + DevWarp::kMachineOffset) != kFieldMachine ||
      core.mem_r16(record + DevWarp::kStateOffset) != DevWarp::kFieldRunning) {
    return "a life loss is legal only while the field machine runs";
  }
  if (core.mem_r8(kPlayer + kActive) != kActiveFree) {
    return lucent::format("the player is not free to be hit (active {})", core.mem_r8(kPlayer + kActive));
  }
  return {};
}

std::string DevLifeLoss::arm(Core &core) {
  std::string why = refusal(core);
  if (!why.empty()) {
    return "refused: " + why;
  }
  armed_ = true;
  return lucent::format("ok: lethal hit armed, vitality {}, lives {}",
                        static_cast<std::int16_t>(core.mem_r16(kPlayer + kVitality)),
                        core.mem_r8(kLives));
}

void DevLifeLoss::applyArmed(Core &core, std::uint32_t frame) {
  if (!armed_) {
    return;
  }
  armed_ = false;
  const std::string why = refusal(core);
  if (!why.empty()) {
    lucent::error("lifeloss", "hit dropped at f{}: {}", frame, why);
    return;
  }
  const std::uint16_t vitality = core.mem_r16(kPlayer + kVitality);
  core.mem_w8(kPlayer + kContactCD, 0u);
  core.mem_w8(kPlayer + kContactCE, 0u);
  psx::cpu::callGuestNow(core, "DevLifeLoss::applyArmed", kDecreaseObjectTimer, kPlayer, vitality & 0xFFu);
  core.mem_w8(kPlayer + kActive, kActiveHurt);
  core.mem_w8(kPlayer + kMode, kModeHurt);
  core.mem_w8(kPlayer + kStep, 0u);
  core.mem_w8(kPlayer + kState, 0u);
  core.mem_w8(kPlayer + kContactKind, 0u);
  lucent::info("lifeloss",
               "lethal hit at f{}: vitality {} -> {}, lives {}",
               frame,
               static_cast<std::int16_t>(vitality),
               static_cast<std::int16_t>(core.mem_r16(kPlayer + kVitality)),
               core.mem_r8(kLives));
}

} // namespace tomba1
