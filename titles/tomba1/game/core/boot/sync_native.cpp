#include "sync_native.h"

#include "cd_native_startup.h"
#include "context.h"
#include "core.h"
#include "game.h"
#include "guest_call.h"
#include "platform_hle.h"

#include <cstdlib>
#include <lucent/log.h>

namespace tomba1 {
namespace {

constexpr std::uint32_t kCdSync = 0x800648C8u;
constexpr std::uint32_t kCdReady = 0x800648E8u;
constexpr std::uint32_t kCdControl = 0x80064938u;
constexpr std::uint32_t kCdControlF = 0x80064A70u;
// CdControlB: CdControl, then CdSync until complete; the movie task stops its stream with it.
constexpr std::uint32_t kCdControlB = 0x80064B9Cu;
constexpr std::uint32_t kDmaInterruptControl = 0x1F8010F4u;
constexpr std::uint32_t kDmaMasterEnable = 0x00800000u;
constexpr std::uint32_t kDmaControlBits = 0x00FFFFFFu;
constexpr std::uint32_t kDmaChannelCount = 7u;

// These linked libgpu helpers use VSync(-1) only as a 240-field timeout clock for the GPU command
// queue. The port's frame driver owns that clock, so the native replacements read logicFrame and
// preserve the retail spin-budget/reset behavior without entering guest VSync.
constexpr std::uint32_t kGpuTimeoutDeadline = 0x80090DB4u;
constexpr std::uint32_t kGpuTimeoutSpinCount = 0x80090DB8u;
constexpr std::uint32_t kGpuTimeoutCriticalState = 0x80090DB0u;
constexpr std::uint32_t kGpuQueueWriteIndex = 0x80090DA0u;
constexpr std::uint32_t kGpuQueueReadIndex = 0x80090DA4u;
constexpr std::uint32_t kGpuStatusPointer = 0x80090D7Cu;
constexpr std::uint32_t kGpuControlPointer = 0x80090D8Cu;
constexpr std::uint32_t kGpuCommandPointer = 0x80090D70u;
constexpr std::uint32_t kCriticalSection = 0x80067FA0u;
constexpr std::uint32_t kTimeoutFields = 0xF0u;
constexpr std::uint32_t kTimeoutSpins = 0xF0000u;

std::uint32_t nativeField(Core &core) {
  if (!core.game) {
    lucent::error("tomba1-sync", "GPU timeout helper has no owning Game");
    std::abort();
  }
  return core.game->timing.logicFrame;
}

void resetTimedOutGpuQueue(Core &core) {
  const std::uint32_t statusAddress = core.mem_r32(kGpuStatusPointer);
  const std::uint32_t controlAddress = core.mem_r32(kGpuControlPointer);
  const std::uint32_t commandAddress = core.mem_r32(kGpuCommandPointer);
  lucent::error("tomba1-sync",
                "GPU command queue timed out: queued={} status=0x{:08X}",
                (core.mem_r32(kGpuQueueWriteIndex) - core.mem_r32(kGpuQueueReadIndex)) & 0x3Fu,
                core.mem_r32(statusAddress));

  psx::cpu::callGuestNow(core, __func__, kCriticalSection, 0u);
  core.mem_w32(kGpuTimeoutCriticalState, core.r[2]);
  core.mem_w32(kGpuQueueReadIndex, 0u);
  core.mem_w32(kGpuQueueWriteIndex, 0u);
  core.mem_w32(statusAddress, 0x401u);
  core.mem_w32(controlAddress, core.mem_r32(controlAddress) | 0x800u);
  core.mem_w32(commandAddress, 0x02000000u);
  core.mem_w32(commandAddress, 0x01000000u);
  psx::cpu::callGuestNow(core, __func__, kCriticalSection, core.mem_r32(kGpuTimeoutCriticalState));
}

} // namespace

// CdControlB is CdControl then CdSync until done.
void bindTitleLibraryOwners(Game &game) {
  PlatformHle &table = game.platform_hle;
  table.bindStock(kCdSync, cdSyncOverride);
  table.bindStock(kCdSyncInternalEntry, cdSyncOverride);
  table.bindStock(kCdReady, cdReadyOverride);
  table.bindStock(kCdControl, cdControlOverride);
  table.bindStock(kCdControlF, cdControlFOverride);
  table.bindStock(kCdControlB, cdControlOverride);
}

void dmaCallbackOverride(Core *core) {
  if (!core || !core->game || core->r[4] >= kDmaChannelCount) {
    lucent::error("tomba1-sync", "invalid DMACallback owner or channel");
    std::abort();
  }

  const auto channel = static_cast<DmaChannel>(core->r[4]);
  core->r[2] = core->game->dmaCallbacks.exchange(channel, core->r[5]);

  const std::uint32_t channelEnable = 1u << (16u + core->r[4]);
  const std::uint32_t dicr = core->mem_r32(kDmaInterruptControl);
  core->mem_w32(kDmaInterruptControl, (dicr & kDmaControlBits) | channelEnable | kDmaMasterEnable);
}

void gpuTimeoutBeginOverride(Core *core) {
  if (!core) {
    std::abort();
  }
  core->mem_w32(kGpuTimeoutDeadline, nativeField(*core) + kTimeoutFields);
  core->mem_w32(kGpuTimeoutSpinCount, 0u);
}

void gpuTimeoutExpiredOverride(Core *core) {
  if (!core) {
    std::abort();
  }

  const auto deadline = static_cast<std::int32_t>(core->mem_r32(kGpuTimeoutDeadline));
  const auto field = static_cast<std::int32_t>(nativeField(*core));
  const std::uint32_t spins = core->mem_r32(kGpuTimeoutSpinCount);
  core->mem_w32(kGpuTimeoutSpinCount, spins + 1u);
  if (deadline < field || kTimeoutSpins < spins) {
    resetTimedOutGpuQueue(*core);
    core->r[2] = 0xFFFFFFFFu;
    return;
  }
  core->r[2] = 0u;
}

} // namespace tomba1
