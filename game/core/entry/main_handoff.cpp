#include "entry/main_handoff.h"

#include "disc_executable.h"
#include "entry/identity.h"
#include "game.h"

#include <cstdlib>
#include <lucent/log.h>

namespace tomba {

void loadMainExecutable(Game &game) {
  const psx::cpu::PsxExeLoadResult loaded = psx::game::loadDiscExecutable(game, title::mainExecutableIdentity());
  if (!loaded) {
    lucent::error("tomba-handoff", "cannot boot MAIN.EXE from the disc: {}", loaded.detail);
    std::abort();
  }
  lucent::info("tomba-handoff",
               "loaded MAIN.EXE from the disc: entry 0x{:08X} text 0x{:08X}+0x{:X}",
               loaded.image.entry,
               loaded.image.textAddress,
               loaded.image.textBytes);
}

} // namespace tomba
