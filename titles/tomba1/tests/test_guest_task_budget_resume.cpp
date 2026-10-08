// THE SYNTHETIC GUEST-SPIN REGRESSION for docs/issues/0007.
//
// That issue fixed the symptom — Tomba! 1's frame driver aborted on
// `ExecutionExitReason::BudgetExhausted` while a legitimate LZ77 glyph decompress at `0x8003EF50`
// emitted 286,720 bytes and reached only 45,200 of them inside one field's 564,480-cycle budget, so
// that one call needs at least seven display fields — and it proved the fix's red case by LOWERING
// `kMaxBudgetResumesPerCall` to 1 and running the product against the operator's disc. It said
// outright that 32 "remains unexercised by a run because the real call finishes in about seven".
//
// This is that missing run, and it is a run of the SHIPPING SCHEDULER against synthetic guests
// rather than of the product against a disc, so it needs no media, is deterministic, and executes in
// about a second:
//
//   * A LONG FINITE CALL — a guest that counts down in RAM and then reaches its cooperative yield.
//     This is the decompress's shape: more work than one field, an owned end. It must complete, it
//     must complete by RESUMING (the entry counter proves the entry ran once, not once per field),
//     and every field before the yield must leave the record re-armed runnable.
//
//   * A GUEST SPIN — a guest that never reaches its yield, with one cooperative yield partway
//     through and then nothing but an unconditional self-branch. It must be admitted exactly
//     `kMaxBudgetResumesPerCall` times after that yield, refuse the next field, and die by SIGABRT
//     with the resume address and the whole register file in the report. The yield matters: it is
//     what makes the count measure one UNBROKEN CALL rather than the slot's lifetime, so the refused
//     field index is `yieldField + 1 + kMaxBudgetResumesPerCall`, and a count that survived the
//     yield would land thirty-two fields earlier.
//
// Nothing here fast-forwards the simulation, skips a lifecycle callback, or writes a phase or a
// timer. Each field is one real `dispatchGuestUntilExit` over a real field budget on the real
// dynarec, and the only thing the test supplies is guest RAM: the two MIPS bodies below and the task
// record the guest's own boot prefix would have written.
//
// THE GUESTS ARE R3000 CODE, WHICH HAS TWO RULES THIS FILE FOLLOWS AND THE FIRST VERSION DID NOT:
//
//   * A load is followed by a NOP before any instruction that WRITES the loaded register. The
//     load-delay slot is not modelled, so `lw $t1, 0($t0)` immediately followed by
//     `addiu $t1, $t1, 1` reads the PREVIOUS value of `$t1` and stores it back — which is exactly
//     what the first version of the countdown guest did, silently, until its entry counter came out
//     unchanged. A synthetic guest that trips over this looks like a product bug and is not one.
//   * A count that does not fit a signed 16-bit immediate is materialised with `lui`/`ori`.
//
// Every field this test counts actually ran guest instructions: `executedInstructions` is asserted
// against the guest's own work count, and the interpreter-fallback counter against zero, so a
// scheduler that dispatched nothing — or that quietly interpreted everything — cannot satisfy the
// rest of this file.
#include "core.h"
#include "game.h"
#include "game_runtime.h"
#include "guest_task_slots.h"
#include "lightrec_executor.h"
#include "native_dispatch.h"

#include <sys/wait.h>
#include <unistd.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace {

// THE SHIPPING BOUND, read from the owner rather than restated. Every field-count assertion below is
// written against this value, which is what makes them a gate on 32 itself: change the constant and
// both cases go red, in opposite directions.
constexpr std::uint32_t kBound = tomba1::kMaxBudgetResumesPerCall;

// ---------------------------------------------------------------------------------------------
// The MIPS encodings this file's guests need, written out here rather than imported.
//
// They are an INDEPENDENT restatement, on purpose. If these were the production encoders the guests
// would agree with them by construction and the assertions below would prove nothing; what they have
// to agree with is the hardware, and the only thing that checks that here is a real Lightrec
// translation of these words reaching a real cooperative yield at a real guest address.
// ---------------------------------------------------------------------------------------------
constexpr std::uint32_t kNop = 0x00000000u;

