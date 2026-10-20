#include "entry/settings.h"

#include "config_var.h"

namespace tomba {
namespace {

psx::config::Var<bool> cvNoFmv("PSXPORT_NO_FMV", false, psx::config::kRunOnly);
psx::config::Var<bool> cvAutoSkip("PSXPORT_AUTO_SKIP", false, psx::config::kRunOnly);
const psx::config::Registration kRegistration{cvNoFmv, cvAutoSkip};

} // namespace

Settings Settings::fromCVars() {
  Settings settings;
  settings.skipMovies = cvNoFmv.get();
  settings.autoSkip = cvAutoSkip.get();
  return settings;
}

} // namespace tomba
