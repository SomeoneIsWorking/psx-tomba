#pragma once

#include "core.h"

#include <cstdint>

namespace tomba {

// The pad edge words FUN_800788AC (Engine::padEdgeFence) writes once per logic frame.
class PadEdges {
public:
  // 0x800E7E68: u16, the buttons newly pressed this frame (cur & ~prev).
  inline static constexpr std::uint32_t kPressed = 0x800E7E68u;
  inline static constexpr std::uint16_t kSelect = 0x0001u;
  inline static constexpr std::uint16_t kStart = 0x0008u;
  inline static constexpr std::uint16_t kRight = 0x0020u;
  inline static constexpr std::uint16_t kLeft = 0x0080u;
  inline static constexpr std::uint16_t kCircle = 0x2000u;
  inline static constexpr std::uint16_t kCross = 0x4000u;

  static bool startPressed(Core &core) {
    return (core.mem_r16(kPressed) & kStart) != 0;
  }
};

} // namespace tomba
