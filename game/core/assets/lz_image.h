// The texture-group image LZ (FUN_80044D8C), over any source and output: guest memory for the engine's loader, a
// host buffer for the selector's art.
#pragma once

#include <array>
#include <cstdint>
#include <optional>

namespace tomba {

// (base, factor) per mode, the table at 0x800153C8: a back-reference reads at base + 2 * factor * row stride.
inline constexpr std::array<std::array<std::int32_t, 2>, 8> kLzImageOffsets{
    {{0, 0}, {-1, 0}, {0, -1}, {-1, -1}, {-2, -1}, {-3, -1}, {1, -1}, {2, -1}}};

inline constexpr std::int32_t lzImageOffset(int mode, std::int32_t strideWords) {
  const std::array<std::int32_t, 2> &entry = kLzImageOffsets[static_cast<std::size_t>(mode)];
  return entry[0] + 2 * (entry[1] * strideWords);
}

// A control byte is count << 3 | mode. Mode 0 copies `count` literal bytes and a count of 0 ends the image; modes
// 1..7 copy `count` bytes, one at a time, from the output at the mode's offset, so an overlapping copy repeats.
// `Source`: `bool more()`, `std::uint8_t next()`. `Output`: `std::int64_t size()`,
// `std::optional<std::uint8_t> at(std::int64_t index)`, `void push(std::uint8_t)`. False when the output refuses a
// back-reference.
template <class Source, class Output> bool decodeLzImage(Source &in, Output &out, std::int32_t strideWords) {
  while (in.more()) {
    const std::uint8_t control = in.next();
    const std::uint32_t count = control >> 3;
    const int mode = control & 7;
    if (mode == 0) {
      if (count == 0) {
        return true;
      }
      for (std::uint32_t k = 0; k < count && in.more(); ++k) {
        out.push(in.next());
      }
      continue;
    }
    const std::int32_t offset = lzImageOffset(mode, strideWords);
    for (std::uint32_t k = 0; k < count; ++k) {
      const std::optional<std::uint8_t> byte = out.at(out.size() + offset);
      if (!byte) {
        return false;
      }
      out.push(*byte);
    }
  }
  return true;
}

} // namespace tomba
