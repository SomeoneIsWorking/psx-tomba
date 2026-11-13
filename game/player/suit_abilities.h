// game/player/suit_abilities.h — the ability bits of Tomba's suit word G+0x17E and the permanent-suits mod.
#pragma once

#include <cstdint>

class Core;

namespace tomba {

// Tomba_suitWord (0x80067EF4) derives G+0x17E from the worn item G+0x6F: its suit index in the low nibble and one
// ability bit per suit (0x40 the Squirrel Clothes' glide and cold immunity, 0x100 the Swimming Pig Suit's swim).
// Under ModSettings::permanentSuits a gate also passes on a suit the inventory holds; the word is never written.
class SuitAbilities {
public:
  // The ability bits the worn item gives; 0 for an item that is not a suit.
  static std::uint16_t abilityBits(std::uint8_t item);
  // True when the worn suit, or under the mod an owned one, carries any of `bits`.
  static bool has(Core &core, std::uint16_t bits);
  // True when the worn word carries any of `bits` and, under the mod, no owned suit is free of them: a handicap
  // gate (0x30, no suit or the Bird Clothes slipping on snow) that any owned suit without it lifts.
  static bool burdened(Core &core, std::uint16_t bits);
};

} // namespace tomba
