#include "entry/mod_settings.h"

#include "config_var.h"
#include "enhancement_gate.h"

#include <array>

namespace tomba {
namespace {

psx::config::Var<long> cvPitVoidOut("PSXPORT_TOMBA2_PIT_VOID_OUT", 0, psx::config::kSaved);
const psx::config::Registration kRegistration{cvPitVoidOut};

const std::array<const char *, 2> kOffOn{"Off", "On"};
const std::array<psx::game::TitleIntSetting, 1> kTitleIntSettings{{
    {.id = "pit_void_out",
     .label = "Pits void out like water",
     .unit = "",
     .var = &cvPitVoidOut,
     .range = {.min = 0, .max = 1, .step = 1},
     .labels = kOffOn},
}};

} // namespace

bool ModSettings::pitVoidOut() {
  return psx::config::enh_int(cvPitVoidOut) != 0;
}

std::span<const psx::game::TitleIntSetting> ModSettings::titleIntSettings() {
  return kTitleIntSettings;
}

} // namespace tomba
