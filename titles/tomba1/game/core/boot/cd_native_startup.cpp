#include "cd_native_startup.h"

#include "core.h"
#include "game.h"
#include "stock_libcd.h"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <lucent/log.h>
#include <span>

namespace tomba1 {
namespace {

// RAM side of FUN_80066084/FUN_80064664's successful controller initialization. The console body
// resets the physical controller, polls it through FUN_800659BC, then publishes these library states
// and callbacks. The port has no asynchronous controller: title disc operations are host-synchronous,
// so only the observable libcd state is retained and the poll (which queries VSync for a timeout) is
// deliberately absent.
constexpr std::uint32_t kLastCommand = 0x8009600Du;
constexpr std::uint32_t kLastMode = 0x8009600Cu;
constexpr std::uint32_t kSyncCallback = 0x80095FECu;
constexpr std::uint32_t kUnusedCallback = 0x80096000u;
constexpr std::uint32_t kFlags = 0x80095FFCu;
constexpr std::uint32_t kReadyStatus = 0x800962C8u;
constexpr std::uint32_t kSyncStatus = 0x800962C9u;
constexpr std::uint32_t kInterruptStatus = 0x800962CAu;
constexpr std::uint32_t kCommandCallback = 0x80096300u;
constexpr std::uint32_t kSyncHandler = 0x800646F4u;
constexpr std::uint32_t kReadyHandler = 0x8006471Cu;
constexpr std::uint32_t kCommandHandler = 0x80064744u;
constexpr std::uint8_t kCdcIrqDataReady = 1u;
constexpr std::uint8_t kCdcIrqComplete = 2u;
constexpr std::uint8_t kCdcIrqAcknowledge = 3u;

// libcd's interrupt handler, which this port replaces with host-served CdControl/CdSync, consumes
// every command response (INT3/INT2) and drops a data-ready (INT1) no ready callback receives. Those
// responses have no other consumer here, so the host command path retires the ones already queued.
void retireUnownedResponses(Core &core) {
  CdcState &controller = core.game->cdc;
  cdc_drive_service(&controller);
  for (;;) {
    const std::uint8_t type = cdc_current_irq_type(&controller);
    const bool commandResponse = type == kCdcIrqComplete || type == kCdcIrqAcknowledge;
    const bool unclaimedData = type == kCdcIrqDataReady && core.mem_r32(kCdReadyCallbackPointer) == 0u;
    if (!commandResponse && !unclaimedData) {
      return;
    }
    psx::cd::acknowledgeCurrentCdResponse(core, std::span<std::uint8_t>{});
  }
}

} // namespace

void initializeSynchronousCd(Core &core, unsigned int resultAddress) {
  core.mem_w8(kLastCommand, 0u);
  core.mem_w8(kLastMode, 0u);
  core.mem_w32(kSyncCallback, kSyncHandler);
  core.mem_w32(kCdReadyCallbackPointer, kReadyHandler);
  core.mem_w32(kUnusedCallback, 0u);
  core.mem_w32(kCommandCallback, kCommandHandler);
  core.mem_w32(kFlags, 0u);
  core.mem_w8(kReadyStatus, 2u);
  core.mem_w8(kSyncStatus, 0u);
  core.mem_w8(kInterruptStatus, 0u);
  core.mem_w8(resultAddress, 0x80u);
  core.r[2] = 1u;
  lucent::info("tomba1-cd", "installed synchronous-ready libcd startup state without a guest wait");
}

void cdControlOverride(Core *core) {
  if (!core || !core->game) {
    std::abort();
  }
  core->game->cd.stockLibrary()->control(*core);
  retireUnownedResponses(*core);
}

void cdControlFOverride(Core *core) {
  if (!core || !core->game) {
    std::abort();
  }
  core->game->cd.stockLibrary()->controlFire(*core);
  retireUnownedResponses(*core);
}

void cdSyncOverride(Core *core) {
  if (!core || !core->game) {
    std::abort();
  }
  retireUnownedResponses(*core);
  const std::uint32_t result = core->r[5];
  if (result != 0u) {
    for (std::uint32_t offset = 0; offset < 8u; ++offset) {
      core->mem_w8(result + offset, 0u);
    }
  }
  core->r[2] = 2u;
}

void cdReadyOverride(Core *core) {
  if (!core || !core->game) {
    std::abort();
  }
  CdcState &controller = core->game->cdc;
  cdc_drive_service(&controller);
  lucent::debug("tomba1-cdready",
                "reason={} queue={}->{} read={} first={} data={}/{} lba={}",
                cdc_current_irq_type(&controller),
                controller.q_head,
                controller.q_tail,
                controller.reading,
                controller.first_sector_pending,
                controller.data_rd,
                controller.data_n,
                controller.loc_lba);
  std::array<std::uint8_t, 8> response{};
  core->r[2] = psx::cd::acknowledgeCurrentCdResponse(*core, response);
  const std::uint32_t result = core->r[5];
  if (core->r[2] != 0u && result != 0u) {
    for (std::uint32_t offset = 0; offset < response.size(); ++offset) {
      core->mem_w8(result + offset, response[offset]);
    }
  }
}

} // namespace tomba1
