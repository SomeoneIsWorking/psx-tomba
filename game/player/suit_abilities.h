// game/player/suit_abilities.h — the ability gates on Tomba's suit word G+0x17E and the owned-suit effect settings.
#pragma once

#include "entry/mod_settings.h"

#include <cstdint>

class Core;

namespace tomba {

// Tomba_suitWord (0x80067EF4) derives G+0x17E from the worn item G+0x6F: its suit index in the low nibble and one
// bit per suit (0x10 none, 0x20 Bird, 0x40 Squirrel, 0x80 Pig, 0x100 Swimming Pig, 0x400.. the robes). Each guest gate
// tests a mask of those bits; an ability names that gate. A worn suit passes as in retail; an owned one passes only
// for the effect its own setting (ModSettings::suitEffect) turns on. The word is never written.
enum class SuitAbility : std::uint8_t {
  Flutter,      // 0x20: the Bird's fall flutter
  AirControl,   // 0x60: the winged rise and air steer (Bird, Squirrel)
  Glide,        // 0x40: the Squirrel's glide
  ColdImmunity, // 0x640: the cold idle skipped (Squirrel, Clear Fruit, Evil Flame Pig Robe)
  PigJump,      // 0x180: the Pig suits' launch, rise and release brake
  Stomp,        // 0x180: the Pig suits' air stomp
  Swim,         // 0x100: the Swimming Pig Suit's swim
};

class SuitAbilities {
public:
  // True when the worn suit, or an owned one whose effect setting is on, gives `ability`.
  static bool has(Core &core, SuitAbility ability);
  // The light-clothes handicaps (0x30: no suit or the Bird), each its own effect: the snow slip (SnowGrip) and area
  // 4's freezing touch (FreezeGuard). True when worn, unless `effect` is on and the inventory holds a heavier suit.
  static bool lightlyDressed(Core &core, SuitEffect effect);
  // The guest mask a gate tests, for its log line.
  static std::uint16_t mask(SuitAbility ability);
};

} // namespace tomba
