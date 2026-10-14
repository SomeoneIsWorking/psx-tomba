// The guest-visible state a native body is compared on against the guest's: main RAM, scratchpad, registers.
#pragma once

#include "core.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <optional>
#include <vector>

namespace tomba::test {

struct GuestSnapshot {
  std::vector<std::uint8_t> ram;
  std::array<std::uint8_t, 0x400> scratch{};
  std::array<std::uint32_t, 32> r{};
};

inline GuestSnapshot capture(const Core &core) {
  GuestSnapshot state;
  state.ram.assign(core.ram, core.ram + sizeof(core.ram));
  std::memcpy(state.scratch.data(), core.scratch, sizeof(core.scratch));
  std::copy(std::begin(core.r), std::end(core.r), state.r.begin());
  return state;
}

inline void restore(Core &core, const GuestSnapshot &state) {
  std::memcpy(core.ram, state.ram.data(), sizeof(core.ram));
  std::memcpy(core.scratch, state.scratch.data(), sizeof(core.scratch));
  std::copy(state.r.begin(), state.r.end(), std::begin(core.r));
}

inline std::optional<std::uint32_t> firstDifference(const std::uint8_t *a, const std::uint8_t *b, std::size_t size) {
  for (std::size_t offset = 0; offset < size; ++offset) {
    if (a[offset] != b[offset]) {
      return static_cast<std::uint32_t>(offset);
    }
  }
  return std::nullopt;
}

} // namespace tomba::test
