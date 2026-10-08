#pragma once

namespace tomba {

// Whether a native movie plays: PSXPORT_NO_FMV skips it, an explicit PSXPORT_NO_FMV=0 forces it.
class MoviePolicy {
public:
  // `skipsWhenHeadless`: the headless sink (PSXPORT_VK_HEADLESS) also skips, unless forced on.
  static bool plays(bool skipsWhenHeadless);
};

} // namespace tomba
