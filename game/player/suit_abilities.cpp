#include "player/suit_abilities.h"

#include "core.h"
#include "entry/mod_settings.h"
#include "items/inventory_layout.h"
#include "player/actor_tomba.h"

namespace tomba {
namespace {

constexpr std::uint32_t kSuitWord = player::ActorTomba::G_ADDR + 0x17Eu;
// The item ids Tomba_suitWord maps: Bird Clothes 0x0E through the Evil Water Pig Robe 0x16.
constexpr std::uint8_t kFirstSuit = 0x0Eu;
constexpr std::uint8_t kLastSuit = 0x16u;

} // namespace

std::uint16_t SuitAbilities::abilityBits(std::uint8_t item) {
  switch (item) {
  case 0x0Eu:
    return 0x20u;
  case 0x0Fu:
    return 0x40u;
  case 0x10u:
    return 0x80u;
  case 0x11u:
    return 0x100u;
  case 0x12u:
    return 0x400u;
  case 0x13u:
    return 0x800u;
  case 0x14u:
    return 0x1000u;
  case 0x15u:
    return 0x2000u;
  case 0x16u:
    return 0x4000u;
  default:
    return 0u;
  }
}

bool SuitAbilities::has(Core &core, std::uint16_t bits) {
  if ((core.mem_r16(kSuitWord) & bits) != 0u) {
    return true;
  }
  if (!ModSettings::permanentSuits()) {
    return false;
  }
  for (std::uint8_t item = kFirstSuit; item <= kLastSuit; ++item) {
    if (core.mem_r8(inventory_layout::kCounts + item) != 0u && (abilityBits(item) & bits) != 0u) {
      return true;
    }
  }
  return false;
}

bool SuitAbilities::burdened(Core &core, std::uint16_t bits) {
  if ((core.mem_r16(kSuitWord) & bits) == 0u) {
    return false;
  }
  if (!ModSettings::permanentSuits()) {
    return true;
  }
  for (std::uint8_t item = kFirstSuit; item <= kLastSuit; ++item) {
    if (core.mem_r8(inventory_layout::kCounts + item) != 0u && (abilityBits(item) & bits) == 0u) {
      return false;
    }
  }
  return true;
}

} // namespace tomba
