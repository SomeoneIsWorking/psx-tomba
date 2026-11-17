#include "guest_ui_draws.h"

#include "core.h"
#include "core/overrides/native_override_catalog.h"
#include "frame/frame_driver.h"
#include "guest_call.h"
#include "hud_anchor_packets.h"
#include "native_dispatch.h"
#include "render/crop_layers.h"
#include "render/guest_ordering_table.h"

#include <cstdint>

namespace tomba {
namespace {

constexpr std::uint32_t kLifeGauge = 0x80025744u;
constexpr std::uint32_t kPieceTally = 0x80025934u;
constexpr std::uint32_t kItemStrip = 0x80025B78u;
constexpr std::uint32_t kPauseMainPage = 0x8007EAE4u;
// The picture's centre column: a HUD element wholly on one side of it belongs to that edge.
constexpr int kCentreColumn = 160;

template <std::uint32_t Entry> void original(Core *c) {
  psx::cpu::callOriginalToReturn(*c, Entry, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
}

// The element at its authored 4:3 place, moved to the drawn picture's edge under a widening.
template <std::uint32_t Entry> void anchored(Core *c) {
  const render::PacketPool pool(*c);
  const std::uint32_t first = pool.cursor();
  original<Entry>(c);
  psx::present::anchorHudElement(*c, FrameDriver::otHead(*c), {.first = first, .end = pool.cursor()}, kCentreColumn);
}

} // namespace

void GuestUiDraws::install(native::NativeOverrideCatalog &natives) {
  natives.declare(kLifeGauge, "GuestUiDraws::lifeGauge", render::uiDraw<anchored<kLifeGauge>>);
  natives.declare(kPieceTally, "GuestUiDraws::pieceTally", render::uiDraw<anchored<kPieceTally>>);
  natives.declare(kItemStrip, "GuestUiDraws::itemStrip", render::uiDraw<original<kItemStrip>>);
  natives.declare(kPauseMainPage, "GuestUiDraws::pauseMainPage", render::uiDraw<original<kPauseMainPage>>);
}

} // namespace tomba
