#include "entry/main_handoff.h"

#include "core.h"
#include "disc_file.h"
#include "entry/tomba_identity.h"
#include "game.h"
#include "psx_exe_image.h"
#include "title_selection.h"

#include <cstdint>
#include <cstdlib>
#include <lucent/log.h>
#include <span>
#include <vector>

namespace tomba {

void loadMainExecutable(Game &game) {
  const psx::host::TitleIdentity &identity = title::mainExecutableIdentity();
  std::vector<std::uint8_t> bytes;
  if (!psx::cd::readDiscFile(game.disc, "\\MAIN.EXE;1", bytes)) {
    lucent::error("tomba-handoff", "MAIN.EXE is not readable from the disc");
    std::abort();
  }
  const psx::host::SelectionResult selection = psx::host::selectExecutable(identity.serial, bytes, {&identity, 1});
  if (!selection) {
    lucent::error("tomba-handoff", "the disc's MAIN.EXE is not the supported revision: {}", selection.detail);
    std::abort();
  }
  const psx::cpu::PsxExeLoadResult loaded = psx::cpu::loadPsxExeImage(game.core, bytes, identity.serial);
  if (!loaded) {
    lucent::error("tomba-handoff", "cannot load MAIN.EXE: {}", loaded.detail);
    std::abort();
  }
  psx::cpu::applyPsxExeTopLevelRegisters(game.core, loaded.image);
  lucent::info("tomba-handoff",
               "loaded MAIN.EXE from the disc: entry 0x{:08X} text 0x{:08X}+0x{:X}",
               loaded.image.entry,
               loaded.image.textAddress,
               loaded.image.textBytes);
}

} // namespace tomba
