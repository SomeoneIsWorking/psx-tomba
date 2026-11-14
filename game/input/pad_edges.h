#pragma once

#include "core.h"

#include <cstdint>

namespace tomba {

// The pad edge words FUN_800788AC (Engine::padEdgeFence) writes once per logic frame.
class PadEdges {
public:
  // 0x800E7E68: u16, the buttons newly pressed this frame (cur & ~prev).
  inline static constexpr std::uint32_t kPressed = 0x800E7E68u;
  inline static constexpr std::uint16_t kStart = 0x0008u;

  static bool startPressed(Core &core) {
    return (core.mem_r16(kPressed) & kStart) != 0;
  }
};

} // namespace tomba
