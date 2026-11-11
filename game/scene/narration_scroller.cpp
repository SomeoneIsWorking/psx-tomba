#include "scene/narration_scroller.h"

#include "blend.h"
#include "core.h"
#include "core/assets/str.h"
#include "core/overrides/guest_jal.h"
#include "core/overrides/native_override_catalog.h"
#include "entry/session.h"
#include "guest_abi.h"
#include "ui/font.h"

namespace tomba {
namespace {

constexpr std::uint32_t kTaskSm = 0x1F800138u;
constexpr std::uint32_t kPhase = 0x54u;        // sm u16: 0 start, 1 scrolling, 2 done
constexpr std::uint32_t kStartedFlag = 0x6Eu;  // sm u8, cleared at the start
constexpr std::uint32_t kScroll = 0x8010D394u; // s32, the top line's y less its index times kLineStep
constexpr std::uint32_t kFirstLine = 0x8010D398u;
constexpr std::uint32_t kLines = 0x8010D320u; // string pointers, null-terminated
constexpr std::int32_t kStartScroll = 0xE0;
constexpr std::int32_t kLineStep = 0x1E;
constexpr std::int32_t kOffTop = -0x10;
constexpr std::int32_t kScreenFoot = 0xF0;
constexpr std::int32_t kEndScroll = -0x30B; // the last line leaves the top
constexpr std::int32_t kMaxLines = 13;
constexpr std::int32_t kCentreX = 0xA0;
constexpr std::int32_t kGlyphWidth = 8;
constexpr std::uint32_t kLineBucket = 1u;
constexpr guest::GuestJal kLength{.target = Str::kLength, .returnAddress = 0x8010C8D4u};
constexpr guest::GuestJal kDrawText{.target = Font::kDrawText, .returnAddress = 0x8010C920u};
constexpr GuestFrameSpill kSpills[] = {{31, 0x30}, {19, 0x2C}, {18, 0x28}, {17, 0x24}, {16, 0x20}};

std::int32_t readS32(Core &core, std::uint32_t address) {
  return static_cast<std::int32_t>(core.mem_r32(address));
}

} // namespace

NarrationScroller::Step NarrationScroller::step(Core &core) {
  const std::uint32_t sm = core.mem_r32(kTaskSm);
  const std::uint16_t phase = core.mem_r16(sm + kPhase);
  // The guest leaves v1 as it last set it on each path: the phase, the task, or the line index past the first line.
  core.r[3] = phase;
  if (phase == 2u) {
    return {.result = 1u};
  }
  if (phase > 2u) {
    return {};
  }
  if (phase == 0u) {
    core.mem_w8(sm + kStartedFlag, 0u);
    core.mem_w32(kScroll, static_cast<std::uint32_t>(kStartScroll));
    core.mem_w16(sm + kPhase, 1u);
    core.mem_w32(kFirstLine, 0u);
  }
  const std::int32_t from = readS32(core, kScroll);
  const std::int32_t scroll = from - 1;
  core.mem_w32(kScroll, static_cast<std::uint32_t>(scroll));
  if (from < kEndScroll + kOffTop) {
    const std::uint32_t task = core.mem_r32(kTaskSm);
    core.mem_w16(task + kPhase, static_cast<std::uint16_t>(core.mem_r16(task + kPhase) + 1u));
    core.r[3] = task;
    return {};
  }
  std::int32_t line = readS32(core, kFirstLine);
  core.r[3] = static_cast<std::uint32_t>(line + 1);
  if (scroll + line * kLineStep < kOffTop) {
    ++line;
    core.mem_w32(kFirstLine, static_cast<std::uint32_t>(line));
  }
  return {.placement = Placement{.scroll = scroll, .line = line}};
}

void NarrationScroller::place(Core &core, Placement placement) {
  std::int32_t y = placement.scroll + placement.line * kLineStep;
  for (std::int32_t i = 0; i < kMaxLines; ++i, y += kLineStep) {
    const std::uint32_t text = core.mem_r32(kLines + static_cast<std::uint32_t>(placement.line + i) * 4u);
    if (text == 0u || y >= kScreenFoot) {
      return;
    }
    const auto width = static_cast<std::int32_t>(guest::dispatchJalToReturn(core, kLength, text));
    const auto x = static_cast<std::int16_t>(kCentreX - ((width * kGlyphWidth) >> 1));
    core.mem_w32(core.r[29] + 16u, kLineBucket);
    guest::dispatchJalToReturn(core,
                               kDrawText,
                               static_cast<std::uint32_t>(static_cast<std::int32_t>(x)),
                               static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int16_t>(y))),
                               0u,
                               text);
  }
}

void NarrationScroller::run(Core *c) {
  const GuestFrame<0x38, 5> frame(c, kSpills);
  const std::uint32_t sm = c->mem_r32(kTaskSm);
  const bool scrolling = c->mem_r16(sm + kPhase) == 1u;
  const std::int32_t drawn = readS32(*c, kScroll);
  const Step stepped = step(*c);
  c->r[2] = stepped.result;
  if (!stepped.placement) {
    return;
  }
  const std::optional<float> t = sessionOf(c).drawReplay.frame().replaying();
  const float pastY = t && scrolling
                          ? static_cast<float>(psx::present::lerpExact(drawn, stepped.placement->scroll, *t) -
                                               stepped.placement->scroll)
                          : 0.0f;
  const Font::PlacedGlyphs placed(eng(c).font, pastY);
  place(*c, *stepped.placement);
  c->r[2] = 0u;
}

void NarrationScroller::registerOverrides(native::NativeOverrideCatalog &natives) {
  natives.declareOverlay("SOP", kEntry, "&NarrationScroller::run", &NarrationScroller::run);
}

} // namespace tomba
