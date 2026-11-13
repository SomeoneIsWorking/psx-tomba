// mod_settings.h — Tomba! 2's optional gameplay mods: saved settings, each off by default.
#pragma once

#include "title_settings.h"

#include <span>

namespace tomba {

// The settings menu's mod rows. Each read goes through psx::config::enh_int, so a comparison run sees every mod off.
class ModSettings {
public:
  // PSXPORT_TOMBA2_PIT_VOID_OUT: a fall onto a death floor voids out as a water fall does.
  static bool pitVoidOut();
  // PSXPORT_TOMBA2_PERMANENT_SUITS: every suit in the inventory lends its ability, worn or not.
  static bool permanentSuits();

  static std::span<const psx::game::TitleIntSetting> titleIntSettings();
};

} // namespace tomba
