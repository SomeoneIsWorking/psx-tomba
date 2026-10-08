#pragma once

#include "execution_exit.h"
#include "r3000.h"

#include <array>
#include <cstddef>
#include <cstdint>

class Core;

namespace tomba1 {

// THE GUEST'S OWN TASK TABLE, in the addresses the title uses. These are facts about the executable,
// so they are named here — where the scheduler that reads and writes them lives — rather than spelled
// inside its bodies. `Tomba1Runtime` binds the three native leaves below, and the budget-resume
// regression test drives the same records, so all four owners read one set of addresses.
inline constexpr std::uint32_t kTaskTableBegin = 0x801FD800u;
inline constexpr std::uint32_t kTaskTableEnd = 0x801FD950u;
inline constexpr std::uint32_t kTaskStride = 0x70u;
inline constexpr std::size_t kTaskSlotCount = 3u;

// The three measured native leaves the guest calls to create, yield, and restart a task.
inline constexpr std::uint32_t kTaskStartEntry = 0x80017154u;
inline constexpr std::uint32_t kTaskYieldEntry = 0x800171D4u;
inline constexpr std::uint32_t kTaskRestartEntry = 0x800172C4u;

// The record's field offsets, and the two state words the scheduler owns. 2 is "started, runnable"
// and is what a fresh start and a budget resume both write; 4 is "running right now" and is only true
// during a dispatch; 1 is "yielded, waiting to be made runnable again"; 3 is "restart requested, the
// entry is in the record". `kTaskRecordStep` is the guest's OWN field, written only at a yield from
// the value the guest passed — the scheduler never reads it and never invents it.
inline constexpr std::uint32_t kTaskRecordState = 0u;
inline constexpr std::uint32_t kTaskRecordStep = 2u;
inline constexpr std::uint32_t kTaskRecordStack = 8u;
inline constexpr std::uint32_t kTaskRecordEntry = 12u;
inline constexpr std::uint16_t kTaskStateWaiting = 1u;
inline constexpr std::uint16_t kTaskStateRunnable = 2u;
inline constexpr std::uint16_t kTaskStateRestart = 3u;
inline constexpr std::uint16_t kTaskStateRunning = 4u;

constexpr std::uint32_t taskRecordAddress(std::size_t slot) {
  return kTaskTableBegin + static_cast<std::uint32_t>(slot) * kTaskStride;
}

// HOW MANY DISPLAY FIELDS ONE UNBROKEN GUEST CALL MAY SPAN. This is the bound that makes resuming a
// budget exit safe, and it is a bound rather than a tolerance because "resume until it finishes" is
// indistinguishable, from outside, from a guest spin loop — the one thing the old code caught by
// aborting.
//
// It is not a guess about how long the game may legitimately run. It is measured: Tomba! 1's boot
// runs one LZ77 text/glyph decompress (`0x8003EF50`, `GuestGlyphStreamDecode`) that emits 286,720
// bytes and reached only 45,200 of them inside a single field's 564,480-cycle budget, so that one
// call spans at least SEVEN fields (see docs/issues/0007). Thirty-two fields is roughly four times
// the longest measured call, which is the headroom a diagnostic wants and far below the ~2,000
// fields a real spin would need to be mistaken for progress. A spin therefore still fails, and it
// fails with the register file and the resume address rather than as a hang.
inline constexpr std::uint32_t kMaxBudgetResumesPerCall = 32u;

// The one dispatch a guest task slot makes: run guest code from `address` on the CURRENT display
// field's budget until the executor publishes a bounded exit. A free function rather than a runtime
// method, because the slot lifecycle and the boot prefix are two owners and only the first of them
// needs a dispatch.
psx::cpu::ExecutionResult dispatchGuestTaskUntilExit(Core &core, std::uint32_t address);

// The three-record cooperative task table: one saved R3000 context, stack top, and resume address per
// slot, advanced by the host once per display field and returned to the guest only at a cooperative
// yield.
//
// It owns the whole multi-field story. A guest call that does not fit in one field's budget exits
// `BudgetExhausted`, and the faithful response — what hardware would do, and what the framework's
// executor contract calls an ordinary bounded exit — is to leave the task exactly where the exit found
// it and continue from the same PC on the next field. Two things that must not become: a silent hang
// (so the resume count is bounded by `kMaxBudgetResumesPerCall`, and the bound is reported with the
// state that hit it), and a resumed task that re-enters from its ENTRY (so a resume carries the
// exit's PC and the saved context, never the recorded entry).
class GuestTaskSlots {
public:
  // The guest's own task create, behind `kTaskStartEntry`: arm slot `slot` to run from `entry` on the
  // next field. Public because the native leaf is a guard around it — "a guest task start reached
  // outside the native frame owner" is the only thing `startOverride` adds, and a test that owns a
  // Core and a task table has no guest to make that call for it.
  void createTask(Core &core, std::size_t slot, std::uint32_t entry);

