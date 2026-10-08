// The authentic FUN_8007E9C8 links a fill and a DR_MODE into the caller's OT slot; the native ScreenFade::draw
// writes the same RAM and scratchpad at 4:3 and widens only the fill across the record canvas.
#include "authenticated_image.h"
#include "core.h"
#include "core/overrides/native_override_catalog.h"
#include "game.h"
#include "psx_exe_image.h"
#include "render/ordering_table.h"
#include "render/screen_fade.h"
#include "render/wide_window.h"
#include "stub_runtime.h"

#include <cstdint>
#include <cstring>
#include <lucent/log.h>
#include <memory>
#include <vector>

namespace {

inline constexpr std::uint32_t kPoolCursor = tomba2::render::PacketPool::kCursor;
inline constexpr std::uint32_t kOtPointer = tomba2::render::OrderingTable::kBasePointer;
inline constexpr std::uint32_t kPool = 0x001C0000u; // retail keeps the cursor as a 24-bit address
inline constexpr std::uint32_t kOt = 0x801D0000u;
inline constexpr std::uint32_t kOtSlots = 8u;
inline constexpr std::uint32_t kOtEnd = 0x00FFFFFFu;
inline constexpr std::uint32_t kStack = 0x801E0000u;

bool check(bool condition, const char *description) {
  if (!condition) {
    lucent::error("screen-fade-test", "failed: {}", description);
  }
  return condition;
}

std::uint32_t ram(std::uint32_t address) {
  return 0x80000000u | address;
}

// Expects the two packets the leaf writes at `packet`, chained in front of `previous` in `slot`.
bool checkFade(Core &core,
               std::uint32_t packet,
               std::uint32_t previous,
               std::uint32_t slot,
               std::uint32_t color,
               std::uint32_t tpage) {
  const std::uint32_t mode = packet + 16u;
  return check(core.mem_r32(kOt + slot * 4u) == mode, "OT slot heads the DR_MODE packet") &&
         check(core.mem_r32(ram(mode)) == (packet | 0x02000000u), "DR_MODE links to the fill with length 2") &&
         check(core.mem_r32(ram(mode + 4u)) == (0xE1000000u | tpage), "DR_MODE carries the blend tpage") &&
         check(core.mem_r32(ram(mode + 8u)) == 0u, "DR_MODE has no texture window") &&
         check(core.mem_r32(ram(packet)) == (previous | 0x03000000u), "fill links to the slot's prior head") &&
         check(core.mem_r32(ram(packet + 4u)) == (0x62000000u | color), "fill is a semi-transparent GP0 0x62 rect") &&
         check(core.mem_r32(ram(packet + 8u)) == 0u, "fill starts at the origin") &&
         check(core.mem_r32(ram(packet + 12u)) == ((240u << 16) | 320u), "fill covers 320x240") &&
         check(core.mem_r32(kPoolCursor) == packet + 28u, "pool cursor advances past both packets");
}

struct Snapshot {
  std::vector<std::uint8_t> ram;
  std::vector<std::uint8_t> scratch;
};

Snapshot snapshot(const Core &core) {
  return {std::vector<std::uint8_t>(core.ram, core.ram + sizeof(core.ram)),
          std::vector<std::uint8_t>(core.scratch, core.scratch + sizeof(core.scratch))};
}

void restore(Core &core, const Snapshot &state) {
  std::memcpy(core.ram, state.ram.data(), sizeof(core.ram));
  std::memcpy(core.scratch, state.scratch.data(), sizeof(core.scratch));
}

std::size_t differingBytes(const std::vector<std::uint8_t> &a, const std::vector<std::uint8_t> &b) {
  std::size_t count = 0;
  for (std::size_t index = 0; index < a.size(); ++index) {
    count += a[index] != b[index] ? 1u : 0u;
  }
  return count;
}

void arguments(Core &core, std::uint32_t color, std::uint32_t blend) {
  core.r[4] = color;
  core.r[5] = blend;
  core.r[6] = 2u;
  core.r[29] = kStack;
}

// The native leaf from the same state as the guest: identical bytes at 4:3; on the 16:9 canvas only the
// fill's x and width differ.
bool checkNative(Core &core, psx::cpu::ImageIdentity resident, std::uint32_t blend) {
  arguments(core, 0x0A0B0Cu, blend);
  const Snapshot before = snapshot(core);
  core.r[31] = 0x80010100u;
  const auto guest =
      psx::cpu::callOriginal(core, {resident, ScreenFade::kLeaf}, psx::cpu::ExecutionBudget::fromCycles(1000000u));
  const Snapshot authentic = snapshot(core);
  const std::uint32_t guestV0 = core.r[2];

  restore(core, before);
  arguments(core, 0x0A0B0Cu, blend);
  ScreenFade::draw(core, tomba2::wide_window::Window{});
  const Snapshot native = snapshot(core);
  if (!check(guest.returned(), "the guest leaf returns") ||
      !check(differingBytes(authentic.ram, native.ram) == 0u &&
                 differingBytes(authentic.scratch, native.scratch) == 0u && core.r[2] == guestV0,
             "the native leaf writes the guest's RAM, scratchpad and v0 at 4:3")) {
    return false;
  }

  restore(core, before);
  arguments(core, 0x0A0B0Cu, blend);
  ScreenFade::draw(core, tomba2::wide_window::Window{-54, 374});
  const Snapshot wide = snapshot(core);
  const std::uint32_t fill = (core.mem_r32(kPoolCursor) - 28u) & 0x1FFFFFu;
  Snapshot widened = authentic;
  widened.ram[fill + 8u] = 0xCAu; // x = -54
  widened.ram[fill + 9u] = 0xFFu;
  widened.ram[fill + 12u] = 0xACu; // width = 428
  widened.ram[fill + 13u] = 0x01u;
  return check(differingBytes(widened.ram, wide.ram) == 0u && differingBytes(authentic.scratch, wide.scratch) == 0u,
               "on the 16:9 canvas the fill alone widens, to columns -54..374");
}

} // namespace