constexpr std::uint32_t encodeLui(std::uint32_t rt, std::uint16_t imm) {
  return 0x3C000000u | (rt << 16u) | imm;
}
constexpr std::uint32_t encodeOri(std::uint32_t rt, std::uint32_t rs, std::uint16_t imm) {
  return 0x34000000u | (rs << 21u) | (rt << 16u) | imm;
}
constexpr std::uint32_t encodeAddiu(std::uint32_t rt, std::uint32_t rs, std::int16_t imm) {
  return 0x24000000u | (rs << 21u) | (rt << 16u) | (static_cast<std::uint32_t>(imm) & 0xFFFFu);
}
constexpr std::uint32_t encodeLw(std::uint32_t rt, std::uint32_t base, std::int16_t offset) {
  return 0x8C000000u | (base << 21u) | (rt << 16u) | (static_cast<std::uint32_t>(offset) & 0xFFFFu);
}
constexpr std::uint32_t encodeSw(std::uint32_t rt, std::uint32_t base, std::int16_t offset) {
  return 0xAC000000u | (base << 21u) | (rt << 16u) | (static_cast<std::uint32_t>(offset) & 0xFFFFu);
}
constexpr std::uint32_t encodeBeqz(std::uint32_t rs, std::int16_t offset) {
  return 0x10000000u | (rs << 21u) | (static_cast<std::uint32_t>(offset) & 0xFFFFu);
}
constexpr std::uint32_t encodeJ(std::uint32_t target) {
  return 0x08000000u | ((target >> 2u) & 0x03FFFFFFu);
}
constexpr std::uint32_t encodeJal(std::uint32_t target) {
  return 0x0C000000u | ((target >> 2u) & 0x03FFFFFFu);
}

// A full 32-bit constant in two instructions. `ori` zero-extends its immediate, so a value whose low
// half is 0x8000 or more would come out 65,536 too large; that case goes through `addiu`'s sign
// extension with the high half carried instead.
void writeConstant(Core &core, std::uint32_t at, std::uint32_t rt, std::uint32_t value) {
  const std::uint32_t high = value & 0xFFFF0000u;
  const std::uint32_t low = value & 0xFFFFu;
  if (low < 0x8000u) {
    core.mem_w32(at, encodeLui(rt, static_cast<std::uint16_t>(high >> 16u)));
    core.mem_w32(at + 4u, encodeOri(rt, rt, static_cast<std::uint16_t>(low)));
    return;
  }
  core.mem_w32(at, encodeLui(rt, static_cast<std::uint16_t>((high + 0x10000u) >> 16u)));
  core.mem_w32(at + 4u, encodeAddiu(rt, rt, static_cast<std::int16_t>(static_cast<std::int32_t>(low) - 0x10000)));
}

// Register numbers.
constexpr std::uint32_t kT0 = 8u;
constexpr std::uint32_t kT1 = 9u;
constexpr std::uint32_t kT2 = 10u;
constexpr std::uint32_t kT3 = 11u;
constexpr std::uint32_t kT4 = 12u;
constexpr std::uint32_t kZero = 0u;

// Guest RAM this test owns: two synthetic bodies, their scratch cells, and the three native task
// leaves they call. All of it is inside ONE activated code image, because a RAM address in no
// active image is a typed fault to the dispatcher, not executable guest code.
//
// The image range is in PHYSICAL addresses, not KSEG0: `containsPhysical` masks the address it is
// asked about but compares it against the range's own `begin`/`end` unmasked, so a range written as
// 0x80017100 claims nothing at all and every dispatch of this test's guests faults.
constexpr std::uint32_t kImageBegin = 0x00017100u;
constexpr std::uint32_t kImageEnd = 0x00017900u;

constexpr std::uint32_t kCountdownEntry = 0x80017400u;
constexpr std::uint32_t kSpinEntry = 0x80017500u;

