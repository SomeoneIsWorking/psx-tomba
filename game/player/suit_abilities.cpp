#include "player/suit_abilities.h"

#include "core.h"
#include "entry/mod_settings.h"
#include "items/inventory_layout.h"
#include "player/actor_tomba.h"

#include <array>
#include <cstddef>
#include <span>

namespace tomba {
namespace {

constexpr std::uint32_t kSuitWord = player::ActorTomba::G_ADDR + 0x17Eu;

constexpr std::uint8_t kBird = 0x0Eu;
constexpr std::uint8_t kSquirrel = 0x0Fu;
constexpr std::uint8_t kPig = 0x10u;
constexpr std::uint8_t kSwimmingPig = 0x11u;
constexpr std::uint8_t kFlameRobe = 0x12u;
constexpr std::uint8_t kLastRobe = 0x16u; // the Evil Water Pig Robe

constexpr std::uint16_t kSnowSlip = 0x30u;

struct Grant {
  std::uint8_t item;
  SuitEffect effect;
};

struct Gate {
  std::uint16_t mask;
  std::span<const Grant> grants;
};

constexpr std::array<Grant, 1> kFlutter{{{kBird, SuitEffect::BirdFlutter}}};
constexpr std::array<Grant, 2> kAirControl{
    {{kBird, SuitEffect::BirdAirControl}, {kSquirrel, SuitEffect::SquirrelAirControl}}};
constexpr std::array<Grant, 1> kGlide{{{kSquirrel, SuitEffect::SquirrelGlide}}};
constexpr std::array<Grant, 2> kCold{
    {{kSquirrel, SuitEffect::SquirrelColdImmunity}, {kFlameRobe, SuitEffect::FlameRobeColdImmunity}}};
constexpr std::array<Grant, 2> kPigJump{{{kPig, SuitEffect::PigJump}, {kSwimmingPig, SuitEffect::SwimmingPigJump}}};
constexpr std::array<Grant, 2> kStomp{{{kPig, SuitEffect::PigStomp}, {kSwimmingPig, SuitEffect::SwimmingPigStomp}}};
constexpr std::array<Grant, 1> kSwim{{{kSwimmingPig, SuitEffect::SwimmingPigSwim}}};

// In SuitAbility order.
constexpr std::array<Gate, 7> kGates{{
    {0x20u, kFlutter},
    {0x60u, kAirControl},
    {0x40u, kGlide},
    {0x640u, kCold},
    {0x180u, kPigJump},
    {0x180u, kStomp},
    {0x100u, kSwim},
}};

const Gate &gate(SuitAbility ability) {
  return kGates[static_cast<std::size_t>(ability)];
}

bool owned(Core &core, std::uint8_t item) {
  return core.mem_r8(inventory_layout::kCounts + item) != 0u;
}

} // namespace

bool SuitAbilities::has(Core &core, SuitAbility ability) {
  const Gate &g = gate(ability);
  if ((core.mem_r16(kSuitWord) & g.mask) != 0u) {
    return true;
  }
  for (const Grant &grant : g.grants) {
    if (owned(core, grant.item) && ModSettings::suitEffect(grant.effect)) {
      return true;
    }
  }
  return false;
}

bool SuitAbilities::slipsOnSnow(Core &core) {
  if ((core.mem_r16(kSuitWord) & kSnowSlip) == 0u) {
    return false;
  }
  if (!ModSettings::suitEffect(SuitEffect::SnowGrip)) {
    return true;
  }
  for (std::uint8_t item = kSquirrel; item <= kLastRobe; ++item) {
    if (owned(core, item)) {
      return false;
    }
  }
  return true;
}

std::uint16_t SuitAbilities::mask(SuitAbility ability) {
  return gate(ability).mask;
}

} // namespace tomba