int main(int argc, char **argv) {
  if (argc != 3) {
    lucent::error("screen-fade-test", "expected MAIN.EXE path and manifest digest");
    return 2;
  }
  auto bytes = tomba::test::readAuthenticatedImage(argv[1], argv[2], psx::cpu::kPsxExeMaxBytes);
  if (!bytes) {
    return 2;
  }
  tomba::test::StubRuntime runtime;
  psxport_install_game(runtime);
  auto game = std::make_unique<Game>();
  Core &core = game->core;
  auto loaded = psx::cpu::loadPsxExeImage(core, *bytes, "MAIN.EXE");
  if (!check(static_cast<bool>(loaded), "authenticated MAIN.EXE maps through the shipping loader")) {
    return 1;
  }
  core.r[29] = kStack;
  core.mem_w32(kPoolCursor, kPool);
  core.mem_w32(kOtPointer, kOt);
  for (std::uint32_t slot = 0u; slot < kOtSlots; ++slot) {
    core.mem_w32(kOt + slot * 4u, kOtEnd);
  }

  ScreenFade fade;
  fade.core = &core;
  fade.applyLeafCall(0x102030u, ScreenFade::kSubtractive, 2u);
  if (!checkFade(core, kPool, kOtEnd, 2u, 0x102030u, 0x40u)) {
    return 1;
  }
  fade.applyLeafCall(0xF0F0F0u, ScreenFade::kAdditive, 2u);
  if (!checkFade(core, kPool + 28u, kPool + 16u, 2u, 0xF0F0F0u, 0x20u)) {
    return 1;
  }
  for (std::uint32_t slot = 0u; slot < kOtSlots; ++slot) {
    if (slot != 2u && !check(core.mem_r32(kOt + slot * 4u) == kOtEnd, "other OT slots stay empty")) {
      return 1;
    }
  }
  if (!check(core.r[29] == kStack, "guest call returns with the caller's stack")) {
    return 1;
  }
  const psx::cpu::ImageIdentity resident = *loaded.identity;
  const std::uint32_t text = loaded.image.textAddress & 0x1FFFFFFFu;
  ScreenFade::registerOverrides();
  tomba::native::bindResident(core, resident, {text, text + loaded.image.textBytes});
  if (!checkNative(core, resident, ScreenFade::kSubtractive) || !checkNative(core, resident, ScreenFade::kAdditive)) {
    return 1;
  }
  lucent::info("screen-fade-test", "passed: 2 leaf calls, packets in OT slot 2 only; native matches the guest at 4:3");
  return 0;
}
