#include "cd/engine_cd.h"
#include "core/entry/runtime.h"
#include "core/sched/scheduler.h"
#include "core/sched/task_slot_layout.h"
#include "execution_control.h"
#include "game.h"
#include "game_runtime.h"
#include "guest_call.h"
#include "lightrec_executor.h"
#include "stock_libgpu.h"

#include <lucent/log.h>
#include <memory>

namespace {

constexpr std::uint32_t kNativeCaller = 0x80010000u;
constexpr std::uint32_t kReturn = 0x80010100u;
constexpr GuestAddressRange kSyntheticText{0x10000u, 0x10200u};

struct NestedObservation {
  psx::cpu::ExecutionResult result;
  std::uint32_t callerPc = 0;
  std::uint32_t activeAddress = 0;
  int calls = 0;
};

// Binds Tomba's libgpu; MAIN.EXE itself is not loaded.
class Runtime final : public psx::game::GameRuntime {
public:
  void registerOverrides(Game &game) override {
    libgpu_.bind(game);
  }
  RenderCapabilities renderCapabilities() const override {
    return RenderCapabilities::direct();
  }
  bool guestVramIsPicture(const Game &) const override {
    return false;
  }

  NestedObservation nested;

private:
  psx::hle::StockLibgpu libgpu_{tomba::kStockLibgpu};
};

void callTimeoutFromNative(Core *core) {
  auto &observed = static_cast<Runtime &>(core->runtime).nested;
  ++observed.calls;
  observed.result =
      psx::cpu::dispatchGuest0(*core, tomba::kStockLibgpu.timeoutArm, psx::cpu::ExecutionBudget::fromCycles(1));
  observed.callerPc = core->pc;
  observed.activeAddress = core->active_native_address;
}

class Checks {
public:
  void require(bool condition, const char *name) {
    ++checked_;
    if (!condition) {
      ++failed_;
      lucent::error("tomba-gpu-timeout-test", "failed: {}", name);
    }
  }

  int finish() const {
    lucent::info("tomba-gpu-timeout-test", "checked={} failed={}", checked_, failed_);
    return failed_ == 0 ? 0 : 1;
  }

private:
  int checked_ = 0;
  int failed_ = 0;
};

void checkGpuAfterTaskBudget(Game &game, Checks &checks) {
  Core &core = game.core;
  constexpr std::uint32_t entry = 0x80011000u;
  constexpr int slot = 1;
  const std::uint32_t base = tomba::sched::taskSlotBase(slot);
  core.mem_w32(entry, 0x1000ffffu); // synthetic busy-loop reaches a scheduler budget suspension
  core.mem_w32(entry + 4u, 0u);
  core.imageCatalog().activate("gpu-preceding-task", {0x11000u, 0x11008u}, 2u);
  core.mem_w16(base, 2u);
  core.mem_w32(base + 8u, 0x801ff000u);
  core.mem_w32(base + 12u, entry);
  const R3000 caller = core;
  tomba::EngineCd cd(core);
  tomba::sched::Scheduler scheduler(game, cd);
  scheduler.stepSlot(slot, caller);
  checks.require(core.lightrecExecutor().counters().executedBlocks > 0 &&
                     core.lightrecExecutor().counters().fallback.calls == 0,
                 "GPU precondition reaches the shipping task executor through JIT with zero fallback");
  core.r[31] = kReturn;
  const auto result =
      psx::cpu::dispatchGuest0(core, tomba::kStockLibgpu.timeoutArm, psx::cpu::ExecutionBudget::fromCycles(1));
  checks.require(result.returned(), "next GPU timeout arm does not consume a prior guest-task budget exit");
  if (!result.returned()) {
    lucent::error("tomba-gpu-timeout-test",
                  "next GPU exit={} PC=0x{:08X} cycles={}",
                  psx::cpu::executionExitName(result.reason),
                  result.guestPc,
                  result.cycles);
  }
}

} // namespace

int main() {
  Runtime runtime;
  auto game = std::make_unique<Game>(runtime, psx::config::Config{});
  Core &core = game->core;
  Checks checks;
  const auto &hle = tomba::kStockLibgpu;
  const auto budget = psx::cpu::ExecutionBudget::fromCycles(1);
  checks.require(game->platform_hle.lookup(hle.timeoutArm) == nullptr,
                 "timeout service is absent before the title binds its libgpu");
  runtime.registerOverrides(*game);
  runtime.registerOverrides(*game);
  checks.require(game->platform_hle.lookup(hle.timeoutArm) != nullptr,
                 "the title's libgpu binds the timeout entry idempotently");

  const auto initialCounters = core.lightrecExecutor().counters();
  core.mem_w32(hle.timeoutDeadlineWord, 7u);
  core.mem_w32(hle.timeoutFlagWord, 9u);
  core.r[31] = kReturn;
  core.r[29] = 0x801fff00u;
  core.pc = kNativeCaller;
  auto result = psx::cpu::dispatchGuest0(core, hle.timeoutArm, budget);
  checks.require(result.returned() && result.cycles == 0 && result.guestPc == kReturn,
                 "direct timeout arm returns without executing guest cycles");
  checks.require(core.pc == kReturn && core.r[31] == kReturn && core.r[29] == 0x801fff00u,
                 "direct timeout arm preserves the caller stack and return address");
  checks.require(core.mem_r32(hle.timeoutDeadlineWord) == 0x7fffffffu && core.mem_r32(hle.timeoutFlagWord) == 0,
                 "timeout arm reaches the existing synchronous GPU deadline/flag policy");

  const auto image = core.imageCatalog().activate("synthetic-native-caller", kSyntheticText, 1u);
  checks.require(core.nativeDispatcher().install({{image, kNativeCaller}, "timeout-caller", callTimeoutFromNative}),
                 "synthetic native caller installs through the shipping dispatcher");
  result = psx::cpu::dispatchGuest0(core, kNativeCaller, budget);
  checks.require(result.returned() && runtime.nested.calls == 1 && runtime.nested.result.returned(),
                 "nested native timeout arm completes both call boundaries");
  checks.require(runtime.nested.callerPc == kNativeCaller && runtime.nested.activeAddress == kNativeCaller &&
                     runtime.nested.result.guestPc == kReturn && core.active_native_address == 0,
                 "nested timeout arm restores the native caller context");

  core.imageCatalog().activate("synthetic-native-caller", kSyntheticText, 1u);
  result = psx::cpu::dispatchGuest0(core, hle.timeoutArm, budget);
  checks.require(result.returned(), "resident image replacement does not remove the platform service");
  result = psx::cpu::dispatchGuest0(core, hle.timeoutArm + 4u, budget);
  checks.require(result.reason == psx::cpu::ExecutionExitReason::Fault,
                 "adjacent unowned address cannot masquerade as the timeout service");
  const auto &counters = core.lightrecExecutor().counters();
  checks.require(counters.calls == initialCounters.calls && counters.executedBlocks == initialCounters.executedBlocks &&
                     counters.fallback.calls == initialCounters.fallback.calls,
                 "all reached timeout calls are host services, with no JIT or fallback execution");
  checkGpuAfterTaskBudget(*game, checks);
  return checks.finish();
}
