#include "tomba1_runtime.h"

#include "cd_native_startup.h"
#include "context.h"
#include "core.h"
#include "dynarec_capabilities.h"
#include "frame_driver.h"
#include "game.h"
#include "guest_cd_stream_callback_layout.h"
#include "guest_task_slots.h"
#include "native_boot.h"
#include "native_dispatch.h"
#include "sync_native.h"

#include <cstdlib>
#include <lucent/log.h>

static_assert(PSXPORT_HAS_DYNAREC_RUNTIME == 1, "Tomba! 1 requires psxport's Lightrec runtime executor");

namespace tomba1 {

const GuestProgramImage Tomba1Runtime::programImage_{
    .bss = {0x0009AFB0u, 0x000A3348u},
    .stackTopWordAddress = 0x80076E38u,
    .stackReserveWordAddress = 0x80076E3Cu,
    .heapBase = 0x800A3348u,
    .heapSizeStoreAddress = 0x800975C0u,
    .heapBaseStoreAddress = 0x800975BCu,
    .globalPointer = 0x80097FA8u,
    .libcInitEntry = 0x8006B70Cu,
    .gameMainEntry = 0x800163B0u,
    .crt0Entry = 0x8006B58Cu,
    .residentText = {0x00010000u, 0x00098000u},
    .stackBias = {true, -8},
};

const GuestPadBufferLayout Tomba1Runtime::padBufferLayout_{
    .slot0Buffer = 0x8009EB58u,
    .slot1Buffer = 0x8009EB7Au,
};

const GuestCdStreamCallbackLayout kCdStreamCallbackLayout{
    .readyCallbackPointer = kCdReadyCallbackPointer,
};

void *Tomba1Runtime::createContext(Core &core) {
  return new Context(core);
}

void Tomba1Runtime::destroyContext(void *context) {
  delete static_cast<Context *>(context);
}

void Tomba1Runtime::registerOverrides(Game &game) {
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
      install(0x80061480u, "gpuTimeoutBeginOverride", gpuTimeoutBeginOverride) &&
      install(0x800614B4u, "gpuTimeoutExpiredOverride", gpuTimeoutExpiredOverride) &&
      install(kDmaCallbackEntry, "dmaCallbackOverride", dmaCallbackOverride) &&
      install(widescreen::kSetDefDrawEnv, "tomba1-wide::SetDefDrawEnv", widescreen::drawEnvironmentOverride);
  if (!installed) {
    lucent::error("tomba1-runtime", "native override registration refused");
    std::abort();
  }
}

void Tomba1Runtime::bootInit(Core &core) {
  runNativeBootPrefix(core);
}

std::unique_ptr<FrameDriver> Tomba1Runtime::createFrameDriver(Game &game) {
  return std::make_unique<Tomba1FrameDriver>(game, *this);
}

const GuestProgramImage *Tomba1Runtime::guestProgramImage() const {
  return &programImage_;
}

const PlatformHlePlan *Tomba1Runtime::platformHlePlan() const {
  return &tomba1::platformHlePlan();
}

const GuestPadBufferLayout *Tomba1Runtime::guestPadBufferLayout() const {
  return &padBufferLayout_;
}

const GuestCdStreamCallbackLayout *Tomba1Runtime::guestCdStreamCallbackLayout() const {
  return &kCdStreamCallbackLayout;
}

RenderCapabilities Tomba1Runtime::renderCapabilities() const {
  return RenderCapabilities::widescreenOnly();
}

bool Tomba1Runtime::guestVramIsPicture(const Game &) const {
  return true;
}

const GuestWidescreenProjection *Tomba1Runtime::guestWidescreenProjection() const {
  return &widescreenPolicy_;
}

} // namespace tomba1
