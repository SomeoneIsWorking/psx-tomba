// game/render/libgpu_draw_mode.h — libgpu SetDrawMode (0x80083DE0): the DR_MODE packet builder.
#pragma once

#include "emit_memory.h"

#include <cstdint>

namespace tomba2::render {

// Fills the two-word packet at `packet` with a GP0(E1) draw mode (and GP0(E2) when `window` names a RECT).
// Returns what the guest function leaves in v0.
std::uint32_t setDrawMode(const EmitMemory &memory,
                          std::uint32_t packet,
                          std::uint32_t drawOnDisplay,
                          std::uint32_t dither,
                          std::uint32_t texturePage,
                          std::uint32_t window);

} // namespace tomba2::render
