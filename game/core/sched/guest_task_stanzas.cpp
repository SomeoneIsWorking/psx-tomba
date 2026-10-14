// guest_task_stanzas.cpp — tasks with no native stage body: guest bodies on a Coro fiber, and the
// dispatch stanza for the native stage entries' guest continuations.
#include "sched/tomba_scheduler.h"

#include "core.h"
#include "coro.h"
#include "entry/tomba_session.h"
#include "execution_control.h"
#include "game.h"
#include "lightrec_executor.h"
#include "native_dispatch.h"
#include "sched/task_slot_layout.h"

#include <lucent/log.h>

#include <cstdlib>

namespace tomba::sched {

namespace {

// One JIT turn of a guest task; true when its budget ran out with `resumePc` the next instruction.
bool executeGuestTaskTurn(Core &core, std::uint32_t &resumePc, std::uint32_t returnAddress) {
  const auto result =
      core.lightrecExecutor().executeFunction(resumePc, returnAddress, psx::cpu::ExecutionBudget::currentTurn(core));
  if (result.returned()) {
    return false;
  }
  if (result.reason == psx::cpu::ExecutionExitReason::BudgetExhausted) {
    resumePc = result.guestPc;
    return true;
  }
  // No host-loop exit channel here: an unsupported exit stops at its owner.
  if (!psx::cpu::requireGuestReturn(result, "guest coroutine task")) {
    std::abort();
  }
  return false;
}

} // namespace

// Runs on the slot's fiber. Only a guest return or an authored cancellation ends the task.
void TombaScheduler::runGuestTask(const RunGuestTaskArgs &args) {
  auto [slot, base, entry] = args;
  const std::uint32_t returnAddress = core_.r[31];
  std::uint32_t resumePc = entry;
  while (executeGuestTaskTurn(core_, resumePc, returnAddress)) {
    const std::uint16_t state = core_.mem_r16(base + kTaskState);
    if (state == static_cast<std::uint16_t>(TaskState::Ended)) {
      return;
    }
    taskCtx_[slot] = static_cast<R3000 &>(core_);
    taskCtx_[slot].pc = resumePc;
    if (state == static_cast<std::uint16_t>(TaskState::Running)) {
      core_.mem_w16(base + kTaskState, static_cast<std::uint16_t>(TaskState::Runnable));
    }
    // The executor's guards are gone before this point; Coro::yield may longjmp to the fiber root.
    coro_[slot]->yield();
  }
}

TombaScheduler::Stanza
TombaScheduler::runCoroutineStanza(int slot, std::uint32_t base, std::uint32_t state, const R3000 &loop) {
  if (isNativeEntry(core_.mem_r32(base + kTaskEntry))) {
    return Stanza::NotMine;
  }
  const bool coFresh = fresh(slot, state);
  if (coFresh) {
    dropCoroutine(slot);
    const std::uint32_t entry = core_.mem_r32(base + kTaskEntry);
    startFresh(slot, base, loop);
    taskStarted_[slot] = true;
    demoNative_[slot] = false;
    gameNative_[slot] = false;
    gameCoop_[slot] = false;
    coro_[slot] = std::make_unique<Coro>();
    coro_[slot]->start([this, slot, base, entry] {
      runGuestTask({.slot = slot, .base = base, .entry = entry});
    });
  } else if (state != static_cast<std::uint32_t>(TaskState::Runnable)) {
    return Stanza::Handled; // sleeping this frame
  } else if (!coro_[slot] || coro_[slot]->done()) {
    taskStarted_[slot] = false;
    return Stanza::Handled;
  }
  enterTask({.slot = slot, .base = base});
  inStage_ = true;
  curIsCoro_ = true;
  lucent::debug("sched",
                "slot {} coro {} st={} entry=0x{:08X} sp=0x{:08X}",
                slot,
                coFresh ? "start" : "resume",
                state,
                core_.mem_r32(base + kTaskEntry),
                taskCtx_[slot].r[29]);
  coro_[slot]->resume();
  curIsCoro_ = false;
  inStage_ = false;
  if (coro_[slot]->done() || core_.mem_r16(base + kTaskState) == static_cast<std::uint16_t>(TaskState::Ended)) {
    core_.mem_w16(base + kTaskState, static_cast<std::uint16_t>(TaskState::Ended));
    taskStarted_[slot] = false;
    dropCoroutine(slot);
  }
  return Stanza::Handled;
}

// A native stage entry the stage stanzas did not take. A fresh GAME runs stageMain and continues in
// the guest at its redirect; a fresh START runs startBinStage and ends the tick. GAME's cooperative
// fallback re-enters the loop top with its callee-saved registers and sp at the 0x20 prologue frame.
void TombaScheduler::runDispatchStanza(int slot, std::uint32_t base, std::uint32_t state, const R3000 &loop) {
  std::uint32_t resumePc = 0;
  bool isFresh = false;
  if (fresh(slot, state)) {
    resumePc = core_.mem_r32(base + kTaskEntry);
    isFresh = true;
    startFresh(slot, base, loop);
    taskStarted_[slot] = true;
    demoNative_[slot] = false;
  } else if (state == static_cast<std::uint32_t>(TaskState::Runnable) && gameCoop_[slot] &&
             core_.mem_r32(base + kTaskEntry) == kStageGameEntry) {
    resumePc = kGameLoopTop;
    taskCtx_[slot].r[16] = kScratchpadBase;
    taskCtx_[slot].r[17] = kScratchpadBase;
    taskCtx_[slot].r[18] = 1;
    taskCtx_[slot].r[31] = kGameLoopTop;
    taskCtx_[slot].r[29] = core_.mem_r32(base + kTaskStack) - 0x20u;
  } else if (state == static_cast<std::uint32_t>(TaskState::Runnable)) {
    gameCoop_[slot] = false;
    resumePc = taskCtx_[slot].r[31];
  } else {
    return; // sleeping this frame
  }
  lucent::debug("sched",
                "slot {} st_in={} resume_pc=0x{:08X} ra=0x{:08X} sp=0x{:08X}",
                slot,
                state,
                resumePc,
                taskCtx_[slot].r[31],
                taskCtx_[slot].r[29]);
  enterTask({.slot = slot, .base = base});
  inStage_ = true;
  if (setjmp(yieldJmp_) == 0) {
    std::uint32_t start = resumePc;
    if (isFresh) {
      core_.pending_guest_redirect = 0;
      if (resumePc == kStageGameEntry) {
        eng(&core_).stageMain(); // leaves the guest redirect at the loop top
      } else if (resumePc == kStageStartEntry) {
        eng(&core_).startBinStage();
        taskCtx_[slot] = static_cast<R3000 &>(core_);
        core_.mem_w16(base + kTaskState, static_cast<std::uint16_t>(TaskState::Runnable));
        inStage_ = false;
        return;
      }
      if (core_.pending_guest_redirect) {
        start = core_.pending_guest_redirect;
        core_.pending_guest_redirect = 0;
      }
    }
    const auto result = psx::cpu::dispatchGuest(
        core_, start, psx::cpu::ExecutionBudget::currentTurn(core_), "TombaScheduler::runDispatchStanza");
    if (result.returned()) {
      core_.mem_w16(base + kTaskState, static_cast<std::uint16_t>(TaskState::Ended));
      taskStarted_[slot] = false;
    } else {
      psx::cpu::requestExecutionExit(core_, result);
    }
  }
  inStage_ = false;
}

} // namespace tomba::sched
