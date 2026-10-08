// tests/test_frame_cadence.cpp — the frame-cadence owner's own state machine.
//
// WHAT IS UNDER TEST: tomba::FrameCadence, the owner of "how many display fields one Tomba! 2
// logic frame spans" and of the observation of the guest's own dwell counter. The recovery and the
// instruction words are in game/core/frame_cadence.h, each read off MAIN.EXE.
// This test is about the OWNER's behaviour on top of them.
//
// Every case here is a case that WOULD fail. The gate's own `sltu` at 0x80050CD8 is reproduced as
// a truth table over the counter. The central case is the one this class exists to get right: the
// port must NOT write the counter, because the guest's own incrementer (LAB_800506B4, vsync
// callback slot 4) already runs in this product through the port's own sequencer tick. A port that
// increments the counter as well would count every field twice and then check a number it
// manufactured itself. The u16 wrap is checked because the image's `sh` has it, and the accounting
// denominators are checked because a cadence that drifts silently is the defect this class exists
// to prevent. The last block is the 60 fps arithmetic: one field per logic frame is what the
// threshold has to hold for, and the owner already publishes and reports that one number.
#include "core.h"
#include "core/frame/frame_cadence.h"

#include <cstdint>
#include <cstdio>
#include <memory>

namespace {

int failures = 0;

void check(bool condition, const char *detail) {
  if (!condition) {
    std::printf("FAIL: %s\n", detail);
    ++failures;
  }
}

// A Core is a whole machine's RAM, so a second one here is the isolation control: an owner that
// reached through a shared pointer rather than its own would show up as a write in `other`.
std::unique_ptr<tomba::FrameCadence> wire(Core &core) {
  auto cadence = std::make_unique<tomba::FrameCadence>();
  cadence->core = &core;
  return cadence;
}

// Stand in for the guest's own incrementer. In the product this is LAB_800506B4 running through
// LibapiIntr::runVblankCallbacks; in a unit test the guest is not there, so the test performs the
// guest's store explicitly and then checks that the owner left it alone.
void guestIncrementsCounter(Core &core) {
  core.mem_w16(tomba::FrameCadence::kDwellCounterAddress,
               static_cast<std::uint16_t>(core.mem_r16(tomba::FrameCadence::kDwellCounterAddress) + 1u));
}

} // namespace

