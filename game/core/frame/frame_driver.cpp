#include "frame/frame_driver.h"

#include "core.h"
#include "debug/dev_control.h"
#include "entry/tomba_runtime.h"
#include "entry/tomba_session.h"
#include "game.h"
#include "guest_call.h"
#include "hle/libapi_intr.h"
#include "render.h"
#include "render/guest_ordering_table.h"

#include <array>
#include <cstdlib>
#include <lucent/log.h>

namespace tomba {
namespace {

using tomba::render::OrderingTable;
using tomba::render::PacketPool;

// The events FUN_80050B08's pass delivers: root counter 2, VBlank and the CD.
constexpr std::array<uint32_t, 3> kFrameEvents{0xF2000003u, 0xF0000001u, 0xF0000009u};

uint8_t bufferParity(Core &core) {
  return core.mem_r8(kBufferParityAddress);
}

uint32_t currentEnv(Core &core) {
  return core.mem_r32(OrderingTable::kBasePointer);
}

void clearOrderingTable(Core &core, uint32_t env) {
  psx::cpu::dispatchGuestToReturn2(
      core, kClearOTagR, env, kOtEntries, psx::cpu::ExecutionBudget::currentTurn(core), __func__);
}

void putDispEnv(Core &core, uint32_t env) {
  psx::cpu::dispatchGuestToReturn1(
      core, kPutDispEnv, env + kDispEnvOffset, psx::cpu::ExecutionBudget::currentTurn(core), __func__);
}

// Flip the parity and point the env pointer at that buffer, as 0x80050C6C does.
uint32_t flipBuffer(Core &core) {
  const uint8_t parity = static_cast<uint8_t>(1u - bufferParity(core));
  core.mem_w8(kBufferParityAddress, parity);
  const uint32_t env = kDisplayBufferBase + parity * kDisplayBufferStride;
  core.mem_w32(OrderingTable::kBasePointer, env);
  return env;
}

// The pass top after the dwell reset: this pass draws into the pool of the current parity.
void rotatePacketPool(Core &core) {
  core.mem_w32(PacketPool::kLastCursor, core.mem_r32(PacketPool::kCursor));
  core.mem_w32(PacketPool::kCursor, (PacketPool::kBase + bufferParity(core) * PacketPool::kStride) & 0xffffffu);
}

void finishPass(Core &core) {
  const uint32_t env = currentEnv(core);
  switch (static_cast<PresentState>(core.mem_r8(kPresentStateAddress))) {
  case PresentState::Present:
    putDispEnv(core, env);
    psx::cpu::dispatchGuestToReturn1(
        core, kPutDrawEnv, env + kDrawEnvOffset, psx::cpu::ExecutionBudget::currentTurn(core), __func__);
    eng(&core).drawOTag(env + kOtHeadOffset);
    clearOrderingTable(core, flipBuffer(core));
    return;
  case PresentState::Swap:
    putDispEnv(core, env);
    core.mem_w8(kPresentStateAddress, static_cast<uint8_t>(PresentState::Hold));
    flipBuffer(core);
    return;
  case PresentState::Clear:
    clearOrderingTable(core, env);
    return;
  case PresentState::Hold:
    return;
  }
}

} // namespace

TombaFrameDriver::TombaFrameDriver(Game &game) : game_(&game) {}

void TombaFrameDriver::enterLoop(Core &core) {
  for (uint32_t buffer = 0; buffer < 2u; ++buffer) {
    const uint32_t base = kDisplayBufferBase + buffer * kDisplayBufferStride;
    core.otTables.name(kOtTable, base, kOtEntries, sizeof(uint32_t), psx::gpu::OtWalk::HighToLow);
  }
  const uint8_t parity = bufferParity(core);
  core.mem_w32(PacketPool::kCursor, PacketPool::kBase + (1u - parity) * PacketPool::kStride);
  const uint32_t env = kDisplayBufferBase + parity * kDisplayBufferStride;
  core.mem_w32(OrderingTable::kBasePointer, env);
  clearOrderingTable(core, env);
}

void TombaFrameDriver::stepFrame(Core &core, uint32_t frame) {
  if (!core.game || core.game != game_) {
    lucent::error("tomba-frame", "TombaFrameDriver used with a different or unbound Game");
    std::abort();
  }

  if (!cards_.finished()) {
    cards_.step(core);
    return;
  }

  Game &game = *game_;
  TombaSession &session = tombaSession(&core);

  autoDrive_.beforeFrame(core, frame);
  game.timing.logicFrame = frame;
  game.perf.frameBegin();
  game.timing.frameTick();
  LibapiIntr::mirrorHostVblank(core, game.timing.vblank);
  // The engine's own frame-rate transaction opens here: zero the guest's dwell counter (what
  // FUN_80050b08 does at 0x80050C8C at the top of its pass) so the per-field work later in this
  // frame counts against this frame and not the last one. FrameCadence owns the decision; the
  // frame driver only sequences it. The one check that matters is at the bottom.
  eng(&core).cadence().beginLogicFrame();
  rotatePacketPool(core);
  session.callSites.beginFrame();
  for (uint32_t eventClass : kFrameEvents) {
    game.hle.deliverEvent(eventClass, 0xffffffffu);
  }

  game.pad.serviceFrame();
  session.cd.audioTrace("pre");
  game.perf.markPre();

  // The pending capture is consumed before any producer begins this frame. Engine::frameUpdate owns
  // Tomba's pad-edge and per-VBlank audio work; this driver owns the one presentation fence.
  eng(&core).frameUpdate();
  game.perf.phaseBegin(GpuPerf::Phase::Present);
  game.presentation.commit(&core, FrameCadence::kRetailVblanksPerLogicFrame);
  // Packet spans live as long as the record: the OT walked last pass is sealed by this commit.
  core.rsub.otAttr.beginLogicFrame(frame);
  game.perf.phaseEnd(GpuPerf::Phase::Present);

  devControl_.applyArmed(core, frame);
  session.cd.audioTrace("post");
  game.perf.phaseBegin(GpuPerf::Phase::GameLogic);
  session.scheduler.step();
  game.perf.phaseEnd(GpuPerf::Phase::GameLogic);

  eng(&core).musicCoord.tick();
  session.cd.audioTrace("coord");
  psx::cpu::dispatchGuestToReturn1(
      core, kStockLibgpu.drawSync, 0, psx::cpu::ExecutionBudget::currentTurn(core), __func__);
  session.scheduler.tickSleepCountdown();
  finishPass(core);
  eng(&core).frameCut().notePassEnded(core);
  game.perf.frameEnd();
  // Close the frame's cadence transaction, once per frame, with the counts named. This reports on
  // the GUEST's own mechanism, not on the port's: the guest's vsync callback (LAB_800506B4) is what
  // advances the dwell counter, and a frame that ends with it short of the quota means the engine's
  // gate would still be shut. Saying so every frame is the point — a cadence that drifts silently
  // is the defect FrameCadence exists to prevent.
  eng(&core).cadence().endLogicFrame(frame);
  diagnostics_.afterFrame(core, frame);
  autoDrive_.afterFrame(core, frame);
}

} // namespace tomba
