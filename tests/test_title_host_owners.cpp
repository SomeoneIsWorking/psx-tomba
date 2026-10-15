// The catalog identities and the launch settings, through the shipping owners.
#include "entry/identity.h"
#include "entry/settings.h"

#include <cstdio>
#include <cstdlib>
#include <lucent/log.h>
#include <string>

namespace {

int failed = 0;

void check(bool condition, const char *name) {
  if (!condition) {
    ++failed;
    lucent::error("title-host-test", "failed: {}", name);
  }
}

void checkIdentity(const psx::host::TitleIdentity &id, const char *serial, const char *name) {
  check(id.serial == serial, name);
  check(id.slug == "tomba2", name);
  check(id.sha256.size() == 64u, name);
  check(id.fileSize != 0u && id.entry >= id.textAddress && id.entry < id.textAddress + id.textSize, name);
}

} // namespace

int main(int argc, char **argv) {
  checkIdentity(tomba::title::bootStubIdentity(), "SCUS_944.54", "boot stub identity");
  checkIdentity(tomba::title::mainExecutableIdentity(), "MAIN.EXE", "main executable identity");
  check(tomba::title::bootStubIdentity().sha256 != tomba::title::mainExecutableIdentity().sha256,
        "stub and main are distinct images");

  // A CVar binds its environment on first read, so each environment is its own process.
  const std::string mode = argc > 1 ? argv[1] : "";
  if (mode == "defaults") {
    const tomba::Settings settings = tomba::Settings::fromCVars();
    check(!settings.skipMovies && !settings.autoSkip, "movies play and nothing is driven by default");
  } else if (mode == "set") {
    setenv("PSXPORT_NO_FMV", "1", 1);
    setenv("PSXPORT_AUTO_SKIP", "1", 1);
    const tomba::Settings settings = tomba::Settings::fromCVars();
    check(settings.skipMovies && settings.autoSkip, "NO_FMV and AUTO_SKIP read as set");
  } else if (mode == "off") {
    setenv("PSXPORT_NO_FMV", "0", 1);
    const tomba::Settings settings = tomba::Settings::fromCVars();
    check(!settings.skipMovies, "NO_FMV=0 plays movies");
  }

  if (failed != 0) {
    std::printf("title host owners: %d failed\n", failed);
    return 1;
  }
  std::printf("title host owners: ok\n");
  return 0;
}