constexpr std::uint32_t kCountdownEntryCell = 0x80017800u;
constexpr std::uint32_t kCountdownCell = 0x80017804u;
constexpr std::uint32_t kSpinCountCell = 0x80017808u;
constexpr std::uint32_t kSpinArmCell = 0x8001780Cu;

// A stack the guests never touch. `runTaskSlot` reads the record's stack word for `$sp`, and the
// synthetic bodies keep every value in registers and in the cells above.
constexpr std::uint32_t kTaskStack = 0x800F0000u;

// The countdown the long finite guest is sized with, and the field cap that bounds both cases.
constexpr std::uint32_t kCountdownInitial = 150000u;
constexpr int kFieldCap = 96;

// THE COUNTDOWN GUEST — the decompress's shape. Decrements a RAM cell, and on reaching zero calls
// the measured cooperative-yield leaf. `kCountdownEntryCell` counts how many times the ENTRY ran,
// which is the observation that separates a resume from a restart: a resume continues at the exit
// PC, so this cell must read exactly one.
void writeCountdownGuest(Core &core) {
  const std::uint32_t base = kCountdownEntry;
  const std::uint32_t loop = base + 0x2Cu;
  const std::uint32_t done = base + 0x48u;
  core.mem_w32(base + 0x00u, encodeLui(kT0, static_cast<std::uint16_t>(kCountdownEntryCell >> 16u)));
  core.mem_w32(base + 0x04u, encodeOri(kT0, kT0, static_cast<std::uint16_t>(kCountdownEntryCell & 0xFFFFu)));
  core.mem_w32(base + 0x08u, encodeLw(kT1, kT0, 0));
  core.mem_w32(base + 0x0Cu, kNop);
  core.mem_w32(base + 0x10u, encodeAddiu(kT1, kT1, 1));
  core.mem_w32(base + 0x14u, encodeSw(kT1, kT0, 0));
  core.mem_w32(base + 0x18u, encodeLui(kT0, static_cast<std::uint16_t>(kCountdownCell >> 16u)));
  core.mem_w32(base + 0x1Cu, encodeOri(kT0, kT0, static_cast<std::uint16_t>(kCountdownCell & 0xFFFFu)));
  writeConstant(core, base + 0x20u, kT1, kCountdownInitial);
  core.mem_w32(base + 0x28u, encodeSw(kT1, kT0, 0));
  core.mem_w32(loop + 0x00u, encodeLw(kT2, kT0, 0));
  core.mem_w32(loop + 0x04u, encodeBeqz(kT2, static_cast<std::int16_t>((done - (loop + 0x08u)) / 4u)));
  core.mem_w32(loop + 0x08u, kNop);
  core.mem_w32(loop + 0x0Cu, encodeAddiu(kT2, kT2, -1));
  core.mem_w32(loop + 0x10u, encodeSw(kT2, kT0, 0));
  core.mem_w32(loop + 0x14u, encodeJ(loop));
  core.mem_w32(loop + 0x18u, kNop);
  core.mem_w32(done + 0x00u, encodeJal(tomba1::kTaskYieldEntry));
  core.mem_w32(done + 0x04u, kNop);
  // Reached only if something re-armed a yielded record, which nothing in this test does.
  core.mem_w32(done + 0x08u, encodeJ(done));
  core.mem_w32(done + 0x0Cu, kNop);
}

