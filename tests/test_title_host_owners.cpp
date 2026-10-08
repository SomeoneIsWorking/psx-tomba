// The catalog identities and the movie policy, through the shipping owners.
#include "entry/tomba_identity.h"
#include "frame/movie_policy.h"

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

  // The config layer caches its first read, so each environment is its own process.
  const std::string mode = argc > 1 ? argv[1] : "";
  if (mode == "skip") {
    setenv("PSXPORT_NO_FMV", "1", 1);
    check(!tomba::MoviePolicy::plays(false), "NO_FMV=1 skips movies");
  } else if (mode == "forced") {
    setenv("PSXPORT_NO_FMV", "0", 1);
    setenv("PSXPORT_VK_HEADLESS", "1", 1);
    check(tomba::MoviePolicy::plays(true), "NO_FMV=0 forces the headless-skipped movie on");
  } else if (mode == "headless") {
    setenv("PSXPORT_VK_HEADLESS", "1", 1);
    check(!tomba::MoviePolicy::plays(true), "headless skips a headless-skippable movie");
    check(tomba::MoviePolicy::plays(false), "headless alone does not skip the boot movie");
  }

  if (failed != 0) {
    std::printf("title host owners: %d failed\n", failed);
    return 1;
  }
  std::printf("title host owners: ok\n");
  return 0;
}
