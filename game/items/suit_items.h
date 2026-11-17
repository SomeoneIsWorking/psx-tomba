// game/items/suit_items.h — the suit item ids: the worn item G+0x6F and the inventory count index.
#pragma once

#include <cstdint>

namespace tomba::suit_items {

inline constexpr std::uint8_t kBird = 0x0Eu;
inline constexpr std::uint8_t kSquirrel = 0x0Fu;
inline constexpr std::uint8_t kPig = 0x10u;
inline constexpr std::uint8_t kSwimmingPig = 0x11u;
inline constexpr std::uint8_t kFlameRobe = 0x12u;
inline constexpr std::uint8_t kLastRobe = 0x16u; // the Evil Water Pig Robe

// Every suit heavier than the light clothes (no suit or the Bird).
constexpr bool heavier(std::uint8_t item) {
  return item >= kSquirrel && item <= kLastRobe;
}

} // namespace tomba::suit_items
