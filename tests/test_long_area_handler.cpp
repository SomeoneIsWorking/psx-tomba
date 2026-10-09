// The area handlers' first state resets the object pools (resident FUN_8007B18C clears 0x208 records byte by
// byte), which needs more than one display field. The shipping handler dispatch resumes it across turns; a
// single-turn dispatch of the same guest function exhausts its budget.
#include "authenticated_image.h"
#include "core.h"
#include "core/overrides/guest_jal.h"
#include "game.h"
#include "psx_exe_image.h"
#include "stub_runtime.h"

#include <lucent/log.h>
#include <memory>

namespace {

inline constexpr std::uint32_t kPoolReset = 0x8007b18cu;
inline constexpr std::uint32_t kPoolFreeCount = 0x800ed098u; // 0x208 once the record pool is rebuilt
inline constexpr std::uint32_t kPoolRecords = 0x208u;
inline constexpr std::uint32_t kReturn = 0x80010100u;
inline constexpr std::uint32_t kStack = 0x801e0000u;

bool check(bool condition, const char *description) {
  if (!condition) {
    lucent::error("long-handler-test", "failed: {}", description);
  }
  return condition;
}

void prepareCall(Core &core) {
  core.r[29] = kStack;
  core.r[31] = kReturn;
}

} // namespace

int main(int argc, char **argv) {
  if (argc != 3) {
    lucent::error("long-handler-test", "expected MAIN.EXE path and sha256 digest");
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
  if (!check(static_cast<bool>(psx::cpu::loadPsxExeImage(core, *bytes, "MAIN.EXE")),
             "authenticated MAIN.EXE maps through the shipping loader")) {
    return 1;
  }

  prepareCall(core);
  const auto oneTurn = psx::cpu::dispatchGuest(core, kPoolReset, psx::cpu::ExecutionBudget::currentTurn(core));
  if (!check(!oneTurn.returned() && oneTurn.reason == psx::cpu::ExecutionExitReason::BudgetExhausted,
             "the pool reset does not fit one display field")) {
    return 1;
  }

  core.mem_w32(kPoolFreeCount, 0u);
  prepareCall(core);
  tomba::guest::dispatchHandlerToReturnResuming(core, kPoolReset, "test");
  if (!check(core.mem_r32(kPoolFreeCount) == kPoolRecords && core.pc == kReturn && core.r[29] == kStack,
             "the shipping handler dispatch resumes it across turns and returns to the caller")) {
    return 1;
  }
  lucent::info("long-handler-test", "PASS: one-turn dispatch exhausts the budget; resuming dispatch completes");
  return 0;
}