// THE SPIN GUEST — the refusal case. The same countdown, except that reaching zero YIELDS ONCE (if
// the arm cell is set) and then drops into an unconditional self-branch, so after the yield the call
// can make no progress at all. The arm cell is what makes the yield land in the middle of the call
// rather than at its end, which is the only way to observe that the resume count is per CALL.
void writeSpinGuest(Core &core, std::uint32_t countdown) {
  const std::uint32_t base = kSpinEntry;
  const std::uint32_t loop = base + 0x24u;
  const std::uint32_t checkArm = base + 0x40u;
  const std::uint32_t spin = base + 0x60u;
  core.mem_w32(base + 0x00u, encodeLui(kT0, static_cast<std::uint16_t>(kSpinCountCell >> 16u)));
  core.mem_w32(base + 0x04u, encodeOri(kT0, kT0, static_cast<std::uint16_t>(kSpinCountCell & 0xFFFFu)));
  writeConstant(core, base + 0x08u, kT1, countdown);
  core.mem_w32(base + 0x10u, encodeSw(kT1, kT0, 0));
  core.mem_w32(base + 0x14u, encodeLui(kT2, static_cast<std::uint16_t>(kSpinArmCell >> 16u)));
  core.mem_w32(base + 0x18u, encodeOri(kT2, kT2, static_cast<std::uint16_t>(kSpinArmCell & 0xFFFFu)));
  core.mem_w32(base + 0x1Cu, encodeAddiu(kT3, kZero, 1));
  core.mem_w32(base + 0x20u, encodeSw(kT3, kT2, 0));
  core.mem_w32(loop + 0x00u, encodeLw(kT4, kT0, 0));
  core.mem_w32(loop + 0x04u, encodeBeqz(kT4, static_cast<std::int16_t>((checkArm - (loop + 0x08u)) / 4u)));
  core.mem_w32(loop + 0x08u, kNop);
  core.mem_w32(loop + 0x0Cu, encodeAddiu(kT4, kT4, -1));
  core.mem_w32(loop + 0x10u, encodeSw(kT4, kT0, 0));
  core.mem_w32(loop + 0x14u, encodeJ(loop));
  core.mem_w32(loop + 0x18u, kNop);
  core.mem_w32(checkArm + 0x00u, encodeLw(kT4, kT2, 0));
  core.mem_w32(checkArm + 0x04u, encodeBeqz(kT4, static_cast<std::int16_t>((spin - (checkArm + 0x08u)) / 4u)));
  core.mem_w32(checkArm + 0x08u, kNop);
  core.mem_w32(checkArm + 0x0Cu, encodeSw(kZero, kT2, 0));
  core.mem_w32(checkArm + 0x10u, encodeJal(tomba1::kTaskYieldEntry));
  core.mem_w32(checkArm + 0x14u, kNop);
  core.mem_w32(checkArm + 0x18u, encodeJ(loop));
  core.mem_w32(checkArm + 0x1Cu, kNop);
  core.mem_w32(spin + 0x00u, encodeJ(spin));
  core.mem_w32(spin + 0x04u, kNop);
}

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

// The three measured task leaves, bound on this Core exactly as `Tomba1Runtime::registerOverrides`
// binds them in the product. The yield leaf is the one the guests actually call.
bool installTaskLeaves(Core &core) {
  const auto image = core.imageCatalog().resolve(tomba1::kTaskYieldEntry);
  if (!image) {
    return false;
  }
  return core.nativeDispatcher().install({{*image, tomba1::kTaskStartEntry},
                                          "GuestTaskSlots::startOverride",
                                          tomba1::GuestTaskSlots::startOverride}) &&
         core.nativeDispatcher().install({{*image, tomba1::kTaskYieldEntry},
                                          "GuestTaskSlots::yieldOverride",
                                          tomba1::GuestTaskSlots::yieldOverride}) &&
         core.nativeDispatcher().install({{*image, tomba1::kTaskRestartEntry},
                                          "GuestTaskSlots::restartOverride",
                                          tomba1::GuestTaskSlots::restartOverride});
}

int failed = 0;

void check(bool condition, const char *name) {
  if (condition) {
    return;
  }
  ++failed;
  std::fprintf(stderr, "FAIL: %s\n", name);
}

// The same check with the claim stated per call, so a failure says WHICH call and what it read.
void checkCall(bool condition, int call, const char *what, unsigned long value) {
  if (condition) {
    return;
  }
  ++failed;
  std::fprintf(stderr, "FAIL: call %d: %s (read %lu)\n", call + 1, what, value);
}

// One completed field, as the child reported it: the field index and the task record's state word
// after it. A field that ends in the refusal produces no line at all, which is how the parent learns
// WHICH field the bound fired on.
struct FieldReport {
  int field = 0;
  unsigned state = 0;
};

