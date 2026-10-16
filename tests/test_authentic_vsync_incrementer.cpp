// FUN_80050b08's init registers LAB_800506B4 in VSync callback slot 4 (0x80050BFC). The VBlank walker then advances the
// dwell counter once per field, which is the number FrameCadence reads.
#include "authenticated_image.h"
#include "core.h"
#include "core/frame/frame_cadence.h"
#include "core/hle/libapi_intr.h"
#include "game.h"
#include "guest_call.h"
#include "psx_exe_image.h"
#include "stub_game_runtime.h"

#include <cstdint>
#include <lucent/log.h>
#include <memory>

namespace {

inline constexpr std::uint32_t kStack = 0x801E0000u;
// libapi's callback setup (ResetCallback), the first library init FUN_80050b08 runs.
inline constexpr std::uint32_t kResetCallback = 0x80085B20u;
// The 8-slot VSync callback table FUN_80086288 walks.
inline constexpr std::uint32_t kCallbackSlot4 = 0x800ABDC0u + 4u * 4u;

bool check(bool condition, const char *description) {
  if (!condition) {
    lucent::error("vsync-incrementer-test", "failed: {}", description);
  }
  return condition;
}

} // namespace

int main(int argc, char **argv) {
  if (argc != 3) {
    lucent::error("vsync-incrementer-test", "expected MAIN.EXE path and manifest digest");
    return 2;
  }
  auto bytes =
      tomba::test::readAuthenticatedImage({.path = argv[1], .sha256 = argv[2], .maxBytes = psx::cpu::kPsxExeMaxBytes});
  if (!bytes) {
    return 2;
  }
  psx::test::StubGameRuntime runtime;
  auto game = std::make_unique<Game>(runtime, psx::config::Config{});
  Core &core = game->core;
  if (!check(psx::cpu::loadPsxExeImage(core, *bytes, "MAIN.EXE").identity.has_value(),
             "authenticated MAIN.EXE maps through the shipping loader")) {
    return 1;
  }
  core.r[29] = kStack;
  psx::cpu::callGuestNow(core, "vsync-incrementer-test", kResetCallback);
  if (!check(core.mem_r32(kCallbackSlot4) == 0u, "slot 4 is empty before the init registers it")) {
    return 1;
  }

  tomba::FrameCadence cadence;
  cadence.core = &core;
  cadence.installIncrementer();
  if (!check(core.mem_r32(kCallbackSlot4) == tomba::FrameCadence::kIncrementer, "slot 4 holds LAB_800506B4")) {
    return 1;
  }

  cadence.beginLogicFrame();
  for (std::uint8_t field = 0; field < tomba::FrameCadence::kRetailVblanksPerLogicFrame; ++field) {
    tomba::LibapiIntr::runVblankCallbacks(&core);
  }
  if (!check(cadence.dwellCounter() == tomba::FrameCadence::kRetailVblanksPerLogicFrame && cadence.gateOpen(),
             "two VBlank walks advance the dwell counter to the quota")) {
    return 1;
  }
  lucent::info("vsync-incrementer-test", "passed: slot 4 registered, 2 walks open the gate");
  return 0;
}
