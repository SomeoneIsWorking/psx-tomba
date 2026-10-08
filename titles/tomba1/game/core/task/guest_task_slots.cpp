#include "guest_task_slots.h"

#include "core.h"
#include "execution_control.h"
#include "execution_exit.h"
#include "guest_call.h"

#include <cstdlib>
#include <lucent/log.h>
#include <string>

namespace tomba1 {
namespace {

constexpr std::uint32_t kCurrentTask = 0x1F8001D4u;

bool validTaskRecord(std::uint32_t record) {
  return record >= kTaskTableBegin && record < kTaskTableEnd && (record - kTaskTableBegin) % kTaskStride == 0u;
}

// One register as eight fixed-width hex digits. A variable-width word is a log line that a reader
// has to re-align by eye to compare two of them, which is the whole job here.
std::string hexWord(std::uint32_t value) {
  constexpr const char *const kDigits = "0123456789ABCDEF";
  std::string text(8, '0');
  for (int shift = 28; shift >= 0; shift -= 4) {
    text[static_cast<std::size_t>((28 - shift) / 4)] = kDigits[(value >> shift) & 0xFu];
  }
  return text;
}

constexpr const char *const kAbiNames[32] = {
    "zero", "at", "v0", "v1", "a0", "a1", "a2", "a3", //
    "t0",   "t1", "t2", "t3", "t4", "t5", "t6", "t7", //
    "s0",   "s1", "s2", "s3", "s4", "s5", "s6", "s7", //
    "t8",   "t9", "k0", "k1", "gp", "sp", "fp", "ra",
};

} // namespace

// The guest task exit report, and the one place a guest exit is described. It is a function rather
// than inline formatting because the register file is printed as a header line and a body line: one
// line per register is what a person needs, and a second call site is what would otherwise copy this
// list. Names are the PSX ABI's, so the reader does not have to know that $16 is s0.
void GuestTaskSlots::reportGuestExit(Core &core,
                                     std::size_t slot,
                                     const psx::cpu::ExecutionResult &result,
                                     const R3000 &exited) {
  lucent::error("tomba1-frame",
                "guest task {} exited with {} at 0x{:08X} after {} of {} budget cycles: {}",
                slot,
                psx::cpu::executionExitName(result.reason),
                result.guestPc,
                result.cycles,
                psx::cpu::ExecutionBudget::currentTurn(core).cycles,
                result.detail);
  // Three registers per line: wide enough to stay legible in a log, narrow enough that a reader
  // scanning for one register does not lose the next row's.
  //
  // The R3000 number here is the GPR INDEX, and the PSX ABI number is index-1, so the two agree for
  // every register except $zero. The name column uses the ABI numbering, which is what a MIPS
  // disassembly of the same state shows, and this report is read next to such a disassembly.
  for (std::size_t start = 0; start < 32; start += 3) {
    const std::size_t end = start + 3 < 32 ? start + 3 : 32;
    std::string line = "  guest-regs";
    for (std::size_t i = start; i < end; ++i) {
      line += std::string(" ") + kAbiNames[i] + "=0x" + hexWord(exited.r[i]);
    }
    lucent::error("tomba1-frame", "{}", line);
  }
}

GuestTaskSlots *GuestTaskSlots::active_ = nullptr;

GuestTaskCreationScope::GuestTaskCreationScope(GuestTaskSlots &slots) : slots_(&slots) {
  slots_->active_ = slots_;
}

GuestTaskCreationScope::~GuestTaskCreationScope() {
  if (slots_ != nullptr) {
    slots_->active_ = nullptr;
  }
}

psx::cpu::ExecutionResult dispatchGuestTaskUntilExit(Core &core, std::uint32_t address) {
  return psx::cpu::dispatchGuestUntilExit(core, address, psx::cpu::ExecutionBudget::currentTurn(core));
}

void GuestTaskSlots::createTask(Core &core, std::size_t slot, std::uint32_t entry) {
  if (slot >= tasks_.size() || entry == 0u) {
    lucent::error("tomba1-frame", "invalid task start: slot={} entry=0x{:08X}", slot, entry);
    std::abort();
  }
  if (slot == activeSlot_) {
    lucent::error("tomba1-frame", "guest tried to replace its own live task slot {}", slot);
    std::abort();
  }
  const std::uint32_t record = taskRecordAddress(slot);
  TaskSlot &task = tasks_[slot];
  task.entry = entry;
  task.resumeAddress = entry;
  task.contextReady = false;
  // A fresh start begins a NEW call, so it begins with no resumes. Nothing can observe this reset:
  // every path that leaves a slot's counter non-zero ends the process, so a fresh start is always
  // reached with the counter already zero. It is stated here because the counter's SCOPE is this
  // decision, not because a transition depends on it.
  task.budgetResumes = 0;
  core.mem_w16(record + kTaskRecordState, kTaskStateRunnable);
}

void GuestTaskSlots::runTaskSlot(Core &core, std::size_t slot, std::uint16_t state, const R3000 &loopContext) {
  const std::uint32_t record = taskRecordAddress(slot);
  TaskSlot &task = tasks_[slot];
  for (int restartBudget = 0; restartBudget < 4; ++restartBudget) {
    if (state == kTaskStateRestart || !task.contextReady) {
      if (state == kTaskStateRestart) {
        task.entry = core.mem_r32(record + kTaskRecordEntry);
      }
      const std::uint32_t entry = task.entry;
      if (entry == 0u) {
        lucent::error("tomba1-frame", "runnable task 0x{:08X} was not started by the native task owner", record);
        std::abort();
      }
      task.context = loopContext;
      task.context.r[29] = core.mem_r32(record + kTaskRecordStack);
      task.context.r[31] = 0xDEAD0000u;
      task.context.pc = entry;
      task.resumeAddress = entry;
      task.contextReady = true;
    }
    if (!task.contextReady) {
      lucent::error("tomba1-frame", "task {} has no saved CPU register context", slot);
      std::abort();
    }

    core.mem_w32(kCurrentTask, record);
    core.mem_w16(record + kTaskRecordState, kTaskStateRunning);
    restartRequested_ = false;
    activeSlot_ = slot;
    active_ = this;
    static_cast<R3000 &>(core) = task.context;
    const psx::cpu::ExecutionResult result = dispatchGuestTaskUntilExit(core, task.resumeAddress);
    task.context = static_cast<R3000 &>(core);
    static_cast<R3000 &>(core) = loopContext;
    active_ = nullptr;
    activeSlot_ = tasks_.size();

    if (restartRequested_) {
      state = kTaskStateRestart;
      continue;
    }
    if (result.reason == psx::cpu::ExecutionExitReason::CooperativeYield) {
      task.resumeAddress = result.guestPc;
      task.budgetResumes = 0;
      return;
    }
    if (result.reason == psx::cpu::ExecutionExitReason::BudgetExhausted) {
      if (resumeAcrossField(core, slot, result)) {
        return;
      }
      std::abort();
    }
    if (result.returned()) {
      lucent::error("tomba1-frame", "guest task {} returned instead of reaching its cooperative yield", slot);
      std::abort();
    }
    // A budget exit names a guest PC, and at a loop back-edge that PC is the LOOP, not the code that
    // entered it. The same PC answers "an in-flight copy is finishing" and "a copy is spinning on a
    // count nothing will reduce", and only the register file separates those two, so the whole file
    // is printed rather than a chosen few: a diagnostic that names the loop but not the callee-saved
    // registers cannot recover the loop's own counter, source cursor or terminator, and asking for
    // that as a follow-up is what turns a measured exit into a guess.
    //
    // It is `task.context`, not the Core: the slot already restored Core to the frame loop's own
    // registers, so reading the Core here would report the loop context and name the loop as the
    // caller of the exit. `task.context` is the state the exit actually left behind.
    reportGuestExit(core, slot, result, task.context);
    std::abort();
  }

  lucent::error("tomba1-frame", "task restarted more than four times in one native frame");
  std::abort();
}

// A budget exit is an ORDINARY bounded exit, not a fault: the framework's executor contract says so,
// and a guest that simply has more work than one display field is not misbehaving. On real hardware
// that call would run for as many fields as it needs and the rest of the machine would keep going, so
// the faithful response is to leave the task exactly where the exit found it and let the next field
// continue from the same PC with the same registers.
//
// Two things this must NOT become, and each has its own check:
//
//   * A silent hang. Resuming forever is indistinguishable from a spin, so the resume count is
//     bounded by `kMaxBudgetResumesPerCall` and the bound is reported with the state that hit it.
//   * A resumed task that re-enters from its ENTRY, silently restarting the call and never finishing
//     it while looking healthy. `contextReady` is what distinguishes a resume from a fresh start, and
//     the resume address must be the PC the exit actually stopped at, not the recorded entry.
//
// The record is re-armed to 2 ("started, runnable") because 4 means "running right now" and is only
// true during the dispatch above. Writing 2 is the same encoding `createTask` uses, and the two are
// told apart by `contextReady`, not by the record — which is why nothing here has to invent a fourth
// task state the guest's own table has no room for.
bool GuestTaskSlots::resumeAcrossField(Core &core, std::size_t slot, const psx::cpu::ExecutionResult &result) {
  const std::uint32_t record = taskRecordAddress(slot);
  TaskSlot &task = tasks_[slot];
  ++task.budgetResumes;
  if (task.budgetResumes > kMaxBudgetResumesPerCall) {
    lucent::error("tomba1-frame",
                  "guest task {} spent {} consecutive display fields without reaching its cooperative "
                  "yield; last resume 0x{:08X} after {} cycles — the task record was re-armed {} times "
                  "beyond the {} allowed for one call, so this is a guest call that makes no progress, "
                  "not one that is merely long",
                  slot,
                  task.budgetResumes,
                  result.guestPc,
                  result.cycles,
                  task.budgetResumes - 1u,
                  kMaxBudgetResumesPerCall);
    reportGuestExit(core, slot, result, task.context);
    return false;
  }
  task.resumeAddress = result.guestPc;
  core.mem_w16(record + kTaskRecordState, kTaskStateRunnable);
  lucent::info("tomba1-frame",
               "guest task {} used its whole field budget at 0x{:08X} ({} cycles) and resumes on field {} of "
               "{} for this call",
               slot,
               result.guestPc,
               result.cycles,
               task.budgetResumes,
               kMaxBudgetResumesPerCall);
  return true;
}

void GuestTaskSlots::startOverride(Core *core) {
  if (!active_ || !core) {
    lucent::error("tomba1-frame", "guest task start reached outside the native frame owner");
    std::abort();
  }
  active_->createTask(*core, core->r[4], core->r[5]);
}

void GuestTaskSlots::runScheduledTasks(Core &core) {
  const R3000 loopContext = static_cast<R3000 &>(core);
  for (std::size_t slot = 0; slot < tasks_.size(); ++slot) {
    const std::uint32_t record = taskRecordAddress(slot);
    const std::uint16_t state = core.mem_r16(record + kTaskRecordState);
    if (state == kTaskStateRunnable || state == kTaskStateRestart) {
      runTaskSlot(core, slot, state, loopContext);
    }
  }
}

void GuestTaskSlots::yieldTask(Core &core) {
  const std::uint32_t record = core.mem_r32(kCurrentTask);
  if (!validTaskRecord(record) || activeSlot_ >= tasks_.size()) {
    lucent::error("tomba1-frame", "cooperative yield has no active measured task");
    std::abort();
  }
  TaskSlot &task = tasks_[activeSlot_];
  if (!task.contextReady || record != taskRecordAddress(activeSlot_)) {
    lucent::error("tomba1-frame", "cooperative yield changed the active task record");
    std::abort();
  }
  core.mem_w16(record + kTaskRecordStep, static_cast<std::uint16_t>(core.r[4]));
  core.mem_w16(record + kTaskRecordState, kTaskStateWaiting);
  task.context = static_cast<R3000 &>(core);
  task.contextReady = true;
  psx::cpu::requestExecutionExit(core, psx::cpu::ExecutionExitReason::CooperativeYield);
}

void GuestTaskSlots::restartTask(Core &core) {
  const std::uint32_t record = core.mem_r32(kCurrentTask);
  if (!validTaskRecord(record) || activeSlot_ >= tasks_.size()) {
    lucent::error("tomba1-frame", "task restart has no active measured task");
    std::abort();
  }
  TaskSlot &task = tasks_[activeSlot_];
  if (!task.contextReady || record != taskRecordAddress(activeSlot_)) {
    lucent::error("tomba1-frame", "task restart changed the active task record");
    std::abort();
  }
  core.mem_w16(record + kTaskRecordState, kTaskStateRestart);
  core.mem_w32(record + kTaskRecordEntry, core.r[4]);
  restartRequested_ = true;
  psx::cpu::requestExecutionExit(core, psx::cpu::ExecutionExitReason::CooperativeYield);
}

void GuestTaskSlots::yieldOverride(Core *core) {
  if (!active_ || !core) {
    lucent::error("tomba1-frame", "guest yield reached outside the native frame owner");
    std::abort();
  }
  active_->yieldTask(*core);
}

void GuestTaskSlots::restartOverride(Core *core) {
  if (!active_ || !core) {
    lucent::error("tomba1-frame", "guest task restart reached outside the native frame owner");
    std::abort();
  }
  active_->restartTask(*core);
}

} // namespace tomba1
