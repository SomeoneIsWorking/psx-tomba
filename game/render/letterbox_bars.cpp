// game/render/letterbox_bars.cpp — FUN_80026864, the cutscene letterbox bars.
#include "letterbox_bars.h"

#include "core.h"
#include "core/overrides/native_override_catalog.h"
#include "ui/panel.h"
#include "wide_window.h"

namespace tomba::render {

namespace {

constexpr std::uint32_t kEntry = 0x80026864u;
constexpr std::uint8_t kIdle = 0;
constexpr std::uint8_t kGrow = 1;
constexpr std::uint8_t kHold = 2;
constexpr std::uint8_t kShrink = 3;
constexpr std::uint8_t kEffectsDraw = 2;
constexpr std::uint8_t kCueOpen = 1;
constexpr std::uint8_t kCueClosed = 0;
constexpr std::uint8_t kCueEnd = 2;
constexpr std::uint32_t kBlackNearBucket = 1u; // FUN_8007FCC8 mode: no fill colour, OT bucket 1

struct DrawBarsArgs {
  std::uint32_t slot;
  int margin;
};
void drawBars(Core &core, const DrawBarsArgs &args) {
  auto [slot, margin] = args;
  const int x = -margin;
  const int width = wide_window::kGuestWidth + 2 * margin;
  const std::int16_t top = static_cast<std::int16_t>(core.mem_r16(slot + LetterboxBars::kHeight));
  Panel::pushDialogBackdrop(&core,
                            {.x = static_cast<std::int16_t>(x),
                             .y = 0,
                             .w = static_cast<std::int16_t>(width),
                             .h = top,
                             .mode = kBlackNearBucket});
  const std::int16_t bottom = static_cast<std::int16_t>(core.mem_r16(slot + LetterboxBars::kHeight));
  Panel::pushDialogBackdrop(&core,
                            {.x = static_cast<std::int16_t>(x),
                             .y = static_cast<std::int16_t>(LetterboxBars::kGuestHeight - bottom),
                             .w = static_cast<std::int16_t>(width),
                             .h = bottom,
                             .mode = kBlackNearBucket});
}

void tickEntry(Core *core) {
  LetterboxBars::tick(*core, core->r[4], wide_window::marginColumns(core));
}

} // namespace

void LetterboxBars::tick(Core &core, std::uint32_t slot, int margin) {
  if (core.mem_r8(kEffectsArmed) != kEffectsDraw) {
    return;
  }
  const std::uint8_t cue = core.mem_r8(kCue);
  switch (core.mem_r8(slot + kState)) {
  case kIdle:
    if (cue == kCueOpen) {
      core.mem_w8(slot + kState, kGrow);
      core.mem_w16(slot + kHeight, 0);
    }
    return;
  case kGrow: {
    const auto height = static_cast<std::int16_t>(core.mem_r16(slot + kHeight) + 1);
    core.mem_w16(slot + kHeight, static_cast<std::uint16_t>(height));
    if (height >= kFullHeight) {
      core.mem_w8(slot + kState, kHold);
    }
    break;
  }
  case kHold:
    if (cue == kCueClosed || cue == kCueEnd) {
      core.mem_w8(slot + kState, kShrink);
    }
    break;
  case kShrink: {
    const auto height = static_cast<std::int16_t>(core.mem_r16(slot + kHeight));
    core.mem_w16(slot + kHeight, static_cast<std::uint16_t>(height - 1));
    if (height == 1) {
      core.mem_w8(slot + kState, kIdle);
    }
    break;
  }
  default:
    return;
  }
  drawBars(core, {.slot = slot, .margin = margin});
}

void LetterboxBars::registerOverrides(native::NativeOverrideCatalog &natives) {
  natives.declare(kEntry, "LetterboxBars::tick", &tickEntry);
}

} // namespace tomba::render
