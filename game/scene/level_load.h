#pragma once

#include "image_identity.h"

#include <cstdint>
#include <optional>

class Core;

namespace tomba::stage {

inline constexpr std::uint32_t kSlot = 0x00106228u;
inline constexpr std::uint32_t kEntryTable = 0x800a3eccu;
inline constexpr std::uint32_t kFileTable = 0x800be1e0u;

// START, DEMO, and GAME reuse one code slot. The caller owns its current image
// token so a stage replacement retires its old native dispatch and JIT identity.
psx::cpu::ImageIdentity
activateLoaded(Core &core, std::optional<psx::cpu::ImageIdentity> &active, std::uint32_t stage, std::uint32_t size);

// Load the selected stage, publish its residency, and write the task's entry PC
// and caller gp. Stage 3 returns to resident code without loading a file.
void loadOverlay(Core &core,
                 std::optional<psx::cpu::ImageIdentity> &active,
                 std::uint32_t taskFields,
                 std::uint32_t stage);

} // namespace tomba::stage