int main() {
  using tomba::FrameCadence;

  auto core = std::make_unique<Core>();
  auto other = std::make_unique<Core>();
  auto cadence = wire(*core);
  wire(*other);

  // The measured addresses are the guest's own fields, and the quota is a byte in scratchpad while
  // the counter is a halfword in main RAM — a store of the wrong width would be invisible here and
  // corrupt the neighbouring field in a run.
  check(FrameCadence::kQuotaAddress == 0x1F800235u, "quota address is not the guest's scratchpad byte");
  check(FrameCadence::kDwellCounterAddress == 0x800E809Cu, "dwell counter address is not the guest's halfword");
  check(FrameCadence::kRetailVblanksPerLogicFrame == 2u, "retail cadence is not 2 fields per logic frame");

  // ---- publish puts the decision in the guest's own field -----------------------------------
  // Pre-load the fields with values that are NOT the answer, so a publish that did nothing, or
  // that wrote the wrong width, cannot pass by leaving an already-correct byte behind.
  core->mem_w8(FrameCadence::kQuotaAddress, 0xA5u);
  core->mem_w16(FrameCadence::kDwellCounterAddress, 0x5A5Au);
  // The byte above the quota field is a real, separate guest byte the guest also zeroes
  // (DAT_1F800236). Seed it with a value the quota is not, so a store one byte too wide is caught
  // rather than landing on a byte that was already correct.
  core->mem_w16(FrameCadence::kQuotaAddress + 1u, 0x0B0Bu);
  cadence->publish();
  check(core->mem_r8(FrameCadence::kQuotaAddress) == FrameCadence::kRetailVblanksPerLogicFrame,
        "publish did not put the decision in the guest's quota byte");
  check(core->mem_r16(FrameCadence::kDwellCounterAddress) == 0x5A5Au,
        "publish touched the dwell counter, which the guest's store does not cover");
  check(core->mem_r16(FrameCadence::kQuotaAddress + 1u) == 0x0B0Bu,
        "publish wrote past the one byte the guest's store covers");

  // ---- beginLogicFrame performs the guest's own reset --------------------------------------
  cadence->beginLogicFrame();
  check(cadence->dwellCounter() == 0u, "beginLogicFrame did not zero the dwell counter");
  check(!cadence->gateOpen(), "the gate reported open with the counter at zero");

  // ---- the central property: consuming a field does NOT write the counter -------------------
  // A port that increments the counter itself manufactures the number it then checks, and the
  // guest's own incrementer adds a second one per field on top. Seed the counter with the value
  // the guest's callback would have produced, spend a field, and require the field untouched.
  guestIncrementsCounter(*core);
  check(cadence->consumeVblank() == 1u, "consumeVblank did not report the guest's counter");
  check(core->mem_r16(FrameCadence::kDwellCounterAddress) == 1u,
        "consumeVblank WROTE the guest's dwell counter — the guest's own incrementer also runs");
  check(!cadence->gateOpen(), "the gate reported open on field 1 of 2 — the guest would still be spinning");

  guestIncrementsCounter(*core);
  check(cadence->consumeVblank() == 2u, "the second field did not report the guest's counter");
  check(cadence->gateOpen(), "the gate did not open once the counter reached the quota");
  check(cadence->gateOpen() == (cadence->dwellCounter() >= cadence->vblanksPerLogicFrame()),
        "the gate's test is not the guest's unsigned comparison");

  // ---- the guest's own writes are the guest's ----------------------------------------------
  // Two overlays carry a body that stores 1 into this halfword to shorten one frame
  // (game/core/frame_cadence.h), and the guest's own incrementer adds one per field. Neither may
  // be disturbed by a port that spends fields: a value above the quota is an open gate, and a
  // value the guest chose is the guest's.
  core->mem_w16(FrameCadence::kDwellCounterAddress, 9u);
  check(cadence->consumeVblank() == 9u, "the owner discarded a guest counter above the quota");
  check(cadence->gateOpen(), "a guest counter above the quota must read as an open gate");
  core->mem_w16(FrameCadence::kDwellCounterAddress, 1u);
  check(!cadence->gateOpen(), "a guest counter below the quota must read as a shut gate");

  // ---- the counter is a u16, and wraps as the image's `sh` does ----------------------------
  // The guest's incrementer at 0x800506C4 stores a u16, so 0xFFFF + 1 is 0 and the gate closes
  // again. An owner that treated the field as a monotonic 32-bit total would read that as a gate
  // open forever. The wrap belongs to the GUEST, so the test writes it the way the guest would.
  core->mem_w16(FrameCadence::kDwellCounterAddress, 0xFFFFu);
  guestIncrementsCounter(*core);
  check(cadence->dwellCounter() == 0u, "a guest counter of 0xFFFF did not wrap the way the image's `sh` does");
  check(!cadence->gateOpen(), "a wrapped guest counter of 0 must read as a shut gate, not a satisfied one");

  // ---- accounting, with denominators -------------------------------------------------------
  const std::uint32_t framesBefore = cadence->logicFrames();
  const std::uint32_t fieldsBefore = cadence->vblanksSpent();
  cadence->beginLogicFrame();
  check(cadence->logicFrames() == framesBefore + 1u, "beginLogicFrame did not count the frame");
  check(cadence->vblanksThisLogicFrame() == 0u, "a fresh logic frame reported fields it had not spent");
  check(cadence->unaccountedVblanks() == 0u, "a fresh logic frame opened with unaccounted fields");
  guestIncrementsCounter(*core);
  cadence->consumeVblank();
  guestIncrementsCounter(*core);
  cadence->consumeVblank();
  check(cadence->vblanksThisLogicFrame() == 2u, "the frame's own field count is wrong");
  check(cadence->vblanksSpent() == fieldsBefore + 2u, "the running field total is wrong");
  check(cadence->unaccountedVblanks() == 2u, "fields spent before the per-field work must read unaccounted");
  cadence->accountVblankWork();
  check(cadence->unaccountedVblanks() == 0u, "accounting the per-field work did not clear the residue");
  check(cadence->vblanksThisLogicFrame() == 2u, "accounting changed the frame's own field count");
  // The quiet path: the guest's counter reached the quota and nothing is unaccounted.
  cadence->endLogicFrame(1u);
  cadence->beginLogicFrame();
  check(cadence->vblanksThisLogicFrame() == 0u, "a new logic frame inherited the previous frame's field count");

  // ---- isolation: this Core's fields, not another's -----------------------------------------
  other->mem_w8(FrameCadence::kQuotaAddress, 0x3Cu);
  other->mem_w16(FrameCadence::kDwellCounterAddress, 0x1234u);
  cadence->publish();
  cadence->beginLogicFrame();
  cadence->consumeVblank();
  check(other->mem_r8(FrameCadence::kQuotaAddress) == 0x3Cu, "publish wrote another Core's quota byte");
  check(other->mem_r16(FrameCadence::kDwellCounterAddress) == 0x1234u,
        "the counter work reached another Core's halfword");

  // ---- the guest's own 60 fps lever, in arithmetic -------------------------------------------
  // One display field per logic frame is what the gate's threshold has to hold for the engine to
  // run a logic frame every vblank instead of every other one. This checks the comparison, not a
  // mode: the decision stays at the measured retail value and nothing here turns it on. What it
  // establishes is that the lever is ONE NUMBER the owner already publishes and reports, so
  // changing it is a decision rather than a reimplementation.
  check(cadence->vblanksPerLogicFrame() == FrameCadence::kRetailVblanksPerLogicFrame,
        "the owner does not start at the measured retail cadence");
  cadence->beginLogicFrame();
  guestIncrementsCounter(*core);
  check(!cadence->gateOpen(), "the gate opened on one field while the quota still spans two");
  guestIncrementsCounter(*core);
  check(cadence->gateOpen(), "the gate did not open on the second field with the retail quota");

  if (failures == 0) {
    std::printf("test_frame_cadence: all checks passed\n");
    return 0;
  }
  std::printf("test_frame_cadence: %d check(s) FAILED\n", failures);
  return 1;
}
