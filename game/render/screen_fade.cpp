// class ScreenFade — implementation. See screen_fade.h.
#include "screen_fade.h"

#include "core.h"
#include "core/overrides/guest_jal.h"
#include "core/overrides/native_override_catalog.h"
#include "guest_abi.h"
#include "guest_call.h"
#include "ordering_table.h"
#include "wide_window.h"

namespace {

using tomba2::render::OrderingTable;
using tomba2::render::PacketPool;

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

void drawEntry(Core *core) {
  ScreenFade::draw(*core, tomba2::wide_window::drawWindow(core));
}

} // namespace

void ScreenFade::applyLeafCall(uint32_t color, uint32_t blend, uint32_t otSlot) {
  psx::cpu::callGuestNow(*core, __func__, kLeaf, color, blend, otSlot);
}

void ScreenFade::draw(Core &core, const tomba2::wide_window::Window &window) {
  GuestFrame<kFrameBytes, 3> frame(&core, kSpills);
  const std::uint32_t color = core.r[4];
  const std::uint32_t blend = core.r[5] & 0xFFu;
  const std::uint32_t slot = core.r[6];
  const std::uint32_t sp = core.r[29];

  core.mem_w32(kStage, color);
  core.mem_w8(kStage + 3u, kFillCommand);
  core.mem_w16(kStage + 8u, static_cast<std::uint16_t>(tomba2::wide_window::kGuestWidth));
  core.mem_w16(kStage + 4u, 0);
  core.mem_w16(kStage + 6u, 0);
  core.mem_w16(kStage + 10u, static_cast<std::uint16_t>(kGuestHeight));

  const PacketPool pool(core);
  const OrderingTable ot = OrderingTable::active(core);
  const std::uint32_t fill = pool.allocate(kFillBytes);
  ot.link(fill, kFillWords, slot);
  core.mem_w32(fill + 4u, core.mem_r32(kStage));
  core.mem_w16(fill + 8u, static_cast<std::uint16_t>(window.left));
  core.mem_w16(fill + 10u, 0);
  core.mem_w16(fill + 12u, static_cast<std::uint16_t>(window.right - window.left));
  core.mem_w16(fill + 14u, static_cast<std::uint16_t>(kGuestHeight));

  const std::uint32_t mode = pool.cursor();
  core.r[16] = mode;
  core.r[17] = slot;
  core.mem_w32(sp + kDrawModeTw, 0u);
  tomba::guest::dispatchJalToReturn(
      core, kSetDrawMode, kReturnFromDrawMode, mode, 0u, 0u, blend == 0u ? kSubtractiveTpage : kAdditiveTpage);
  ot.link(mode, kModeWords, slot);
  pool.setCursor(pool.cursor() + kModeBytes);
  core.r[2] = pool.cursor();
  core.r[3] = PacketPool::kCursorPage;
  core.r[4] = ot.slot(slot);
}

void ScreenFade::registerOverrides() {
  tomba::native::declareOverride(kLeaf, "ScreenFade::draw", &drawEntry);
}
