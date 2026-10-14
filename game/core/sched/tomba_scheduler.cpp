// tomba_scheduler.cpp — the per-frame slot pass, the native stage stanzas and the task primitives.
#include "sched/tomba_scheduler.h"

#include "cd/tomba_cd.h"
#include "core.h"
#include "coro.h"
#include "entry/tomba_session.h"
#include "game.h"
#include "guest_call.h"
#include "sched/task_slot_layout.h"

#include <lucent/log.h>

#include <cstdlib>

namespace tomba::sched {

namespace {

constexpr std::uint32_t kEnterCriticalSection = 0x80080890u;
constexpr std::uint32_t kExitCriticalSection = 0x800808A0u;
constexpr std::uint32_t kOpenThread = 0x80080860u;
constexpr std::uint32_t kCloseThread = 0x80080870u;
constexpr std::uint32_t kBiosThreadHandle = 0xFF000000u;
constexpr std::uint32_t kYieldResume = 0x80051FA4u; // FUN_80051F80's epilogue
constexpr std::uint32_t kFreshTaskReturn = 0xDEAD0000u;

void callGuest(Core &core, std::uint32_t address, const char *owner) {
  psx::cpu::dispatchGuestToReturn0(core, address, psx::cpu::ExecutionBudget::currentTurn(core), owner);
}

} // namespace

TombaScheduler::TombaScheduler(Game &game, TombaCd &cd) : game_(game), core_(game.core), cd_(cd) {}

TombaScheduler::~TombaScheduler() = default;

TombaScheduler &taskScheduler(Core &core) {
  return tombaSession(&core).scheduler;
}

bool TombaScheduler::isNativeEntry(std::uint32_t entry) {
  return entry == kStageDemoEntry || entry == kStageGameEntry || entry == kStageStartEntry ||
         entry == kSopAreaLoadEntry;
}

bool TombaScheduler::fresh(int slot, std::uint32_t state) const {
  return state == static_cast<std::uint32_t>(TaskState::Restart) ||
         (state == static_cast<std::uint32_t>(TaskState::Runnable) && !taskStarted_[slot]);
}

void TombaScheduler::startFresh(int slot, std::uint32_t base, const R3000 &loop) {
  taskCtx_[slot] = loop;
  taskCtx_[slot].r[29] = core_.mem_r32(base + kTaskStack);
  taskCtx_[slot].r[31] = kFreshTaskReturn;
}

void TombaScheduler::enterTask(const EnterTaskArgs &args) {
  auto [slot, base] = args;
  core_.mem_w16(base + kTaskState, static_cast<std::uint16_t>(TaskState::Running));
  core_.mem_w32(kCurrentTaskPointer, base);
  curSlot_ = slot;
  static_cast<R3000 &>(core_) = taskCtx_[slot];
}

void TombaScheduler::dropCoroutine(int slot) {
  coro_[slot].reset(); // ~Coro cancels a parked fiber
}

// FUN_80080880. FUN_80051F80 (yield), FUN_80051FB4 (end) and FUN_80052078 (stage swap) set the
// task state and then call it.
void TombaScheduler::yield(Core &core) {
  if (!inStage_) {
    core.r[2] = core.r[4];
    return;
  }
  lucent::debug("yieldpc",
                "switch yield ra=0x{:08X} r16=0x{:08X} r29=0x{:08X} slot2=0x{:X}",
                core.r[31],
                core.r[16],
                core.r[29],
                core.mem_r32(taskSlotBase(2)));
  const int slot = curSlot_;
  taskCtx_[slot] = static_cast<R3000 &>(core);
  if (curIsCoro_) {
    if (core.mem_r16(taskSlotBase(slot) + kTaskState) == static_cast<std::uint16_t>(TaskState::Ended)) {
      lucent::debug("sched", "switch exit slot {} ra=0x{:08X}", slot, core.r[31]);
      coro_[slot]->exit_now();
    }
    coro_[slot]->yield();
    return;
  }
  std::longjmp(yieldJmp_, 1);
}

// FUN_80051F80. Frame: sp-=24, ra at +16.
void TombaScheduler::yieldPrim(std::uint16_t mode) {
  Core &c = core_;
  c.r[29] -= 24;
  const std::uint32_t task = c.mem_r32(kCurrentTaskPointer);
  c.mem_w32(c.r[29] + 16, c.r[31]);
  c.mem_w16(task + kTaskCountdown, mode);
  c.mem_w16(task + kTaskState, static_cast<std::uint16_t>(TaskState::Yielded));
  c.r[2] = 1;
  c.r[3] = task;
  c.r[4] = kBiosThreadHandle;
  c.r[31] = kYieldResume;
  yield(c);
  c.r[31] = c.mem_r32(c.r[29] + 16);
  c.r[29] += 24;
}

// FUN_80051F14. Frame: sp-=24, s0 at +16, ra at +20.
void TombaScheduler::spawnPrim(const SpawnPrimArgs &args) {
  auto [slot, entry] = args;
  Core &c = core_;
  c.r[29] -= 24;
  const std::uint32_t base = taskSlotBase(static_cast<int>(slot));
  c.mem_w32(c.r[29] + 16, c.r[16]);
  c.r[16] = base;
  c.mem_w32(c.r[29] + 20, c.r[31]);
  c.mem_w32(base + kTaskEntry, entry);
  c.r[31] = 0x80051F40u;
  c.mem_w32(base + kTaskGp, c.r[28]); // FUN_80080930 returns the caller's gp
  c.mem_w16(base + kTaskState, static_cast<std::uint16_t>(TaskState::Runnable));
  c.r[31] = 0x80051F54u;
  c.mem_w8(base + kTaskSwitchFlag, 0);
  callGuest(c, kEnterCriticalSection, "TombaScheduler::spawnPrim");
  c.r[4] = c.mem_r32(base + kTaskEntry);
  c.r[5] = c.mem_r32(base + kTaskStack);
  c.r[6] = c.mem_r32(base + kTaskGp);
  c.r[31] = 0x80051F68u;
  callGuest(c, kOpenThread, "TombaScheduler::spawnPrim");
  c.r[31] = 0x80051F70u;
  c.mem_w32(base + kTaskHandle, c.r[2]);
  callGuest(c, kExitCriticalSection, "TombaScheduler::spawnPrim");
  c.r[31] = c.mem_r32(c.r[29] + 20);
  c.r[16] = c.mem_r32(c.r[29] + 16);
  c.r[29] += 24;
}

// FUN_80052010. Frame: sp-=24, s0 at +16, ra at +20, spilled even on the closed early-out.
void TombaScheduler::forceClose(std::uint32_t slot) {
  Core &c = core_;
  c.r[29] -= 24;
  const std::uint32_t base = taskSlotBase(static_cast<int>(slot));
  c.mem_w32(c.r[29] + 16, c.r[16]);
  c.r[16] = base;
  c.mem_w32(c.r[29] + 20, c.r[31]);
  if (c.mem_r16(base + kTaskState) != static_cast<std::uint16_t>(TaskState::Ended)) {
    c.mem_w16(base + kTaskState, static_cast<std::uint16_t>(TaskState::Ended));
    c.mem_w8(base + kTaskRunFlag, 0);
    c.r[31] = 0x80052054u;
    c.mem_w8(base + kTaskSwitchFlag, 0);
    callGuest(c, kEnterCriticalSection, "TombaScheduler::forceClose");
    c.r[4] = c.mem_r32(base + kTaskHandle);
    c.r[31] = 0x80052060u;
    callGuest(c, kCloseThread, "TombaScheduler::forceClose");
    c.r[31] = 0x80052068u;
    callGuest(c, kExitCriticalSection, "TombaScheduler::forceClose");
  }
  c.r[31] = c.mem_r32(c.r[29] + 20);
  c.r[16] = c.mem_r32(c.r[29] + 16);
  c.r[29] += 24;
}

// FUN_80051FB4. Frame: sp-=24, s0 at +16, ra at +20; does not return on a task.
void TombaScheduler::selfClose() {
  Core &c = core_;
  c.r[29] -= 24;
  c.mem_w32(c.r[29] + 16, c.r[16]);
  c.r[16] = kScratchpadBase;
  const std::uint32_t task = c.mem_r32(kCurrentTaskPointer);
  c.mem_w32(c.r[29] + 20, c.r[31]);
  c.mem_w8(task + kTaskRunFlag, 0);
  c.mem_w16(task + kTaskState, static_cast<std::uint16_t>(TaskState::Ended));
  c.r[31] = 0x80051FDCu;
  c.mem_w8(task + kTaskSwitchFlag, 0);
  callGuest(c, kEnterCriticalSection, "TombaScheduler::selfClose");
  c.r[4] = c.mem_r32(task + kTaskHandle);
  c.r[31] = 0x80051FF0u;
  callGuest(c, kCloseThread, "TombaScheduler::selfClose");
  c.r[31] = 0x80051FF8u;
  callGuest(c, kExitCriticalSection, "TombaScheduler::selfClose");
  c.r[4] = kBiosThreadHandle;
  c.r[31] = 0x80052000u;
  yield(c);
  c.r[31] = c.mem_r32(c.r[29] + 20);
  c.r[16] = c.mem_r32(c.r[29] + 16);
  c.r[29] += 24;
}

void TombaScheduler::spawn(const SpawnArgs &args) {
  auto [slot, entry] = args;
  const std::uint32_t base = taskSlotBase(slot);
  core_.mem_w32(base + kTaskEntry, entry);
  core_.mem_w32(base + kTaskGp, core_.r[28]);
  core_.mem_w16(base + kTaskState, static_cast<std::uint16_t>(TaskState::Runnable));
  core_.mem_w8(base + kTaskSwitchFlag, 0);
  core_.mem_w32(base + kTaskHandle, kBiosThreadHandle);
}

// DEMO: fresh runs stageMain then frame(); resume runs frame(). An entry rewrite drops ownership.
TombaScheduler::Stanza
TombaScheduler::runDemoStanza(int slot, std::uint32_t base, std::uint32_t state, const R3000 &loop) {
  const bool demoFresh = fresh(slot, state) && core_.mem_r32(base + kTaskEntry) == kStageDemoEntry;
  if (!demoFresh &&
      !(demoNative_[slot] && state == static_cast<std::uint32_t>(TaskState::Runnable) && taskStarted_[slot])) {
    return Stanza::NotMine;
  }
  if (demoFresh) {
    startFresh(slot, base, loop);
    taskStarted_[slot] = true;
    demoNative_[slot] = true;
  }
  enterTask({.slot = slot, .base = base});
  inStage_ = true;
  if (setjmp(yieldJmp_) == 0) {
    Engine &engine = eng(&core_);
    if (demoFresh) {
      engine.demo.stageMain();
    }
    engine.demo.frame();
  } else if (!warnedDemoYield_) {
    warnedDemoYield_ = true;
    lucent::debug("demo", "caught a DEMO substate yield");
  }
  inStage_ = false;
  if (core_.mem_r32(base + kTaskEntry) != kStageDemoEntry) {
    demoNative_[slot] = false;
    taskStarted_[slot] = false;
    return Stanza::Handled;
  }
  taskCtx_[slot] = static_cast<R3000 &>(core_);
  core_.mem_w16(base + kTaskState, static_cast<std::uint16_t>(TaskState::Runnable));
  return Stanza::Handled;
}

// SOP area-load: fresh only; every leaf is a synchronous CD read, so it ends in one call.
TombaScheduler::Stanza
TombaScheduler::runSopAreaLoadStanza(int slot, std::uint32_t base, std::uint32_t state, const R3000 &loop) {
  if (!fresh(slot, state) || core_.mem_r32(base + kTaskEntry) != kSopAreaLoadEntry) {
    return Stanza::NotMine;
  }
  startFresh(slot, base, loop);
  enterTask({.slot = slot, .base = base});
  inStage_ = true;
  if (setjmp(yieldJmp_) == 0) {
    eng(&core_).sop.areaLoad();
  } else {
    lucent::debug("sched", "SOP area-load yielded; a leaf is not synchronous");
  }
  inStage_ = false;
  core_.mem_w16(base + kTaskState, static_cast<std::uint16_t>(TaskState::Ended));
  taskStarted_[slot] = false;
  return Stanza::Handled;
}

// GAME: fresh runs stagePrologue + frame(); resume runs frame(). frame() returning 0 hands the task to
// the guest loop top, which the dispatch stanza re-enters each frame.
TombaScheduler::Stanza
TombaScheduler::runGameStanza(int slot, std::uint32_t base, std::uint32_t state, const R3000 &loop) {
  const bool gameFresh = fresh(slot, state) && core_.mem_r32(base + kTaskEntry) == kStageGameEntry;
  if (!gameFresh &&
      !(gameNative_[slot] && state == static_cast<std::uint32_t>(TaskState::Runnable) && taskStarted_[slot])) {
    return Stanza::NotMine;
  }
  if (gameFresh) {
    startFresh(slot, base, loop);
    taskStarted_[slot] = true;
    gameNative_[slot] = true;
  }
  enterTask({.slot = slot, .base = base});
  inStage_ = true;
  int handled = 1;
  if (setjmp(yieldJmp_) == 0) {
    Engine &engine = eng(&core_);
    if (gameFresh) {
      engine.stagePrologue();
    }
    handled = engine.frame();
  } else if (!warnedGameYield_) {
    warnedGameYield_ = true;
    lucent::debug("sched", "caught a GAME substate yield");
  }
  inStage_ = false;
  if (core_.mem_r32(base + kTaskEntry) != kStageGameEntry) {
    gameNative_[slot] = false;
    taskStarted_[slot] = false;
    return Stanza::Handled;
  }
  if (!handled) {
    core_.r[16] = kScratchpadBase;
    core_.r[17] = kScratchpadBase;
    core_.r[18] = 1;
    core_.r[31] = kGameLoopTop;
    taskCtx_[slot] = static_cast<R3000 &>(core_);
    gameNative_[slot] = false;
    gameCoop_[slot] = true;
    core_.mem_w16(base + kTaskState, static_cast<std::uint16_t>(TaskState::Runnable));
    lucent::debug("sched", "GAME -> cooperative guest loop");
    return Stanza::Handled;
  }
  taskCtx_[slot] = static_cast<R3000 &>(core_);
  core_.mem_w16(base + kTaskState, static_cast<std::uint16_t>(TaskState::Runnable));
  return Stanza::Handled;
}

void TombaScheduler::stepSlot(int slot, const R3000 &loop) {
  const std::uint32_t base = taskSlotBase(slot);
  // Slot 2 is the XA voice task; while the native clip player owns it, its state mirrors the clip.
  if (slot == 2 && cd_.holdsVoiceSlot()) {
    if (cd_.clipStreaming()) {
      core_.mem_w16(base + kTaskState, static_cast<std::uint16_t>(TaskState::Runnable));
    } else {
      core_.mem_w16(base + kTaskState, static_cast<std::uint16_t>(TaskState::Ended));
      cd_.releaseVoiceSlot();
    }
    taskStarted_[2] = false;
    return;
  }
  const std::uint32_t state = core_.mem_r16(base + kTaskState);
  if (state == static_cast<std::uint32_t>(TaskState::Ended)) {
    taskStarted_[slot] = false;
    demoNative_[slot] = false;
    return;
  }
  if (runDemoStanza(slot, base, state, loop) == Stanza::Handled) {
    return;
  }
  if (runSopAreaLoadStanza(slot, base, state, loop) == Stanza::Handled) {
    return;
  }
  if (runGameStanza(slot, base, state, loop) == Stanza::Handled) {
    return;
  }
  if (runCoroutineStanza(slot, base, state, loop) == Stanza::Handled) {
    return;
  }
  runDispatchStanza(slot, base, state, loop);
}

void TombaScheduler::step() {
  const R3000 loop = core_; // registers only; RAM is shared
  for (int slot = 0; slot < kTaskSlotCount; ++slot) {
    stepSlot(slot, loop);
  }
  static_cast<R3000 &>(core_) = loop;
}

// FUN_800506D0: a leaf over the three slots; a sleeping slot's countdown reaching zero re-arms it.
void TombaScheduler::tickSleepCountdown() {
  for (int slot = 0; slot < kTaskSlotCount; ++slot) {
    const std::uint32_t base = taskSlotBase(slot);
    if (core_.mem_r16(base + kTaskState) != static_cast<std::uint16_t>(TaskState::Yielded)) {
      continue;
    }
    const auto countdown = static_cast<std::uint16_t>(core_.mem_r16(base + kTaskCountdown) - 1u);
    core_.mem_w16(base + kTaskCountdown, countdown);
    if (countdown == 0u) {
      core_.mem_w16(base + kTaskState, static_cast<std::uint16_t>(TaskState::Runnable));
    }
  }
}

} // namespace tomba::sched
