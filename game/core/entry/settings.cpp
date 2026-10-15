#include "entry/settings.h"

#include "config_var.h"

namespace tomba {
namespace {

psx::config::BoolVar cvNoFmv("PSXPORT_NO_FMV",
                             false,
                             "skip the native movies (boot cards and the opening OP.STR)",
                             /*persistable=*/false);
psx::config::BoolVar cvAutoSkip("PSXPORT_AUTO_SKIP",
                                false,
                                "drive the front end into GAME free-roam with scripted pad taps",
                                /*persistable=*/false);

} // namespace

Settings Settings::fromCVars() {
  Settings settings;
  settings.skipMovies = cvNoFmv.get();
  settings.autoSkip = cvAutoSkip.get();
  return settings;
}

} // namespace tomba
