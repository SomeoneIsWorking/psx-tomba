// Guest bytes and halfwords as the values `lb` and `lh` load, in the guest's wrapping 32-bit arithmetic.
#pragma once

#include <cstdint>

namespace tomba1 {

constexpr std::uint32_t sext8(std::uint8_t value) {
  return (std::uint32_t{value} ^ 0x80u) - 0x80u;
}

constexpr std::uint32_t sext16(std::uint32_t value) {
  return ((value & 0xFFFFu) ^ 0x8000u) - 0x8000u;
}

} // namespace tomba1
