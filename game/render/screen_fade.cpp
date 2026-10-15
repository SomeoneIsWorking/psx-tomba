// class ScreenFade — implementation. See screen_fade.h.
#include "screen_fade.h"

#include "blend.h"
#include "core.h"
#include "core/overrides/guest_jal.h"
#include "core/overrides/native_override_catalog.h"
#include "entry/session.h"
#include "gp0_primitive_decode.h"
#include "guest_abi.h"
#include "guest_call.h"
#include "guest_ordering_table.h"
#include "wide_window.h"

#include <cstdlib>
#include <lucent/log.h>

namespace tomba {

namespace {

using tomba::render::OrderingTable;
using tomba::render::PacketPool;

constexpr GuestFrameSpill kSpills[] = {{17, 0x1C}, {31, 0x20}, {16, 0x18}};
constexpr std::uint32_t kFrameBytes = 0x28u;
constexpr std::uint32_t kDrawModeTw = 0x10u; // SetDrawMode's 5th argument, sp-relative

constexpr std::uint32_t kSetDrawMode = 0x80083DE0u;
constexpr std::uint32_t kReturnFromDrawMode = 0x8007EA9Cu;

constexpr std::uint8_t kFillCommand = 0x62; // monochrome rect, semi-transparent
constexpr std::uint32_t kFillWords = 3u;
constexpr std::uint32_t kFillBytes = 16u;
constexpr std::uint32_t kModeWords = 2u;
constexpr std::uint32_t kModeBytes = 12u;
constexpr std::uint32_t kSubtractiveTpage = 0x40u;
constexpr std::uint32_t kAdditiveTpage = 0x20u;

// Each call is an object of its own: the fade a call site draws, n-th in its frame.
void drawEntry(Core *core) {
  const psx::present::EmissionScope::Guard object(
      core->emission, ScreenFade::kLeaf, sessionOf(core).callSites.object(core->r[31]), 0);
  ScreenFade::draw(*core, tomba::wide_window::drawWindow(core));
}

// What one fade drew: its fill colour and span, and the blend its draw mode packet set.
struct Saved {
  std::uint32_t table = 0;
  std::uint32_t slot = 0; // OT slot of bucket 0
  std::uint32_t bucket = 0;
  std::uint32_t color = 0; // 0x00BBGGRR
  std::uint32_t texturePage = 0;
  std::int32_t left = 0;
  std::int32_t width = 0;
};

std::uint32_t lerpColor(std::uint32_t from, std::uint32_t to, float t) {
  std::uint32_t color = 0;
  for (std::uint32_t shift = 0; shift < 24u; shift += 8u) {
    const auto channel = psx::present::lerpInt(
        static_cast<std::int32_t>((from >> shift) & 0xFFu), static_cast<std::int32_t>((to >> shift) & 0xFFu), t);
    color |= static_cast<std::uint32_t>(channel) << shift;
  }
  return color;
}

// The fill at t: its colour moves, its span and blend are the later frame's.
class FadeRender final : public psx::present::StateProducer {
public:
  void render(const psx::present::StateInterval &states, psx::present::PrimitiveSink &sink) const override {
    const auto [from, to, t] = states;
    const auto later = psx::present::stateAs<Saved>(to);
    const auto earlier = from.data() == to.data() ? later : psx::present::stateAs<Saved>(from);
    const std::uint32_t color =
        earlier.texturePage == later.texturePage ? lerpColor(earlier.color, later.color, t) : later.color;
    const std::uint32_t command[kFillWords] = {(static_cast<std::uint32_t>(kFillCommand) << 24) | color,
                                               static_cast<std::uint16_t>(later.left),
                                               (static_cast<std::uint32_t>(ScreenFade::kGuestHeight) << 16) |
                                                   static_cast<std::uint16_t>(later.width)};
    auto fill = psx::gpu::decodePacketPrimitive(command);
    if (!fill) {
      lucent::error("screen-fade", "fade fill packet did not decode");
      std::abort();
    }
    psx::gpu::applyTexPageAttribute(fill->state, static_cast<std::uint16_t>(later.texturePage));
    sink.emit(psx::present::OtSlot{static_cast<std::uint16_t>(later.table), later.slot + later.bucket}, *fill);
  }
};

} // namespace

void ScreenFade::applyLeafCall(uint32_t color, uint32_t blend, uint32_t otSlot) {
  psx::cpu::callGuestNow(*core, __func__, kLeaf, color, blend, otSlot);
}

void ScreenFade::draw(Core &core, const tomba::wide_window::Window &window) {
  GuestFrame<kFrameBytes, 3> frame(&core, kSpills);
  const std::uint32_t color = core.r[4];
  const std::uint32_t blend = core.r[5] & 0xFFu;
  const std::uint32_t slot = core.r[6];
  const std::uint32_t sp = core.r[29];

  core.mem_w32(kStage, color);
  core.mem_w8(kStage + 3u, kFillCommand);
  core.mem_w16(kStage + 8u, static_cast<std::uint16_t>(tomba::wide_window::kGuestWidth));
  core.mem_w16(kStage + 4u, 0);
  core.mem_w16(kStage + 6u, 0);
  core.mem_w16(kStage + 10u, static_cast<std::uint16_t>(kGuestHeight));

  const PacketPool pool(core);
  const OrderingTable ot = OrderingTable::active(core);
  const std::uint32_t fill = pool.allocate(kFillBytes);
  ot.link({.packet = fill, .words = kFillWords, .bucket = slot});
  core.mem_w32(fill + 4u, core.mem_r32(kStage));
  core.mem_w16(fill + 8u, static_cast<std::uint16_t>(window.left));
  core.mem_w16(fill + 10u, 0);
  core.mem_w16(fill + 12u, static_cast<std::uint16_t>(window.right - window.left));
  core.mem_w16(fill + 14u, static_cast<std::uint16_t>(kGuestHeight));

  const std::uint32_t mode = pool.cursor();
  core.r[16] = mode;
  core.r[17] = slot;
  core.mem_w32(sp + kDrawModeTw, 0u);
  tomba::guest::dispatchJalToReturn(core,
                                    {.target = kSetDrawMode, .returnAddress = kReturnFromDrawMode},
                                    mode,
                                    0u,
                                    0u,
                                    blend == 0u ? kSubtractiveTpage : kAdditiveTpage);
  ot.link({.packet = mode, .words = kModeWords, .bucket = slot});
  pool.setCursor(pool.cursor() + kModeBytes);
  const auto table = core.otTables.slotOf(ot.base());
  if (core.emission.isOpen() && table) {
    core.frameStates.save(core.emission.current(),
                          Saved{table->table,
                                table->index,
                                slot,
                                color & 0x00FFFFFFu,
                                blend == 0u ? kSubtractiveTpage : kAdditiveTpage,
                                window.left,
                                window.right - window.left});
  }
  core.r[2] = pool.cursor();
  core.r[3] = PacketPool::kCursorPage;
  core.r[4] = ot.slot(slot);
}

void ScreenFade::registerStateRenders(psx::present::StateProducerTable &renders) {
  renders.install(kLeaf, std::make_unique<FadeRender>());
}

void ScreenFade::registerOverrides(native::NativeOverrideCatalog &natives) {
  natives.declare(kLeaf, "ScreenFade::draw", &drawEntry);
}

} // namespace tomba