  // One display field's worth of scheduled guest work: every record that is runnable, in slot order.
  // A slot that spends its field budget returns from here and is picked up again next field; a slot
  // that reaches its cooperative yield leaves its record waiting.
  void runScheduledTasks(Core &core);

  static void startOverride(Core *core);
  static void yieldOverride(Core *core);
  static void restartOverride(Core *core);

private:
  struct TaskSlot {
    R3000 context{};
    std::uint32_t entry = 0;
    std::uint32_t resumeAddress = 0;
    // How many display fields this slot has been resumed across WITHOUT reaching its cooperative
    // yield. Reset by every yield and by every fresh start, so it measures one unbroken guest call,
    // not the slot's lifetime. See `kMaxBudgetResumesPerCall` for what bounds it.
    std::uint32_t budgetResumes = 0;
    bool contextReady = false;
  };

  void runTaskSlot(Core &core, std::size_t slot, std::uint16_t state, const R3000 &loopContext);

  // Takes the EXITED task's saved context explicitly, so the report cannot be called with the frame
  // loop's own registers by mistake. `runTaskSlot` already copied them out of the Core before
  // restoring the loop context, and it owns the copy; handing the context over keeps this a
  // formatter and leaves the one place that has the state responsible for getting it right.
  static void
  reportGuestExit(Core &core, std::size_t slot, const psx::cpu::ExecutionResult &result, const R3000 &exited);

  void yieldTask(Core &core);
  void restartTask(Core &core);

  // The one budget exit path: records the resume, re-arms the slot for the next field, and either
  // returns (progress) or reports and stops (no progress). Kept beside `runTaskSlot` rather than
  // inline because the two decisions it makes — is this the same call, and has it gone on too long —
  // are the whole of the policy, and inlining them buries them in the dispatch sequence.
  bool resumeAcrossField(Core &core, std::size_t slot, const psx::cpu::ExecutionResult &result);

  std::array<TaskSlot, kTaskSlotCount> tasks_{};
  bool restartRequested_ = false;
  std::size_t activeSlot_ = tasks_.size();

  // The reachability guard the native leaves share, and the only thing the creation scope below
  // touches: those leaves exist to catch a task transition raised from somewhere the scheduler is
  // not driving, so the state they check is the scheduler's alone to set.
  friend class GuestTaskCreationScope;

  static GuestTaskSlots *active_;
};

// The boot prefix creates the first task records, so the slot owner has to be reachable while that
// guest code runs. This is that reachability as a scope, so it cannot outlive the prefix: the guard
// in `startOverride` exists to catch a task start from somewhere the scheduler is not driving, and a
// scope that could be left open would make that guard a suggestion.
class GuestTaskCreationScope {
public:
  explicit GuestTaskCreationScope(GuestTaskSlots &slots);
  ~GuestTaskCreationScope();

  GuestTaskCreationScope(const GuestTaskCreationScope &) = delete;
  GuestTaskCreationScope &operator=(const GuestTaskCreationScope &) = delete;

private:
  GuestTaskSlots *slots_;
};

} // namespace tomba1