std::vector<FieldReport> parseFieldLog(const std::string &text) {
  std::vector<FieldReport> reports;
  std::size_t at = 0;
  while (at < text.size()) {
    const std::size_t end = text.find('\n', at);
    if (end == std::string::npos) {
      break;
    }
    FieldReport report{};
    if (std::sscanf(text.c_str() + at, "%d %u", &report.field, &report.state) == 2) {
      reports.push_back(report);
    }
    at = end + 1u;
  }
  return reports;
}

// THE FINITE CASE, in the parent. Runs one display field at a time until the task yields, and
// reports how many fields the call needed, so the spin case is sized from measured work rather than
// from a guess about how many loop iterations fit in a field.
int measureFiniteCall(Core &core) {
  tomba1::GuestTaskSlots tasks;
  writeCountdownGuest(core);
  core.mem_w32(tomba1::taskRecordAddress(0) + tomba1::kTaskRecordStack, kTaskStack);

  const auto &counters = core.lightrecExecutor().counters();
  const std::uint64_t instructionsBefore = counters.executedInstructions;
  const std::uint64_t fallbacksBefore = counters.fallback.calls;
  int firstCallFields = 0;

  // TWO CALLS, ONE SLOT OWNER, because the resume count is cleared by a fresh task start as well as
  // by a yield, and only a second call on the SAME owner can see that. A count left over from the
  // first call would silently shorten the second call's allowance — and since the second call is
  // demonstrably shorter than the bound on its own, it would go unnoticed without this.
  for (int call = 0; call < 2; ++call) {
    tasks.createTask(core, 0, kCountdownEntry);
    int fields = 0;
    for (; fields < kFieldCap; ++fields) {
      tasks.runScheduledTasks(core);
      const std::uint16_t state = core.mem_r16(tomba1::taskRecordAddress(0) + tomba1::kTaskRecordState);
      if (state == tomba1::kTaskStateWaiting) {
        break;
      }
      check(state == tomba1::kTaskStateRunnable,
            "a field that spent its budget must leave the task record re-armed runnable");
    }
    checkCall(fields >= 2, call, "the call must span MORE THAN ONE display field", static_cast<unsigned long>(fields));
    checkCall(
        fields < kFieldCap, call, "the call must reach its cooperative yield", static_cast<unsigned long>(fields));
    checkCall(fields <= kBound, call, "the call must finish inside the bound", static_cast<unsigned long>(fields));
    checkCall(core.mem_r32(kCountdownEntryCell) == static_cast<std::uint32_t>(call + 1),
              call,
              "the guest ENTRY must run once per call and resume every later field, never re-enter per field",
              core.mem_r32(kCountdownEntryCell));
    checkCall(core.mem_r32(kCountdownCell) == 0u,
              call,
              "the countdown must finish its work before it yields",
              core.mem_r32(kCountdownCell));
    check(core.mem_r16(tomba1::taskRecordAddress(0) + tomba1::kTaskRecordState) == tomba1::kTaskStateWaiting,
          "a reached cooperative yield must leave the record waiting, not runnable");
    std::fprintf(stderr,
                 "call %d: %d display fields, entry cell %u, count cell %u\n",
                 call + 1,
                 fields,
                 core.mem_r32(kCountdownEntryCell),
                 core.mem_r32(kCountdownCell));
    if (call == 0) {
      firstCallFields = fields;
    }
  }

  // THE INSTRUMENTS RAN. Without these, every other assertion here is consistent with a scheduler
  // that dispatched nothing at all, and with one that quietly interpreted the guest instead of
  // translating it: the dead-tap shape, a confident answer about the wrong subject.
  const std::uint64_t executed = counters.executedInstructions - instructionsBefore;
  check(executed >= 2u * kCountdownInitial,
        "the countdown guest really executed guest instructions, at least one per unit of its work, in BOTH calls");
  check(counters.fallback.calls == fallbacksBefore, "the guests ran on the dynarec with no interpreter fallback");
  return firstCallFields;
}

} // namespace

