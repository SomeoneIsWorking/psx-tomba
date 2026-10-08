// NEGATIVE COVERAGE for the issue-0015 abort: `bindResident` must REFUSE, by aborting the process,
// when a declaration uses the resident form at an address outside the resident text range.
//
// This test covers the decision itself, which only the product's own binder can make: a resident-form
// declaration at an address no MODE image covers, including AREA-slot addresses. Without it the abort
// is untested, and an untested fatal path is one nobody dares change.
//
// The refusal is `std::abort()` in the product, and it stays that way: an unreachable declaration
// means the process is running a catalog it does not understand, which is not a recoverable state.
// ctest does not invert WILL_FAIL for a child killed by a signal, so the death is observed the way a
// process can be observed — this test FORKS, and the parent asserts the child was killed by SIGABRT.
// Testing the signal rather than a non-zero exit is also the stronger claim: it distinguishes the
// refusal from bindResident quietly returning.
#include "core/overrides/native_override_catalog.h"
#include "game.h"
#include "game_runtime.h"
#include "lightrec_executor.h"

#include <lucent/log.h>
#include <sys/wait.h>
#include <unistd.h>

#include <memory>

namespace {

class Runtime final : public GameRuntime {
public:
  void *createContext(Core &) override {
    return nullptr;
  }
  void destroyContext(void *) override {}
  void registerOverrides(Game &) override {}
  void bootInit(Core &) override {}
  RenderCapabilities renderCapabilities() const override {
    return RenderCapabilities::direct();
  }
  bool guestVramIsPicture(const Game &) const override {
    return false;
  }
};

// Inside the resident text range, so it must install and must NOT trip the refusal.
constexpr std::uint32_t kInsideEntry = 0x80010000u;
// An AREA-slot address: no MODE image covers it, and `declareOverlayOverride` is the only form that
// can reach one. Declared with the resident form this is the exact defect issue 0015 is about.
constexpr std::uint32_t kUnreachableEntry = 0x8018BD30u;
constexpr GuestAddressRange kResidentText{0x10000u, 0x10110u};

void insideOwner(Core *core) {
  core->r[2] = 1u;
}

void unreachableOwner(Core *core) {
  core->r[2] = 2u;
}

int failed = 0;

void check(bool condition, const char *name) {
  if (condition) {
    return;
  }
  ++failed;
  lucent::error("native-catalog-abort-test", "failed: {}", name);
}

} // namespace

int main() {
  Runtime runtime;
  psxport_install_game(runtime);
  auto game = std::make_unique<Game>();
  Core &core = game->core;
  core.mem_w32(kInsideEntry, 0x24020005u);      // addiu v0, zero, 5
  core.mem_w32(kInsideEntry + 4u, 0x03e00008u); // jr ra
  core.mem_w32(kInsideEntry + 8u, 0u);
  const auto resident = core.imageCatalog().activate("resident", kResidentText, 1u);

  // Declared in the child, because the catalog is process-global: the parent must keep a catalog it
  // can still reason about, and a second `bindResident` here would abort it for the wrong reason.
  const pid_t child = fork();
  if (child == 0) {
    tomba::native::declareOverride(kInsideEntry, "inside-owner", insideOwner);
    tomba::native::declareOverride(kUnreachableEntry, "unreachable-owner", unreachableOwner);
    tomba::native::bindResident(core, resident, kResidentText);
    _exit(0); // only reached if the refusal did NOT fire
  }
  check(child > 0, "fork produced a child");
  if (child <= 0) {
    return 1;
  }

  int status = 0;
  check(waitpid(child, &status, 0) == child, "reaped the child");
  check(WIFSIGNALED(status), "bindResident ABORTED on an unreachable declaration");
  check(WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT, "the abort was SIGABRT");
  check(WIFSIGNALED(status) || WEXITSTATUS(status) != 0, "the child did not report success");

  if (failed != 0) {
    lucent::error("native-catalog-abort-test", "{} check(s) failed", failed);
    return 1;
  }
  lucent::info("native-catalog-abort-test", "PASS: bindResident refuses an unreachable declaration");
  return 0;
}
