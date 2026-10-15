// synchronous_task_wait.cpp — FUN_80044BD4 as a synchronous wait: the spawned slot-1 task runs to
// completion before the caller continues.
#include "sched/scheduler.h"

#include "core.h"
#include "coro.h"
#include "entry/session.h"
#include "execution_control.h"
#include "game.h"
#include "native_dispatch.h"
#include "sched/task_slot_layout.h"

#include <lucent/log.h>

#include <cstdlib>

namespace tomba::sched {

namespace {

constexpr int kPumpLimit = 4096;

} // namespace

SyncWaitCompletion Scheduler::completeSyncWait(std::uint32_t taskBase, std::uint32_t flag) {
  SyncWaitCompletion result{};
  if (flag != 1) {
    result.stamp = static_cast<std::uint16_t>(rngOf(&core_).next()); // FUN_8009A450
    core_.mem_w16(taskBase + kSyncWaitStamp, result.stamp);
    result.stamped = true;
  }
  lucent::debug("sched", "sync wait complete task=0x{:08X} flag={} stamped={}", taskBase, flag, result.stamped);
  return result;
}

void Scheduler::runSyncSlot(int slot) {
  Core &core = core_;
  const std::uint32_t base = taskSlotBase(slot);
  const std::uint32_t entry = core.mem_r32(base + kTaskEntry);
  const bool isFresh = !taskStarted_[slot];

  const R3000 callerRegs = static_cast<R3000 &>(core);
  const std::uint32_t callerTask = core.mem_r32(kCurrentTaskPointer);
  const int callerSlot = curSlot_;
  const bool callerCoro = curIsCoro_;

  if (isFresh) {
    taskCtx_[slot] = callerRegs;
    taskCtx_[slot].r[29] = core.mem_r32(base + kTaskStack);
    taskCtx_[slot].r[31] = 0xDEAD0000u;
  }
  const std::uint32_t startPc = isFresh ? entry : taskCtx_[slot].r[31];
  dropCoroutine(slot);
  taskStarted_[slot] = true;
  core.mem_w16(base + kTaskState, static_cast<std::uint16_t>(TaskState::Running));
  core.mem_w32(kCurrentTaskPointer, base);
  curSlot_ = slot;
  curIsCoro_ = true;
  static_cast<R3000 &>(core) = taskCtx_[slot];
  lucent::debug("sched",
                "inline task slot {} {} pc=0x{:08X} sp=0x{:08X}",
                slot,
                isFresh ? "start" : "resume",
                startPc,
                core.r[29]);

  coro_[slot] = std::make_unique<Coro>();
  Coro &coroutine = *coro_[slot];
  coroutine.start([&core, startPc] {
    const auto result =
        psx::cpu::dispatchGuest(core, startPc, psx::cpu::ExecutionBudget::currentTurn(core), "sync_task_wait");
    (void)psx::cpu::completeOrPropagate(core, result);
  });
  int pumps = 0;
  while (true) {
    coroutine.resume();
    if (coroutine.done() || core.mem_r16(base + kTaskState) == static_cast<std::uint16_t>(TaskState::Ended)) {
      break;
    }
    if (++pumps >= kPumpLimit) {
      lucent::error(
          "sched", "FATAL: synchronous task 0x{:08X} (slot {}) still parked after {} resumes", entry, slot, pumps);
      std::abort();
    }
    core.mem_w16(base + kTaskCountdown, 0);
    core.mem_w16(base + kTaskState, static_cast<std::uint16_t>(TaskState::Runnable));
  }
  core.mem_w16(base + kTaskState, static_cast<std::uint16_t>(TaskState::Ended));

  curIsCoro_ = callerCoro;
  curSlot_ = callerSlot;
  dropCoroutine(slot);
  taskStarted_[slot] = false;
  core.mem_w32(kCurrentTaskPointer, callerTask);
  static_cast<R3000 &>(core) = callerRegs;
}

// Frame: sp-=40; the caller's live s0..s3 and ra spill at +16..+32, then fn/flag/p2/p3 live in
// s2/s3/s1/s0 so nested callee spills hold the guest's values.
void Scheduler::spawnAndWait(const SpawnAndWaitArgs &args) {
  auto [fn, p2, p3, flag] = args;
  Core &core = core_;
  core.r[29] -= 40;
  core.mem_w32(core.r[29] + 24, core.r[18]);
  core.r[18] = fn;
  core.mem_w32(core.r[29] + 28, core.r[19]);
  core.r[19] = flag;
  core.mem_w32(core.r[29] + 20, core.r[17]);
  core.r[17] = p2;
  core.mem_w32(core.r[29] + 16, core.r[16]);
  core.r[16] = p3;
  core.mem_w32(core.r[29] + 32, core.r[31]);

  while (core.mem_r16(taskSlotBase(1) + kTaskState) != static_cast<std::uint16_t>(TaskState::Ended)) {
    runSyncSlot(1);
  }

  core.r[4] = 2;
  core.r[31] = kSyncWaitForceCloseRa;
  forceClose(2);
  core.mem_w8(kSyncWaitParam2, static_cast<std::uint8_t>(core.r[17]));
  core.r[17] = kScratchpadBase;
  core.mem_w8(kSyncWaitParam3, static_cast<std::uint8_t>(core.r[16]));
  core.mem_w8(kSyncWaitDoneFlag, 0);
  core.r[4] = 1;
  core.r[5] = core.r[18];
  core.r[31] = kSyncWaitSpawnRa;
  spawnPrim({.slot = core.r[4], .entry = core.r[5]});
  core.r[31] = kSyncWaitFinishRa;
  completeSyncWait(core.mem_r32(kCurrentTaskPointer), core.r[19]);

  if (core.mem_r8(kSyncWaitDoneFlag) == 0) {
    runSyncSlot(1);
  }
  if (core.mem_r8(kSyncWaitDoneFlag) == 0) {
    lucent::error("sched", "FATAL: synchronous task 0x{:08X} ended without raising the done flag", fn);
    std::abort();
  }

  core.r[31] = core.mem_r32(core.r[29] + 32);
  core.r[19] = core.mem_r32(core.r[29] + 28);
  core.r[18] = core.mem_r32(core.r[29] + 24);
  core.r[17] = core.mem_r32(core.r[29] + 20);
  core.r[16] = core.mem_r32(core.r[29] + 16);
  core.r[29] += 40;
}

} // namespace tomba::sched
