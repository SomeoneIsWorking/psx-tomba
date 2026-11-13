#include "guest_ui_draws.h"

#include "core.h"
#include "core/overrides/native_override_catalog.h"
#include "guest_call.h"
#include "native_dispatch.h"
#include "render/crop_layers.h"

#include <cstdint>

namespace tomba {
namespace {

constexpr std::uint32_t kLifeGauge = 0x80025744u;
constexpr std::uint32_t kPieceTally = 0x80025934u;
constexpr std::uint32_t kItemStrip = 0x80025B78u;
constexpr std::uint32_t kPauseMainPage = 0x8007EAE4u;

template <std::uint32_t Entry> void original(Core *c) {
  psx::cpu::callOriginalToReturn(*c, Entry, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
}

} // namespace

void GuestUiDraws::install(native::NativeOverrideCatalog &natives) {
  natives.declare(kLifeGauge, "GuestUiDraws::lifeGauge", render::uiDraw<original<kLifeGauge>>);
  natives.declare(kPieceTally, "GuestUiDraws::pieceTally", render::uiDraw<original<kPieceTally>>);
  natives.declare(kItemStrip, "GuestUiDraws::itemStrip", render::uiDraw<original<kItemStrip>>);
  natives.declare(kPauseMainPage, "GuestUiDraws::pauseMainPage", render::uiDraw<original<kPauseMainPage>>);
}

} // namespace tomba
