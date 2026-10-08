#include "authenticated_image.h"
#include "core/assets/str.h"
#include "core/overrides/native_override_catalog.h"
#include "game.h"
#include "lightrec_executor.h"
#include "psx_exe_image.h"
#include "stub_runtime.h"

#include <array>
#include <lucent/log.h>
#include <memory>

namespace {

inline constexpr std::uint32_t kLength = 0x80079528u;
inline constexpr std::uint32_t kReturn = 0x80010100u;
inline constexpr std::uint32_t kString = 0x800f0000u;
inline constexpr std::uint32_t kStack = 0x801e0000u;
inline constexpr auto kBudget = psx::cpu::ExecutionBudget::fromCycles(20000u);

bool check(bool condition, const char *description) {
  if (!condition) {
    lucent::error("real-resident-test", "failed: {}", description);
  }
  return condition;
}

void prepareCall(Core &core) {
  core.r[2] = 0u;
  core.r[3] = 0u;
  core.r[4] = kString;
  core.r[29] = kStack;
  core.r[31] = kReturn;
}

} // namespace

int main(int argc, char **argv) {
  if (argc != 3) {
    lucent::error("real-resident-test", "expected MAIN.EXE path and manifest digest");
    return 2;
  }
  auto bytes = tomba::test::readAuthenticatedImage(argv[1], argv[2], psx::cpu::kPsxExeMaxBytes);
  if (!bytes) {
    return 2;
  }

  tomba::test::StubRuntime runtime;
  psxport_install_game(runtime);
  auto game = std::make_unique<Game>();
  Core &core = game->core;
  auto loaded = psx::cpu::loadPsxExeImage(core, *bytes, "MAIN.EXE");
  if (!check(static_cast<bool>(loaded), "authenticated MAIN.EXE maps through the shipping loader")) {
    return 1;
  }
  auto image = *loaded.identity;
  psx::cpu::NativeKey key{image, kLength};
  if (!check(loaded.image.physicalText.containsPhysical(kLength) && core.currentImageIdentity(kLength) == image,
             "strlen entry belongs to the loaded resident image")) {
    return 1;
  }
  tomba::Str::registerOverrides();
  tomba::native::bindResident(core, image, loaded.image.physicalText);
  if (!check(core.nativeDispatcher().isInstalled(key), "shipping Str::length owner binds to MAIN.EXE")) {
    return 1;
  }
  for (std::uint32_t index = 0u; index < 4u; ++index) {
    core.mem_w8(kString + index, static_cast<std::uint8_t>('T' + index));
  }
  core.mem_w8(kString + 4u, 0u);

  prepareCall(core);
  auto blocksBeforeNative = core.lightrecExecutor().counters().executedBlocks;
  auto native = psx::cpu::dispatchGuest(core, kLength, kBudget);
  if (!check(native.returned() && native.detail == "ov_strLength" && core.pc == kReturn && core.r[2] == 4u &&
                 core.r[3] == 4u && core.r[4] == kString + 4u && core.r[29] == kStack && core.r[31] == kReturn &&
                 core.lightrecExecutor().counters().executedBlocks == blocksBeforeNative,
             "normal dispatch executes the shipping native owner and returns through the guest ABI")) {
    return 1;
  }
  std::array<std::uint32_t, 32> nativeRegisters{};
  for (std::size_t index = 0u; index < nativeRegisters.size(); ++index) {
    nativeRegisters[index] = core.r[index];
  }

  prepareCall(core);
  auto blocksBeforeOriginal = core.lightrecExecutor().counters().executedBlocks;
  auto original = psx::cpu::callOriginal(core, key, kBudget);
  if (!check(original.returned() && core.pc == kReturn && core.r[2] == 4u && core.r[3] == 4u &&
                 core.r[4] == kString + 4u && core.r[29] == kStack && core.r[31] == kReturn &&
                 core.lightrecExecutor().counters().executedBlocks > blocksBeforeOriginal,
             "scoped original executes authentic strlen bytes with a Lightrec return and matching ABI")) {
    return 1;
  }
  for (std::size_t index = 0u; index < nativeRegisters.size(); ++index) {
    if (!check(nativeRegisters[index] == core.r[index], "all guest GPRs match native versus original")) {
      lucent::error("real-resident-test",
                    "GPR {} native=0x{:08X} original=0x{:08X}",
                    index,
                    nativeRegisters[index],
                    core.r[index]);
      return 1;
    }
  }

  core.r[2] = 0u;
  core.r[3] = 9u;
  core.r[29] = kStack;
  core.r[31] = kReturn;
  auto wrongAddress = psx::cpu::dispatchGuest(core, kLength + 0x24u, kBudget);
  if (!check(wrongAddress.returned() && wrongAddress.detail != "ov_strLength" && core.pc == kReturn &&
                 core.r[2] == 9u && core.r[29] == kStack && core.nativeDispatcher().isInstalled(key) &&
                 !core.nativeDispatcher().isInstalled({image, kLength + 0x24u}),
             "neighboring resident address takes the guest path instead of the native owner")) {
    return 1;
  }

  if (!check(core.nativeDispatcher().remove(key), "disable the exact resident override")) {
    return 1;
  }
  prepareCall(core);
  auto blocksBeforeDisabled = core.lightrecExecutor().counters().executedBlocks;
  auto disabled = psx::cpu::dispatchGuest(core, kLength, kBudget);
  if (!check(disabled.returned() && disabled.detail != "ov_strLength" && core.pc == kReturn && core.r[2] == 4u &&
                 core.r[3] == 4u && core.r[29] == kStack &&
                 core.lightrecExecutor().counters().executedBlocks > blocksBeforeDisabled,
             "disabled override takes the original guest path and returns normally")) {
    return 1;
  }

  auto counters = core.lightrecExecutor().counters();
  lucent::info("real-resident-test",
               "PASS: native=1 original=1 wrong-address guest=1 disabled guest=1 "
               "executed_blocks={} executed_instructions={} fallback_blocks={}",
               counters.executedBlocks,
               counters.executedInstructions,
               counters.fallback.calls);
  return counters.executedBlocks > 0u && counters.fallback.calls == 0u ? 0 : 1;
}
