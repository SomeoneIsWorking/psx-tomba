#include "tomba1/game/assets/gam_file.h"

#include <cstring>

namespace tomba1 {
namespace {

constexpr std::uint8_t kMagic[4] = {'G', 'A', 'M', 0};
constexpr std::size_t kHeaderBytes = 8;
constexpr int kItemsPerFlags = 16;

} // namespace

std::optional<std::vector<std::uint8_t>> unpackGam(std::span<const std::uint8_t> file) {
  if (file.size() < kHeaderBytes || std::memcmp(file.data(), kMagic, sizeof kMagic) != 0) {
    return std::nullopt;
  }
  std::uint32_t length = 0;
  std::memcpy(&length, file.data() + 4, sizeof length);
  std::vector<std::uint8_t> out;
  out.reserve(length);
  std::size_t at = kHeaderBytes;
  // A flag word's bit, low first: clear is one literal byte, set is a (distance, length) pair copying from the output.
  while (out.size() < length) {
    if (at + 2 > file.size()) {
      return std::nullopt;
    }
    const unsigned flags = file[at] | (static_cast<unsigned>(file[at + 1]) << 8);
    at += 2;
    for (int item = 0; item < kItemsPerFlags && out.size() < length; ++item) {
      if (((flags >> item) & 1u) == 0) {
        if (at >= file.size()) {
          return std::nullopt;
        }
        out.push_back(file[at++]);
        continue;
      }
      if (at + 2 > file.size()) {
        return std::nullopt;
      }
      const std::size_t distance = file[at];
      const std::size_t count = file[at + 1];
      at += 2;
      if (distance == 0 || distance > out.size()) {
        return std::nullopt;
      }
      for (std::size_t k = 0; k < count; ++k) {
        out.push_back(out[out.size() - distance]);
      }
    }
  }
  out.resize(length);
  return out;
}

} // namespace tomba1
