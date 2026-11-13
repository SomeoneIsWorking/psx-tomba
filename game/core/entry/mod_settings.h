// mod_settings.h — Tomba! 2's optional gameplay mods: saved settings, each off by default.
#pragma once

#include "title_settings.h"

#include <cstdint>
#include <span>

namespace tomba {

// One owned-suit effect each; its setting lends that effect while the suit is in the inventory and not worn.
enum class SuitEffect : std::uint8_t {
  BirdFlutter,
  BirdAirControl,
  SquirrelGlide,
  SquirrelAirControl,
  SquirrelColdImmunity,
  PigJump,
  PigStomp,
  SwimmingPigSwim,
  SwimmingPigJump,
  SwimmingPigStomp,
  FlameRobeColdImmunity,
  SnowGrip,
  FreezeGuard,
  SquirrelUpdraft,
  FlameRobeFreezeImmunity,
};

// The settings menu's mod rows. Each read goes through psx::config::enh_int, so a comparison run sees every mod off.
class ModSettings {
public:
  // PSXPORT_TOMBA2_PIT_VOID_OUT: a fall onto a death floor voids out as a water fall does.
  static bool pitVoidOut();
  // PSXPORT_TOMBA2_SUIT_*: an owned suit lends this effect, worn or not.
  static bool suitEffect(SuitEffect effect);

  static std::span<const psx::game::TitleIntSetting> titleIntSettings();
};

} // namespace tomba
