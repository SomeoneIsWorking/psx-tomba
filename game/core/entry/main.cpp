// game/core/entry/main.cpp — the Tomba process entry point: the multi-title host over this repository's catalog.
#include "entry/tomba_catalog.h"
#include "product_host.h"

#include <cstring>
#include <lucent/log.h>

namespace {

constexpr const char *kProvisioningRoot = "scratch/bin";

bool helpRequested(int argc, char **argv) {
  return argc == 2 && (std::strcmp(argv[1], "-h") == 0 || std::strcmp(argv[1], "--help") == 0);
}

} // namespace

int main(int argc, char **argv) {
  if (helpRequested(argc, argv)) {
    lucent::info("cli",
                 "Usage: tomba2_port [executable]\n"
                 "With no argument: the title selector, then the chosen Tomba title, in this process.\n"
                 "With an executable: run exactly that serial-identified boot executable (SCUS_944.54).");
    return 0;
  }
  const tomba::TombaCatalog catalog;
  psx::host::ProductHost host(catalog, kProvisioningRoot);
  return argc > 1 ? host.runExecutable(argv[1]) : host.runSelector();
}
