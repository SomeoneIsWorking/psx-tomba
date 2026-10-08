#include "frame_driver.h"

#include "context.h"
#include "core.h"
#include "execution_control.h"
#include "execution_exit.h"
#include "game.h"
#include "guest_call.h"
#include "tomba1_runtime.h"

#include <cstdlib>
#include <lucent/log.h>

namespace tomba1 {
namespace {

constexpr std::uint32_t kVblankEventClass = 0xF2000003u;
constexpr std::uint32_t kVblankEventSpec = 2u;
constexpr std::uint32_t kRequestedFields = 0x1F8001EAu;
constexpr std::uint32_t kDrawSyncBeforeVblank = 0x1F8001ECu;
constexpr std::uint32_t kDrawSync = 0x8005EB54u;
constexpr std::uint32_t kResetGraph = 0x8005E694u;
constexpr std::uint32_t kDisplaySwap = 0x80016940u;
constexpr std::uint32_t kTickTaskSleeps = 0x800173B0u;

} // namespace

Tomba1FrameDriver::Tomba1FrameDriver(Game &game, Tomba1Runtime &runtime) : game_(game), runtime_(runtime) {}

void Tomba1FrameDriver::finishMainIteration(Core &core, std::uint32_t fields) {
  if (core.mem_r16(kDrawSyncBeforeVblank) != 0u) {
    psx::cpu::dispatchGuestToReturn1(core, kDrawSync, 0u, psx::cpu::ExecutionBudget::currentTurn(core), __func__);
  }

  for (std::uint32_t field = 0; field < fields; ++field) {
    game_.hle.deliverEvent(kVblankEventClass, kVblankEventSpec);
    game_.spu_audio.frame();
  }

  if (core.mem_r16(kDrawSyncBeforeVblank) == 0u) {
    psx::cpu::dispatchGuestToReturn1(core, kResetGraph, 1u, psx::cpu::ExecutionBudget::currentTurn(core), __func__);
  }

  const std::uint8_t displayState = core.mem_r8(0x1F8001CCu);
  if ((displayState < 2u || displayState == 3u) && core.mem_r16(0x1F8001F0u) < 0x4001u) {
    if (displayState == 3u) {
      core.mem_w8(0x1F8001CCu, 2u);
    }
    psx::cpu::dispatchGuestToReturn0(core, kDisplaySwap, psx::cpu::ExecutionBudget::currentTurn(core), __func__);
    psx::cpu::dispatchGuestToReturn0(core, kTickTaskSleeps, psx::cpu::ExecutionBudget::currentTurn(core), __func__);
  }
}

void Tomba1FrameDriver::stepFrame(Core &core, std::uint32_t frame) {
  if (core.game != &game_ || frame != completedFrames_) {
    lucent::error("tomba1-frame",
                  "invalid frame request: game-bound={} requested={} completed={}",
                  core.game == &game_,
                  frame,
                  completedFrames_);
    std::abort();
  }

  game_.timing.logicFrame = frame;
  game_.timing.frameTick();
  core.rsub.otAttr.beginLogicFrame(frame);
  game_.pad.serviceFrame();

  if (!booted_) {
    // The boot prefix is guest code that creates the first task records, so the task owner is
    // reachable for the duration of it and not one instruction longer.
    const GuestTaskCreationScope creation(tasks_);
    runtime_.bootInit(core);
    booted_ = true;
  }

  core.mem_w16(0x1F8001E8u, 0u);
  if (core.mem_r16(0x1F8001F0u) < 0x4001u) {
    const std::int16_t parity = static_cast<std::int16_t>(core.mem_r16(0x1F8001F4u));
    core.mem_w32(0x8009C8A8u, static_cast<std::uint32_t>(parity * 0x780 + 0x800A1890));
  }

  tasks_.runScheduledTasks(core);

  const std::uint32_t fields = core.mem_r16(kRequestedFields);
  if (fields == 0u || fields > 4u) {
    lucent::error("tomba1-frame", "retail frame requested {} display fields; expected 1..4", fields);
    std::abort();
  }
  finishMainIteration(core, fields);
  // The aspect is a live player setting, and 0x80016B04 states the guest's projection centre exactly
  // once per image, so a run that changed aspect after boot would otherwise widen only the host
  // canvas around an un-widened guest frustum. The owner re-latches here and re-asserts the centre
  // only when it actually differs, so a 4:3 run still writes exactly what 0x80016B04 wrote.
  context(core).widescreen.synchronizePresentation(core);
  game_.presentation.commit(&core, static_cast<int>(fields), game_.temporalPresentation.get());
  ++completedFrames_;
}

} // namespace tomba1
