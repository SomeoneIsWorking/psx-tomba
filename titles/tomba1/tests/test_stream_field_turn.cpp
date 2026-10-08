#include "stream_field_turn.h"

#include "cd_native_startup.h"
#include "cdc_state.h"

#include "core.h"
#include "dma_callbacks.h"
#include "game.h"
#include "game_runtime.h"
#include "guest_cd_stream_callback_layout.h"
#include "platform_hle.h"
#include "sync_native.h"

#include <cstdint>
#include <cstdio>
#include <memory>

namespace {

int fieldOperationCount = 0;
int audioOperationIndex = -1;
int discOperationIndex = -1;

void recordAudioOperation(Core &) {
  audioOperationIndex = fieldOperationCount++;
}

void recordDiscOperation(Core &) {
  discOperationIndex = fieldOperationCount++;
}

class TestRuntime final : public GameRuntime {
public:
  void *createContext(Core &) override {
    return nullptr;
  }
  void destroyContext(void *) override {}
  void registerOverrides(Game &) override {}
  void bootInit(Core &) override {}

  const GuestCdStreamCallbackLayout *guestCdStreamCallbackLayout() const override {
    return exposeCallbackLayout ? &callbackLayout : nullptr;
  }

  RenderCapabilities renderCapabilities() const override {
    return RenderCapabilities::widescreenOnly();
  }

  bool guestVramIsPicture(const Game &) const override {
    return true;
  }

  bool exposeCallbackLayout = false;
  GuestCdStreamCallbackLayout callbackLayout{0x80095FF0u};
};

bool check(bool condition, const char *message) {
  if (!condition) {
    std::fprintf(stderr, "FAIL: %s\n", message);
  }
  return condition;
}

} // namespace

int main() {
  if (!check(tomba1::kCdReadyCallbackPointer == 0x80095FF0u,
             "shipping stream pump must use the CdReadyCallback slot written by 0x80064920")) {
    return 1;
  }
  const PlatformHlePlan &plan = tomba1::platformHlePlan();
  if (!check(plan.cdReadAddress != 0u && plan.cdReadSyncAddress != 0u,
             "shipping plan must publish both typed stock-read addresses")) {
    return 1;
  }
  for (int index = 0; index < plan.bindingCount; ++index) {
    const std::uint32_t address = plan.bindings[index].addr;
    if (!check(address != plan.cdReadAddress && address != plan.cdReadSyncAddress,
               "framework-owned stock-read leaves must not remain in generic bindings")) {
      return 1;
    }
  }
  bool hasInternalCdSync = false;
  for (int index = 0; index < plan.bindingCount; ++index) {
    if (plan.bindings[index].addr == tomba1::kCdSyncInternalEntry) {
      hasInternalCdSync = plan.bindings[index].fn == tomba1::cdSyncOverride;
    }
  }
  if (!check(hasInternalCdSync,
             "linked libcd's direct internal CdSync calls must share the public wrapper's native owner")) {
    return 1;
  }
  if (!check(tomba1::kDmaCallbackEntry == 0x80067E84u,
             "shipping override must bind the measured linked DMACallback entry")) {
    return 1;
  }

  TestRuntime runtime;
  psxport_install_game(runtime);
  auto game = std::make_unique<Game>();

  constexpr std::uint32_t dmaInterruptControl = 0x1F8010F4u;
  constexpr std::uint32_t firstCallback = 0x80066D80u;
  constexpr std::uint32_t replacementCallback = 0x80066E00u;
  game->core.r[4] = static_cast<std::uint32_t>(DmaChannel::Cdrom);
  game->core.r[5] = firstCallback;
  tomba1::dmaCallbackOverride(&game->core);
  if (!check(game->core.r[2] == 0u && game->dmaCallbacks.current(DmaChannel::Cdrom) == firstCallback,
             "first DMACallback registration must return zero and publish channel 3")) {
    return 1;
  }
  if (!check((game->core.mem_r32(dmaInterruptControl) & 0x00880000u) == 0x00880000u,
             "DMACallback must arm channel 3 and the DICR master bit")) {
    return 1;
  }
  game->core.r[5] = replacementCallback;
  tomba1::dmaCallbackOverride(&game->core);
  if (!check(game->core.r[2] == firstCallback && game->dmaCallbacks.current(DmaChannel::Cdrom) == replacementCallback &&
                 game->dmaCallbacks.current(DmaChannel::MdecOut) == 0u,
             "replacement must return the prior callback without changing another channel")) {
    return 1;
  }

  const tomba1::StreamFieldTurn orderedFieldTurn{recordAudioOperation, recordDiscOperation};
  orderedFieldTurn.service(game->core);
  if (!check(fieldOperationCount == 2 && audioOperationIndex == 0 && discOperationIndex == 1,
             "one stream field must advance audio exactly once before pumping disc")) {
    return 1;
  }

  CdcState &controller = game->cdc;
  controller.index = 2;
  controller.q_head = 0;
  controller.q_tail = 1;
  controller.q[0].type = 1u;
  controller.q[0].len = 1;
  controller.q[0].resp[0] = 0x22u;
  controller.irq_edge = 1u;
  game->hle.i_stat = 1u << 2u;
  game->core.r[5] = 0x80010000u;
  tomba1::cdReadyOverride(&game->core);
  if (!check(game->core.r[2] == 1u && game->core.mem_r8(0x80010000u) == 0x22u,
             "native CdReady must return and copy the current controller response")) {
    return 1;
  }
  if (!check(controller.q_head == controller.q_tail && controller.index == 2 && controller.irq_edge == 0u &&
                 (game->hle.i_stat & (1u << 2u)) == 0u,
             "native CdReady must consume only the direct-callback controller edge")) {
    return 1;
  }
  tomba1::cdReadyOverride(&game->core);
  if (!check(game->core.r[2] == 0u, "native CdReady without a pending response must be nonblocking")) {
    return 1;
  }

  game->cd.stream_active = 1;
  game->cd.stream_delivered = 11u;
  tomba1::serviceStreamHostTurn(&game->core);
  if (!check(game->cd.stream_delivered == 11u, "missing callback layout must not deliver a sector")) {
    return 1;
  }

  runtime.exposeCallbackLayout = true;
  game->core.mem_w32(runtime.callbackLayout.readyCallbackPointer, 0x80010000u);
  game->cd.stream_active = 0;
  game->cd.stream_delivered = 17u;
  tomba1::serviceStreamHostTurn(&game->core);
  if (!check(game->cd.stream_delivered == 17u, "inactive stream must not deliver a sector")) {
    return 1;
  }

  std::puts("PASS: CdSync, DMA registration, field order, callback layout, and inactive stream contracts hold");
  return 0;
}