int main() {
  Runtime runtime;
  psxport_install_game(runtime);

  auto game = std::make_unique<Game>();
  Core &core = game->core;
  core.imageCatalog().activate("tomba1-budget-resume", {kImageBegin, kImageEnd}, 0x42554447ull);
  check(installTaskLeaves(core), "the measured task leaves bind to the activated test image");

  const int finiteFields = measureFiniteCall(core);

  // The bound's own arithmetic, asserted rather than restated (docs/issues/0007): 32 is about four
  // times the longest MEASURED call — the seven-field glyph decompress — and far below the ~2,000
  // fields a real spin would need to be mistaken for progress. The equality is the gate on the
  // NUMBER, so a careless edit is red rather than a silent change of headroom; the other two state
  // why the number is what it is, so the relationship is checked as well as the value.
  check(kBound == 32u, "kMaxBudgetResumesPerCall is the derived value 32, not a number to retune");
  check(kBound >= 4u * 7u, "the bound keeps the four-times headroom over the measured seven-field call");
  check(kBound < 2000u, "the bound stays far below the ~2000 fields a real spin would need to be mistaken for");

  // Size the spin guest's pre-yield countdown from the field the finite call actually took, so its
  // yield lands in the middle of a multi-field call. A coarse division is fine: the assertions that
  // follow need only "more than one field", and the child's own yield field is read back rather than
  // assumed.
  const std::uint32_t perField = kCountdownInitial / static_cast<std::uint32_t>(finiteFields > 0 ? finiteFields : 1);
  const std::uint32_t spinCountdown = (perField * 3u) > 0u ? (perField * 3u) : 1u;

  // THE SPIN CASE, in a child, because the refusal is `std::abort()` and stays that way: a guest that
  // will never reach its yield is not a recoverable state. ctest does not invert WILL_FAIL for a
  // signalled child, so the death is observed the way a process can be observed — and testing the
  // SIGNAL rather than a non-zero exit is the stronger claim, because it tells the refusal apart
  // from the driver quietly returning.
  int fieldPipe[2] = {-1, -1};
  int reportPipe[2] = {-1, -1};
  check(pipe(fieldPipe) == 0, "field-report pipe created");
  check(pipe(reportPipe) == 0, "child-report pipe created");
  if (failed != 0) {
    return 1;
  }

  const pid_t child = fork();
  if (child == 0) {
    close(fieldPipe[0]);
    close(reportPipe[0]);
    dup2(reportPipe[1], STDERR_FILENO);
    close(reportPipe[1]);

    // THE PARENT'S OWN CORE, not a fresh one. Lightrec initialises exactly one machine per process
    // and refuses a second ("Lightrec supports one initialized machine per process"), so a second
    // `Game` here would fault before executing a single guest instruction. What the case needs
    // instead is a clean SLOT, and a fresh `GuestTaskSlots` with the guest code and task record
    // rewritten is exactly that: the resume count starts at zero and the record is armed at the
    // spin guest's entry, so no field count here is a property of the order the two cases run in.
    writeSpinGuest(core, spinCountdown);
    core.mem_w32(tomba1::taskRecordAddress(0) + tomba1::kTaskRecordStack, kTaskStack);

    tomba1::GuestTaskSlots tasks;
    tasks.createTask(core, 0, kSpinEntry);
    const std::uint32_t record = tomba1::taskRecordAddress(0);
    char line[64];
    for (int field = 0; field < kFieldCap; ++field) {
      tasks.runScheduledTasks(core);
      const std::uint16_t state = core.mem_r16(record + tomba1::kTaskRecordState);
      const int written = std::snprintf(line, sizeof(line), "%d %u\n", field, state);
      if (written > 0) {
        (void)!write(fieldPipe[1], line, static_cast<std::size_t>(written));
      }
      // THE GUEST'S OWN TASK SWITCH, which this host stands in for. A yielded record is left waiting
      // and nothing in the SCHEDULER puts it back on the runnable list: in the product that is guest
      // code, running in the main task, that re-arms each runnable record once per frame. Without
      // this the spin would be admitted zero times after its yield, which would make the case pass
      // for the wrong reason — a task nobody re-arms is not a spin, it is a stopped task.
      if (state == tomba1::kTaskStateWaiting) {
        core.mem_w16(record + tomba1::kTaskRecordState, tomba1::kTaskStateRunnable);
      }
    }
    // Only reached if the bound never fired, which is the failure this whole case exists to catch.
    _exit(0);
  }

  close(fieldPipe[1]);
  close(reportPipe[1]);
  check(child > 0, "fork produced a child");
  if (child <= 0) {
    return 1;
  }

  std::string fieldLog;
  std::string report;
  char buffer[4096];
  ssize_t got = 0;
  while ((got = read(fieldPipe[0], buffer, sizeof(buffer))) > 0) {
    fieldLog.append(buffer, static_cast<std::size_t>(got));
  }
  close(fieldPipe[0]);
  while ((got = read(reportPipe[0], buffer, sizeof(buffer))) > 0) {
    report.append(buffer, static_cast<std::size_t>(got));
  }
  close(reportPipe[0]);

  int status = 0;
  check(waitpid(child, &status, 0) == child, "reaped the child");

  const std::vector<FieldReport> fields = parseFieldLog(fieldLog);
  int yieldField = -1;
  for (const FieldReport &entry : fields) {
    if (entry.state == tomba1::kTaskStateWaiting) {
      yieldField = entry.field;
      break;
    }
  }
  std::size_t runnableFields = 0;
  for (const FieldReport &entry : fields) {
    if (entry.state == tomba1::kTaskStateRunnable) {
      ++runnableFields;
    }
  }
  std::fprintf(stderr,
               "spin case: %zu fields returned (%zu re-armed runnable), cooperative yield on field %d\n",
               fields.size(),
               runnableFields,
               yieldField);

  check(WIFSIGNALED(status), "the spinning guest task was REFUSED, not resumed forever");
  check(WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT, "the refusal was SIGABRT");
  check(!fields.empty(), "the spin case ran at least one display field before the refusal");
  check(yieldField >= 2, "the spin guest's cooperative yield must land inside a multi-field call");
  // The refusal happened on the field AFTER the last one that returned, so the number of fields this
  // call is allowed is exactly what this pins — and it is measured off the child's own field log, not
  // off a value the parent chose.
  check(fields.size() == static_cast<std::size_t>(yieldField + 1) + kBound,
        "a call is admitted exactly kMaxBudgetResumesPerCall times AFTER its yield and refused on the next field");
  check(fields.size() > kBound, "the resume count cannot be cumulative across the slot's lifetime");
  // Every field but the yield's left the record re-armed runnable, so the refusal is a refusal and
  // not a record the scheduler lost.
  check(runnableFields == fields.size() - 1u && yieldField >= 0,
        "every field but the cooperative yield's left the task record re-armed runnable");

  // The refusal has to SAY SO, with the state that hit the bound: the resume address, the field count,
  // and the whole register file. A refusal that only ends the process is a hang with extra steps, and
  // the report is the part a person actually reads.
  check(report.find("spent") != std::string::npos &&
            report.find("without reaching its cooperative") != std::string::npos,
        "the refusal names the consecutive fields a call spent without yielding");
  check(report.find("beyond the") != std::string::npos, "the refusal names the bound it exceeded");
  check(report.find("guest-regs zero=0x") != std::string::npos, "the refusal prints the exited task's register file");
  // `ra` is the LAST register of the LAST printed line, so it is present only if all thirty-two were
  // printed. A diagnostic that names the loop but not the callee-saved registers cannot recover the
  // loop's own counter or source cursor, and asking for that afterwards turns a measured exit into a
  // guess.
  check(report.find("ra=0x") != std::string::npos, "the register file is printed in full, not as a chosen few");

  if (failed != 0) {
    std::fprintf(
        stderr, "FAIL: %d check(s) failed\n--- child report ---\n%s-------------------\n", failed, report.c_str());
    return 1;
  }
  std::puts("PASS: a multi-field guest call completes by resuming, and a guest spin is refused at the bound");
  return 0;
}
