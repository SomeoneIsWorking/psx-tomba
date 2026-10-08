#include "frame/frame_driver.h"

#include "cfg.h"
#include "core.h"
#include "debug/dev_warp.h"
#include "entry/game_ctx.h"
#include "game.h"
#include "gpu_vk.h"
#include "guest_call.h"
#include "hle/libapi_intr.h"
#include "hw_bind.h"
#include "render.h"

#include <cstdlib>
#include <lucent/log.h>

namespace tomba {
namespace {

const GameConfig &requireMeasuredConfig(Core &core) {
  if (!core.cfg) {
    lucent::error("tomba-frame", "TombaFrameDriver requires Tomba! 2's measured compatibility facts");
    std::abort();
  }
  return *core.cfg;
}

void bindFrameHardware(Core &core) {
  gte_bind(&core);
  if (gpu_vk_wide_engine(&core)) {
    const int ofx = gpu_vk_wide_engine_ofx(&core);
    gte_write_ctrl(24u, static_cast<uint32_t>(ofx) << 16);
    core.rsub.projParams.setGeomOfxForAspect(static_cast<float>(ofx));
  }
  core.rsub.projprim.bind(&core);
  spu_bind(&core);
  mdec_bind(&core);
  xa_bind(&core);
}

uint8_t bufferParity(Core &core) {
  return core.mem_r8(kBufferParityAddress);
}

uint32_t currentEnv(Core &core, const GameConfig &cfg) {
  return core.mem_r32(cfg.otBasePtr);
}

void clearOrderingTable(Core &core, const GameConfig &cfg, uint32_t env) {
  psx::cpu::dispatchGuestToReturn2(
      core, cfg.clearOtagR, env, kOtEntries, psx::cpu::ExecutionBudget::currentTurn(core), __func__);
}

void putDispEnv(Core &core, uint32_t env) {
  psx::cpu::dispatchGuestToReturn1(
      core, kPutDispEnv, env + kDispEnvOffset, psx::cpu::ExecutionBudget::currentTurn(core), __func__);
}

// Flip the parity and point the env pointer at that buffer, as 0x80050C6C does.
uint32_t flipBuffer(Core &core, const GameConfig &cfg) {
  const uint8_t parity = static_cast<uint8_t>(1u - bufferParity(core));
  core.mem_w8(kBufferParityAddress, parity);
  const uint32_t env = cfg.otRegionBase + parity * cfg.otRegionStride;
  core.mem_w32(cfg.otBasePtr, env);
  return env;
}

// The pass top after the dwell reset: this pass draws into the pool of the current parity.
void rotatePacketPool(Core &core, const GameConfig &cfg) {
  core.mem_w32(cfg.poolPtrLast, core.mem_r32(cfg.poolPtrCur));
  core.mem_w32(cfg.poolPtrCur, (cfg.packetPoolBase + bufferParity(core) * cfg.packetPoolStride) & 0xffffffu);
}

void finishPass(Core &core, const GameConfig &cfg) {
  const uint32_t env = currentEnv(core, cfg);
  switch (static_cast<PresentState>(core.mem_r8(kPresentStateAddress))) {
  case PresentState::Present:
    putDispEnv(core, env);
    psx::cpu::dispatchGuestToReturn1(
        core, cfg.putDrawEnv, env + kDrawEnvOffset, psx::cpu::ExecutionBudget::currentTurn(core), __func__);
    eng(&core).drawOTag(env + kOtHeadOffset);
    clearOrderingTable(core, cfg, flipBuffer(core, cfg));
    return;
  case PresentState::Swap:
    putDispEnv(core, env);
    core.mem_w8(kPresentStateAddress, static_cast<uint8_t>(PresentState::Hold));
    flipBuffer(core, cfg);
    return;
  case PresentState::Clear:
    clearOrderingTable(core, cfg, env);
    return;
  case PresentState::Hold:
    return;
  }
}

} // namespace

TombaFrameDriver::TombaFrameDriver(Game &game) : game_(&game) {}

void TombaFrameDriver::enterLoop(Core &core) {
  const GameConfig &cfg = requireMeasuredConfig(core);
  const uint8_t parity = bufferParity(core);
  core.mem_w32(cfg.poolPtrCur, cfg.packetPoolBase + (1u - parity) * cfg.packetPoolStride);
  const uint32_t env = cfg.otRegionBase + parity * cfg.otRegionStride;
  core.mem_w32(cfg.otBasePtr, env);
  clearOrderingTable(core, cfg, env);
}

void TombaFrameDriver::stepFrame(Core &core, uint32_t frame) {
  if (!core.game || core.game != game_) {
    lucent::error("tomba-frame", "TombaFrameDriver used with a different or unbound Game");
    std::abort();
  }

  if (!cards_.finished()) {
    cards_.step(core);
    game_->run.fieldDelivered();
    return;
  }

  Game &game = *game_;
  const GameConfig &cfg = requireMeasuredConfig(core);

  autoDrive_.beforeFrame(core, frame);
  bindFrameHardware(core);
  game.timing.logicFrame = frame;
  game.perf.frameBegin();
  game.timing.frameTick();
  LibapiIntr::mirrorHostVblank(core, game.timing.vblank);
  // The engine's own frame-rate transaction opens here: zero the guest's dwell counter (what
  // FUN_80050b08 does at 0x80050C8C at the top of its pass) so the per-field work later in this
  // frame counts against this frame and not the last one. FrameCadence owns the decision; the
  // frame driver only sequences it. The one check that matters is at the bottom.
  eng(&core).cadence().beginLogicFrame();
  rotatePacketPool(core, cfg);
  for (uint32_t eventClass : cfg.irqEventClasses) {
    game.hle.deliverEvent(eventClass, 0xffffffffu);
  }

  game.pad.serviceFrame();
  game.cd.audioTrace("pre");
  game.perf.markPre();

  // The pending capture is consumed before any producer begins this frame. Engine::frameUpdate owns
  // Tomba's pad-edge and per-VBlank audio work; this driver owns the one presentation fence.
  eng(&core).frameUpdate();
  game.perf.phaseBegin(GpuPerf::Phase::Present);
  game.presentation.commit(&core, 0, game.temporalPresentation.get());
  // Packet spans live as long as the record: the OT walked last pass is sealed by this commit.
  core.rsub.otAttr.beginLogicFrame(frame);
  game.perf.phaseEnd(GpuPerf::Phase::Present);
  game.run.fieldDelivered();

  devWarp_.applyArmed(core, frame);
  game.cd.audioTrace("post");
  game.perf.phaseBegin(GpuPerf::Phase::GameLogic);
  game.pcSched.step();
  game.perf.phaseEnd(GpuPerf::Phase::GameLogic);

  eng(&core).musicCoord.tick();
  game.cd.audioTrace("coord");
  psx::cpu::dispatchGuestToReturn1(core, cfg.drawSync, 0, psx::cpu::ExecutionBudget::currentTurn(core), __func__);
  game.pcSched.tickSleepCountdown();
  finishPass(core, cfg);
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
