// ScreenFade (FUN_8007E9C8): the fill and DR_MODE packets at 4:3 and across the record canvas.
#include "core/overrides/native_override_catalog.h"
#include "game.h"
#include "ordering_table.h"
#include "screen_fade.h"
#include "stub_runtime.h"
#include "wide_window.h"

#include <cstdint>
#include <lucent/log.h>
#include <memory>

namespace {

using tomba2::render::OrderingTable;
using tomba2::render::PacketPool;
using tomba2::wide_window::Window;

constexpr GuestAddressRange kResidentText{0x10000u, 0xBE800u};
constexpr std::uint32_t kSetDrawMode = 0x80083DE0u;
constexpr std::uint32_t kJrRa = 0x03E00008u;
constexpr std::uint32_t kOt = 0x801B0000u;
constexpr std::uint32_t kPool = 0x001A0000u; // retail keeps the cursor as a 24-bit address
constexpr std::uint32_t kStack = 0x801FFF00u;
constexpr std::uint32_t kSlot = 4u;
constexpr std::uint32_t kChainEnd = 0x00FFFFFFu;
constexpr std::uint32_t kColor = 0x00102030u;

int failed = 0;
int checked = 0;

void check(bool condition, const char *name) {
  ++checked;
  if (!condition) {
    ++failed;
    lucent::error("screen-fade-test", "failed: {}", name);
  }
}

std::uint32_t ram(std::uint32_t address) {
  return 0x80000000u | address;
}

void fade(Core &core, std::uint32_t blend, const Window &window) {
  core.mem_w32(OrderingTable::kBasePointer, kOt);
  core.mem_w32(kOt + kSlot * 4u, kChainEnd);
  PacketPool(core).setCursor(kPool);
  core.r[4] = kColor;
  core.r[5] = blend;
  core.r[6] = kSlot;
  core.r[16] = 0x16u;
  core.r[17] = 0x17u;
  core.r[29] = kStack;
  core.r[31] = 0x31u;
  ScreenFade::draw(core, window);
}

void testFade(Core &core, const Window &window, const char *name) {
  fade(core, ScreenFade::kSubtractive, window);
  const std::uint32_t mode = kPool + 16u;
  const auto width = static_cast<std::uint32_t>(window.right - window.left);
  const auto left = static_cast<std::uint16_t>(window.left);
  lucent::info("screen-fade-test", "{}: window [{}, {})", name, window.left, window.right);
  check(core.mem_r32(kOt + kSlot * 4u) == mode, "the OT slot heads the DR_MODE packet");
  check(core.mem_r32(ram(mode)) == (kPool | 0x02000000u), "the DR_MODE links to the fill with length 2");
  check(core.mem_r32(ram(kPool)) == (kChainEnd | 0x03000000u), "the fill links to the slot's prior head");
  check(core.mem_r32(ram(kPool + 4u)) == (0x62000000u | kColor), "the fill is a semi-transparent GP0 0x62 rect");
  check(core.mem_r32(ram(kPool + 8u)) == left, "the fill starts at the window's left column, line 0");
  check(core.mem_r32(ram(kPool + 12u)) == ((240u << 16) | width), "the fill spans the window, 240 lines");
  check(PacketPool(core).cursor() == kPool + 28u, "the pool advances past both packets");
  check(core.mem_r32(ScreenFade::kStage) == (0x62000000u | kColor) && core.mem_r32(ScreenFade::kStage + 4u) == 0u &&
            core.mem_r32(ScreenFade::kStage + 8u) == ((240u << 16) | 320u),
        "the scratchpad staging holds the guest's 320x240 rect");
  check(core.r[29] == kStack && core.r[16] == 0x16u && core.r[17] == 0x17u && core.r[31] == 0x31u,
        "the caller's sp, s0, s1 and ra survive");
  check(core.mem_r32(kStack - 0x28u + 0x10u) == 0u && core.mem_r32(kStack - 0x28u + 0x20u) == 0x31u,
        "the guest frame holds SetDrawMode's null texture window and the spilled ra");
}

void testAdditive(Core &core) {
  fade(core, ScreenFade::kAdditive, Window{});
  check(core.r[7] == 0x20u, "an additive fade asks SetDrawMode for tpage 0x20");
  fade(core, ScreenFade::kSubtractive, Window{});
  check(core.r[7] == 0x40u, "a subtractive fade asks SetDrawMode for tpage 0x40");
}

} // namespace

int main() {
  tomba::test::StubRuntime runtime;
  psxport_install_game(runtime);
  auto game = std::make_unique<Game>();
  Core &core = game->core;
  core.mem_w32(kSetDrawMode, kJrRa);
  core.mem_w32(kSetDrawMode + 4u, 0u);
  const auto resident = core.imageCatalog().activate("resident", kResidentText, 1u);
  tomba::native::bindResident(core, resident, kResidentText);

  testFade(core, Window{}, "4:3");
  testFade(core, Window{-54, 374}, "16:9 record canvas");
  testAdditive(core);
  lucent::info("screen-fade-test", "checked={} failed={}", checked, failed);
  return failed == 0 ? 0 : 1;
}
