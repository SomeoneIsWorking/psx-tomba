#include "cd_native_startup.h"

#include "cd_control.h"
#include "cdc_state.h"
#include "core.h"
#include "game.h"

#include <cstdint>
#include <cstdlib>
#include <lucent/log.h>

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
  if (!core) {
    std::abort();
  }
  cd_control_sync(core);
}

void cdControlFOverride(Core *core) {
  if (!core) {
    std::abort();
  }
  // CdControlF has no result-buffer argument. The shared controller owner consumes the CdControl
  // ABI, so make that absent third argument explicit rather than forwarding a stale caller register.
  core->r[6] = 0u;
  cd_control_sync(core);
}

void cdSyncOverride(Core *core) {
  if (!core) {
    std::abort();
  }
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
  const int previousBank = controller.index;
  cdc_drive_service(&controller);
  cdc_write(&controller, 0u, 1u);
  const std::uint32_t reason = cdc_read(&controller, 3u) & 7u;
  lucent::debug("tomba1-cdready",
                "reason={} queue={}->{} read={} first={} data={}/{} lba={}",
                reason,
                controller.q_head,
                controller.q_tail,
                controller.reading,
                controller.first_sector_pending,
                controller.data_rd,
                controller.data_n,
                controller.loc_lba);
  if (reason == 0u) {
    cdc_write(&controller, 0u, static_cast<std::uint8_t>(previousBank));
    core->r[2] = 0u;
    return;
  }

  const std::uint32_t result = core->r[5];
  if (result != 0u) {
    for (std::uint32_t offset = 0; offset < 8u; ++offset) {
      core->mem_w8(result + offset, static_cast<std::uint8_t>(cdc_read(&controller, 1u)));
    }
  }

  // The title host turn invokes CdReadyCallback directly, replacing the guest IRQ path. Consume
  // the current controller edge and its already-latched I_STAT bit before acknowledging it. If the
  // ACK exposes another queued response, cdc_write raises a fresh edge and leaves it intact.
  controller.irq_edge = 0u;
  core->game->hle.i_stat &= ~(1u << 2u);
  cdc_write(&controller, 3u, static_cast<std::uint8_t>(reason));
  cdc_write(&controller, 0u, static_cast<std::uint8_t>(previousBank));
  core->r[2] = reason;
}

} // namespace tomba1
