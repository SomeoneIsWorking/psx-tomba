#include "frame/movie_policy.h"

#include "cfg.h"

#include <cstdlib>

namespace tomba {

bool MoviePolicy::plays(bool skipsWhenHeadless) {
  const char *override = cfg_str("PSXPORT_NO_FMV");
  if (override != nullptr && *override != '\0' && std::atoi(override) == 0) {
    return true;
  }
  return !cfg_on("PSXPORT_NO_FMV") && !(skipsWhenHeadless && cfg_on("PSXPORT_VK_HEADLESS"));
}

} // namespace tomba
