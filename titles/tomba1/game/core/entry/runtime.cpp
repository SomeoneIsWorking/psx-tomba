#include "runtime.h"

#include "cd_native_startup.h"
#include "context.h"
#include "core.h"
#include "crt0_boot.h"
#include "frame_driver.h"
#include "game.h"
#include "guest_code_module.h"
#include "guest_task_slots.h"
#include "native_boot.h"
#include "native_dispatch.h"
#include "platform_hle.h"
#include "sync_native.h"

#include <cstdlib>
#include <lucent/log.h>

namespace tomba1 {

namespace {

constexpr psx::game::Crt0Image kCrt0{
    .bss = {0x8009AFB0u, 0x800A3348u},
    .stackTopWordAddress = 0x80076E38u,
    .stackReserveWordAddress = 0x80076E3Cu,
    .heapBase = 0x800A3348u,
    .heapSizeStoreAddress = 0x800975C0u,
    .heapBaseStoreAddress = 0x800975BCu,
    .globalPointer = 0x80097FA8u,
    .libcInitEntry = 0x8006B70Cu,
    .crt0Entry = 0x8006B58Cu,
    .stackBias = {.declared = true, .bytes = -8},
};

// Where the load-list walker 0x80021D70 places a type-0xF0 entry: the code module resident code
// calls into (jal 0x800E7D5C from 0x80019A7C and 0x80019A94).
constexpr std::uint32_t kCodeModuleBase = 0x800E7388u;

} // namespace

Runtime::Runtime()
    : vsync_({.entry = kVSyncEntry, .queryCounter = kVSyncQueryCounter}),
      libcd_({.read = kCdRead, .readSync = kCdReadSync}, {}, kCdReadyCallbackPointer),
      libgte_({.setGeomOffset = kSetGeomOffset, .setGeomScreen = kSetGeomScreen}),
      pad_({.slot0Buffer = 0x8009EB58u, .slot1Buffer = 0x8009EB7Au}) {}

void Runtime::startProgram(Core &core) {
  psx::game::Crt0Boot{kCrt0}.run(core);
  // The prefix creates the first task records, so the slots are reachable for it and no longer.
  const GuestTaskCreationScope creation(context(core).tasks);
  runNativeBootPrefix(core);
}

void Runtime::registerOverrides(Game &game) {
  vsync_.bind(game);
  libcd_.bind(game);
  libgte_.bind(game);
  bindTitleLibraryOwners(game);
  const auto install = [&game](std::uint32_t address, std::string_view name, psx::cpu::NativeFunction function) {
    const auto identity = game.core.currentImageIdentity(address);
    if (!identity) {
      lucent::error("tomba1-runtime", "cannot bind '{}' at ambiguous guest address 0x{:08X}", name, address);
      return false;
    }
    return game.core.nativeDispatcher().install({{*identity, address}, name, function});
  };
  const bool installed =
      install(kTaskStartEntry, "GuestTaskSlots::startOverride", GuestTaskSlots::startOverride) &&
      install(kTaskYieldEntry, "GuestTaskSlots::yieldOverride", GuestTaskSlots::yieldOverride) &&
      install(kTaskRestartEntry, "GuestTaskSlots::restartOverride", GuestTaskSlots::restartOverride) &&
      install(kTaskExitEntry, "GuestTaskSlots::exitOverride", GuestTaskSlots::exitOverride) &&
      install(0x80061480u, "gpuTimeoutBeginOverride", gpuTimeoutBeginOverride) &&
      install(0x800614B4u, "gpuTimeoutExpiredOverride", gpuTimeoutExpiredOverride) &&
      install(kDmaCallbackEntry, "dmaCallbackOverride", dmaCallbackOverride);
  if (!installed) {
    lucent::error("tomba1-runtime", "native override registration refused");
    std::abort();
  }
}

std::unique_ptr<psx::game::TitleSessionState> Runtime::createSession(Game &game) {
  return std::make_unique<Context>(game.core);
}

void Runtime::deliverPad(Core &core, const psx::input::PadDelivery &delivery) {
  pad_.deliver(core, delivery);
}

void Runtime::stockCdReadLanded(Core &core, const psx::cd::StockReadLanding &landing) {
  if (landing.destination == kCodeModuleBase) {
    psx::code_module::publishStockReadLanding(core, landing);
  }
}

std::unique_ptr<psx::game::FrameDriver> Runtime::createFrameDriver(Game &game) {
  return std::make_unique<FrameDriver>(game);
}

const GuestWidescreenProjection *Runtime::guestWidescreenProjection() const {
  return &widescreenPolicy_;
}

} // namespace tomba1
