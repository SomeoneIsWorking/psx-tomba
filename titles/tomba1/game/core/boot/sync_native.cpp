#include "sync_native.h"

#include "cd_control.h"
#include "cd_native_startup.h"
#include "context.h"
#include "core.h"
#include "dma_callbacks.h"
#include "game.h"
#include "guest_call.h"
#include "platform_hle.h"
#include "widescreen_projection.h"

#include <cstdlib>
#include <lucent/log.h>

namespace tomba1 {
namespace {

// SCUS_942.36's linked libetc VSync. The function reads the vblank counter for negative mode,
// handles mode 1 without waiting, and otherwise enters the GPU/vblank wait whose timeout path cites
// the retail "VSync: timeout" string. Ghidra's next function begins at 0x80067D78.
constexpr std::uint32_t kVSyncEntry = 0x80067C30u;
constexpr std::uint32_t kVSyncEnd = 0x80067D78u;
constexpr std::uint32_t kLibCdEntry = 0x80064664u;
constexpr std::uint32_t kLibCdEnd = 0x80066BA8u;
constexpr std::uint32_t kLibGteEntry = 0x80063834u;
constexpr std::uint32_t kLibGteEnd = 0x80063B00u;
// The guest's own libgte SetGeomScreen (0x80063A54) is framework-owned: the title does not change
// H, and re-asserting H is what the game's own 0x8002D784 does per area with the same 0x220.
// SetGeomOffset (0x80063A34) is the opposite case, and that is why it is NOT in the typed
// setGeomOffset field: the title widens exactly this publication, and a title-owned body must
// replace the framework's standard handler rather than sit behind it — `resolveHostDispatch` in
// runtime/cpu/native_dispatch.cpp consults `platform_hle.lookup` BEFORE the native dispatcher, so
// an override installed only in the native registry at this address would be unreachable.
constexpr std::uint32_t kSetGeomScreen = 0x80063A54u;
constexpr std::uint32_t kSetGeomOffset = 0x80063A34u;
constexpr std::uint32_t kCdSync = 0x800648C8u;
constexpr std::uint32_t kCdReady = 0x800648E8u;
constexpr std::uint32_t kCdControl = 0x80064938u;
constexpr std::uint32_t kCdControlF = 0x80064A70u;
constexpr std::uint32_t kCdRead = 0x80066A50u;
constexpr std::uint32_t kCdReadSync = 0x80066B30u;
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

// The PlatformHle body for 0x80063A34. The leaf's own behaviour is the GTE CR24/CR25 write plus the
// 16.16 argument registers; the owner's `publishProjection` performs both, and only replaces the
// horizontal centre. OFY and H are not this title's to move.
void publishProjectionOverride(Core *core) {
  if (!core) {
    lucent::error("tomba1-sync", "libgte SetGeomOffset reached without a Core");
    std::abort();
  }
  context(*core).widescreen.publishProjection(*core);
}

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

  psx::cpu::dispatchGuestToReturn1(core, kCriticalSection, 0u, psx::cpu::ExecutionBudget::currentTurn(core), __func__);
  core.mem_w32(kGpuTimeoutCriticalState, core.r[2]);
  core.mem_w32(kGpuQueueReadIndex, 0u);
  core.mem_w32(kGpuQueueWriteIndex, 0u);
  core.mem_w32(statusAddress, 0x401u);
  core.mem_w32(controlAddress, core.mem_r32(controlAddress) | 0x800u);
  core.mem_w32(commandAddress, 0x02000000u);
  core.mem_w32(commandAddress, 0x01000000u);
  psx::cpu::dispatchGuestToReturn1(core,
                                   kCriticalSection,
                                   core.mem_r32(kGpuTimeoutCriticalState),
                                   psx::cpu::ExecutionBudget::currentTurn(core),
                                   __func__);
}

} // namespace

namespace {

// Tomba! 1's HLE table, built once at static-initialisation time from the constants above and the six
// handlers this file owns. A namespace-scope const, NOT a function-local static: the policy forbids
// those, and returning a reference to a function-local aggregate would dangle the moment the frame
// returned. Nothing here depends on a Core, so initialising at load is also simply correct.
const PlatformHlePlan kPlatformHlePlan = [] {
  PlatformHlePlan value{};
  value.setGeomScreen = kSetGeomScreen;
  value.cdReadAddress = kCdRead;
  value.cdReadSyncAddress = kCdReadSync;
  value.vsyncAddress = kVSyncEntry;
  value.bindings[0] = {kCdSync, cdSyncOverride};
  value.bindings[1] = {kCdSyncInternalEntry, cdSyncOverride};
  value.bindings[2] = {kCdReady, cdReadyOverride};
  value.bindings[3] = {kCdControl, cdControlOverride};
  value.bindings[4] = {kCdControlF, cdControlFOverride};
  // The title WIDENS this projection, so it owns the body rather than leaving it to the
  // framework's standard handler. The guest's own libgpu SetDefDrawEnv is installed in
  // Tomba1Runtime::registerOverrides instead: PlatformHle's builtin table covers the two libgte
  // projection leaves and the stock library services, and a guest libgpu rectangle constructor is
  // none of those.
  value.bindings[5] = {kSetGeomOffset, publishProjectionOverride};
  value.bindingCount = 6;
  value.windowLo[0] = kLibCdEntry;
  value.windowHi[0] = kLibCdEnd;
  value.windowLo[1] = kVSyncEntry;
  value.windowHi[1] = kVSyncEnd;
  value.windowLo[2] = kLibGteEntry;
  value.windowHi[2] = kLibGteEnd;
  return value;
}();

} // namespace

const PlatformHlePlan &platformHlePlan() {
  return kPlatformHlePlan;
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
