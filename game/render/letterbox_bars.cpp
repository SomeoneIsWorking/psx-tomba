// game/render/letterbox_bars.cpp — FUN_80026864, the cutscene letterbox bars.
#include "letterbox_bars.h"

#include "blend.h"
#include "core.h"
#include "core/overrides/native_override_catalog.h"
#include "entry/session.h"
#include "frame/update_block_draws.h"
#include "guest_redraw.h"
#include "projected_vertex.h"
#include "ui/panel.h"
#include "wide_window.h"

#include <array>
#include <cmath>

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
// The 0x60 rect packet after its tag: colour, corner x|y, size w|h.
constexpr std::uint32_t kBarCorner = 8u;
constexpr std::uint32_t kBarSize = 12u;

struct DrawBarsArgs {
  std::uint32_t slot;
  int margin;
};
// The top bar's packet and the bottom's.
std::array<std::uint32_t, 2> drawBars(Core &core, const DrawBarsArgs &args) {
  auto [slot, margin] = args;
  const int x = -margin;
  const int width = wide_window::kGuestWidth + 2 * margin;
  const std::int16_t top = static_cast<std::int16_t>(core.mem_r16(slot + LetterboxBars::kHeight));
  const std::uint32_t topBar = Panel::pushDialogBackdrop(&core,
                                                         {.x = static_cast<std::int16_t>(x),
                                                          .y = 0,
                                                          .w = static_cast<std::int16_t>(width),
                                                          .h = top,
                                                          .mode = kBlackNearBucket});
  const std::int16_t bottom = static_cast<std::int16_t>(core.mem_r16(slot + LetterboxBars::kHeight));
  const std::uint32_t bottomBar =
      Panel::pushDialogBackdrop(&core,
                                {.x = static_cast<std::int16_t>(x),
                                 .y = static_cast<std::int16_t>(LetterboxBars::kGuestHeight - bottom),
                                 .w = static_cast<std::int16_t>(width),
                                 .h = bottom,
                                 .mode = kBlackNearBucket});
  return {topBar, bottomBar};
}

// A bar's packet: its corner at `x` columns and `y` lines, sub-line placed, `height` lines tall.
struct BarPlace {
  std::uint32_t packet;
  float x;
  double y;
  std::int32_t height;
};
void placeBar(psx::present::GuestRedraw &redraw, const BarPlace &bar) {
  redraw.writeVertex(bar.packet + kBarCorner,
                     psx::gte::ProjectedVertex::nearest({bar.x, static_cast<float>(bar.y), 0.0f, 0.0f}));
  redraw.writeHalf(bar.packet + kBarSize + 2u, static_cast<std::uint16_t>(bar.height));
}

// The bars' edges at the height t of the way; the bars are as tall as the higher height, so each reaches its
// screen edge, and at t = 1 they are the later frame's.
void rebuildBars(psx::present::GuestRedraw &redraw,
                 const UpdateBlockDraws::Draw &earlier,
                 const UpdateBlockDraws::Draw &later,
                 float t) {
  const auto from = static_cast<std::int32_t>(earlier.values[0]);
  const auto to = static_cast<std::int32_t>(later.values[0]);
  const auto x = static_cast<float>(static_cast<std::int32_t>(later.values[1]));
  const double edge = psx::present::lerpExact(from, to, t);
  const auto height = static_cast<std::int32_t>(std::ceil(edge));
  placeBar(redraw, {.packet = later.packets[0], .x = x, .y = edge - height, .height = height});
  placeBar(redraw, {.packet = later.packets[1], .x = x, .y = LetterboxBars::kGuestHeight - edge, .height = height});
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
  const psx::present::EmissionScope::Guard object(core.emission, kEntry, slot, 0);
  const std::array<std::uint32_t, 2> bars = drawBars(core, {.slot = slot, .margin = margin});
  sessionOf(&core).drawReplay.updateDraws().note(
      core,
      {.writer = kEntry,
       .object = slot,
       .packets = bars,
       .values = {static_cast<std::uint32_t>(static_cast<std::int16_t>(core.mem_r16(slot + kHeight))),
                  static_cast<std::uint32_t>(-margin)},
       .rebuild = &rebuildBars});
}

void LetterboxBars::registerOverrides(native::NativeOverrideCatalog &natives) {
  natives.declare(kEntry, "LetterboxBars::tick", &tickEntry);
}

} // namespace tomba::render
