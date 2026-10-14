// tomba_scheduler.h — Tomba! 2's cooperative task scheduler (FUN_80051E60) and its primitives.
#pragma once

#include "core.h"

#include <array>
#include <csetjmp>
#include <cstdint>
#include <memory>

class Coro;
class Game;

namespace tomba {
class TombaCd;
} // namespace tomba

namespace tomba::sched {

struct SyncWaitCompletion {
  bool stamped = false;
  std::uint16_t stamp = 0;
};

// One pass per frame over the three task slots. DEMO, GAME, START and SOP area-load run as native
// stage bodies; every other task runs its guest body on a Coro fiber. A task context is the R3000
// register file only: guest RAM is shared by every task.
class TombaScheduler {
public:
  // `cd` holds slot 2 while a native XA clip plays.
  TombaScheduler(Game &game, TombaCd &cd);
  ~TombaScheduler();
  TombaScheduler(const TombaScheduler &) = delete;
  TombaScheduler &operator=(const TombaScheduler &) = delete;

  void step();
  // FUN_80080880 ChangeThread: parks the running task; outside a task it returns the handle in v0.
  void yield(Core &core);
  // FUN_800506D0: a sleeping task's countdown re-arms it.
  void tickSleepCountdown();

  // One slot's turn of step(); `loop` holds the frame-loop registers a fresh task inherits.
  void stepSlot(int slot, const R3000 &loop);

  void yieldPrim(std::uint16_t mode); // FUN_80051F80
  struct SpawnPrimArgs {
    std::uint32_t slot;
    std::uint32_t entry;
  };
  void spawnPrim(const SpawnPrimArgs &args); // FUN_80051F14
  void forceClose(std::uint32_t slot);       // FUN_80052010
  void selfClose();                          // FUN_80051FB4
  struct SpawnArgs {
    int slot;
    std::uint32_t entry;
  };
  // FUN_80051F14's slot writes without its frame or BIOS calls.
  void spawn(const SpawnArgs &args);
  struct SpawnAndWaitArgs {
    std::uint32_t fn;
    std::uint32_t p2;
    std::uint32_t p3;
    std::uint32_t flag;
  };
  // FUN_80044BD4: spawns slot 1 and runs it to completion before returning.
  void spawnAndWait(const SpawnAndWaitArgs &args);
  // FUN_80044BD4's tail for a wait whose task already ran: flag != 1 stamps the task with the RNG.
  SyncWaitCompletion completeSyncWait(std::uint32_t taskBase, std::uint32_t flag);

  bool hasLiveCoroutine(int slot) const {
    return coro_[slot] != nullptr;
  }
  bool taskStarted(int slot) const {
    return taskStarted_[slot];
  }
  const R3000 &savedContext(int slot) const {
    return taskCtx_[slot];
  }

private:
  enum class Stanza : std::uint8_t { NotMine, Handled };

  static bool isNativeEntry(std::uint32_t entry);
  bool fresh(int slot, std::uint32_t state) const;
  struct EnterTaskArgs {
    int slot;
    std::uint32_t base;
  };
  void enterTask(const EnterTaskArgs &args);
  void startFresh(int slot, std::uint32_t base, const R3000 &loop);
  void dropCoroutine(int slot);
  Stanza runDemoStanza(int slot, std::uint32_t base, std::uint32_t state, const R3000 &loop);
  Stanza runSopAreaLoadStanza(int slot, std::uint32_t base, std::uint32_t state, const R3000 &loop);
  Stanza runGameStanza(int slot, std::uint32_t base, std::uint32_t state, const R3000 &loop);
  Stanza runCoroutineStanza(int slot, std::uint32_t base, std::uint32_t state, const R3000 &loop);
  void runDispatchStanza(int slot, std::uint32_t base, std::uint32_t state, const R3000 &loop);
  struct RunGuestTaskArgs {
    int slot;
    std::uint32_t base;
    std::uint32_t entry;
  };
  void runGuestTask(const RunGuestTaskArgs &args);
  void runSyncSlot(int slot);

  Game &game_;
  Core &core_;
  TombaCd &cd_;
  std::jmp_buf yieldJmp_{};
  std::array<R3000, 3> taskCtx_{};
  std::array<std::unique_ptr<Coro>, 3> coro_;
  std::array<bool, 3> taskStarted_{};
  std::array<bool, 3> demoNative_{};
  std::array<bool, 3> gameNative_{};
  // GAME re-enters its guest loop top each frame while a state is not native.
  std::array<bool, 3> gameCoop_{};
  bool inStage_ = false;
  bool curIsCoro_ = false;
  int curSlot_ = 0;
  bool warnedDemoYield_ = false;
  bool warnedGameYield_ = false;
};

// The scheduler of the Game `core` belongs to.
TombaScheduler &taskScheduler(Core &core);

} // namespace tomba::sched
