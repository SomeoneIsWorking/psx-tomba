// The double-buffered GPU packet pool the frame's submitters append to through the cursor 0x1F800164.
#pragma once

#include <cstdint>

class Core;

namespace tomba1 {

class PacketPool {
public:
  // 0x8001936C: cursor = (0x800B3188 + buffer * 0xC000) & 0xFFFFFF, buffer the halfword at 0x1F8001F4.
  static constexpr std::uint32_t kBase = 0x000B3188u;
  static constexpr std::uint32_t kPassBytes = 0xC000u;
  static constexpr std::uint32_t kPasses = 2u;
  static constexpr std::uint32_t kCursor = 0x1F800164u;

  // The packet at the cursor; aborts when `bytes` from it would pass the end of the pass it is in.
  static std::uint32_t reserve(Core &core, std::uint32_t bytes);
};

} // namespace tomba1
