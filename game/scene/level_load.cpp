#include "level_load.h"

#include "core.h"
#include "core/overrides/native_override_catalog.h"
#include "game.h"

#include <array>
#include <cstdlib>
#include <lucent/log.h>
#include <string_view>

namespace tomba::stage {
namespace {

constexpr std::array<std::string_view, 3> kNames{"START", "DEMO", "GAME"};
constexpr std::uint32_t kRamBytes = 0x00200000u;

void requireImageRange(std::uint32_t stage, std::uint32_t size) {
  if (stage >= kNames.size() || size == 0u || size > kRamBytes - kSlot) {
    lucent::error("tomba-stage", "refused invalid stage {} image size {}", stage, size);
    std::abort();
  }
}

} // namespace

psx::cpu::ImageIdentity
activateLoaded(Core &core, std::optional<psx::cpu::ImageIdentity> &active, std::uint32_t stage, std::uint32_t size) {
  requireImageRange(stage, size);
  return native::activateOverlay(core, active, kNames[stage], {kSlot, kSlot + size});
}

void loadOverlay(Core &core,
                 std::optional<psx::cpu::ImageIdentity> &active,
                 std::uint32_t taskFields,
                 std::uint32_t stage) {
  if (stage > kNames.size()) {
    lucent::error("tomba-stage", "refused invalid stage index {}", stage);
    std::abort();
  }

  if (stage == kNames.size()) {
    native::retireOverlay(core, active);
  } else {
    const std::uint32_t lba = core.mem_r32(kFileTable + stage * 8u);
    const std::uint32_t size = core.mem_r32(kFileTable + stage * 8u + 4u);
    requireImageRange(stage, size);
    core.game->cd.loadFile(0x80000000u | kSlot, lba, size);
    activateLoaded(core, active, stage, size);
  }

  const std::uint32_t entry = core.mem_r32(kEntryTable + stage * 4u);
  core.mem_w32(taskFields, entry);
  core.mem_w32(taskFields + 4u, core.r[28]); // FUN_80080930 returns the caller's gp.
}

} // namespace tomba::stage
