#include "entry/mod_settings.h"

#include "config_var.h"
#include "enhancement_gate.h"

#include <array>

namespace tomba {
namespace {

psx::config::Var<long> cvPitVoidOut("PSXPORT_TOMBA2_PIT_VOID_OUT", 0, psx::config::kSaved);
psx::config::Var<long> cvPermanentSuits("PSXPORT_TOMBA2_PERMANENT_SUITS", 0, psx::config::kSaved);
const psx::config::Registration kRegistration{cvPitVoidOut, cvPermanentSuits};

const std::array<const char *, 2> kOffOn{"Off", "On"};
const std::array<psx::game::TitleIntSetting, 2> kTitleIntSettings{{
    {.id = "pit_void_out",
     .label = "Pits void out like water",
     .unit = "",
     .var = &cvPitVoidOut,
     .range = {.min = 0, .max = 1, .step = 1},
     .labels = kOffOn},
    {.id = "permanent_suits",
     .label = "Owned suits keep their abilities",
     .unit = "",
     .var = &cvPermanentSuits,
     .range = {.min = 0, .max = 1, .step = 1},
     .labels = kOffOn},
}};

} // namespace

bool ModSettings::pitVoidOut() {
  return psx::config::enh_int(cvPitVoidOut) != 0;
}

bool ModSettings::permanentSuits() {
  return psx::config::enh_int(cvPermanentSuits) != 0;
}

std::span<const psx::game::TitleIntSetting> ModSettings::titleIntSettings() {
  return kTitleIntSettings;
}

} // namespace tomba
