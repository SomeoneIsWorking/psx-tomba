// tomba_settings.h — Tomba! 2's launch knobs, read once through the CVar registry.
#pragma once

namespace tomba {

class TombaSettings {
public:
  // Snapshot of the title's CVars (environment, settings file and defaults already layered).
  static TombaSettings fromCVars();

  bool skipMovies = false; // PSXPORT_NO_FMV
  bool autoSkip = false;   // PSXPORT_AUTO_SKIP: front end driven into GAME free-roam
};

} // namespace tomba
