// game/ai/sway_schedule.cpp — SwaySchedule::advanceRateThenSway, guest 0x8012D27C (A00 = area 0).
//
// WHAT IT DOES, IN GAME TERMS
// A set of field objects in area 0 rocks back and forth on the spot, and the rocking WINDS DOWN as
// the area's scripted event sequence plays out. This is their per-frame driver, and it does two
// things in order:
//   1. SCHEDULE. Walk a six-step schedule (the node's own phase byte) that owns one number: how fast
//      this object rocks. It starts fast, holds until the sequence reaches event step 7, eases down
//      to a middle speed, holds again until event step 17, then eases down to a slow idle and stays
//      there. If the sequence is ALREADY over when the object spawns, it skips the whole ramp and
//      starts at the slow idle speed directly.
//   2. SWAY. Run this object's rocking motion for the frame — one of three variants picked by the
//      object's type byte.
// The two halves are joined by exactly one field, the node's rate at +0x50, which step 1 writes and
// step 2 consumes.
//
// HOW IT WAS IDENTIFIED
//   * ITS CALLER NAMES ITS ROLE. The only non-tabular reference to 0x8012D27C is the behaviour
//     handler FUN_8012D404, already owned as game/ai/beh_cull_tick_render.cpp, whose state 1 is
//     "cull; if NOT culled, FUN_8012D27C (per-type tick) then FUN_800517F8 (render)". So this runs
//     once per visible frame per object, with a0 = the node, between the cull and the render update.
//   * ITS CALLEES NAME THE FIELD IT WRITES. All three tail calls (0x8012CF4C, 0x8012D05C,
//     0x8012D16C) have the same measured shape, and it is unambiguous:
//         if (node16[+0x50] == 0) return;                       // rate 0 -> no motion at all
//         ... phase = node16[+0x4A];  v = Trig::rcos(phase);     // 0x80083F50, named in game/math
//         node16[+0x56] = node16[+0x5A] +- (v >> k);             // output = centre +- amplitude
//         node16[+0x4A] = (phase + node16[+0x50]) & 0xFFF;       // 4096 units = one full turn
//     That is a sinusoidal oscillator whose per-frame phase step IS +0x50, the one field this leaf
//     writes. The `== 0 -> return` early-out is what makes "rate", not "amplitude", the right
//     reading: zero here freezes the motion outright.
//   * THE THREE VARIANTS DIFFER ONLY IN AMPLITUDE AND DIRECTION, which is why the type byte only
//     picks between them and changes nothing else: 0x8012CF4C centres at (+0x56 + 1792) and adds
//     rcos>>3; 0x8012D05C centres at (+0x56 + 768) and adds rcos>>4; 0x8012D16C centres at
//     (+0x56 - 768) and SUBTRACTS. Each also re-rolls Rng::next() & 1 (0x8009A450, owned as
//     game/math/rng.h) whenever the phase lands exactly on 0, alternating between a single-speed and
//     a double-speed-half-amplitude style at every completed cycle.
//   * THE SCHEDULE'S CLOCK IS A PROGRESS COUNTER, NOT A MODE. 0x800BF9E0 has exactly one writer in
//     all of authenticated executable/overlay evidence — a `+1` at guest 0x80117084 (authenticated executable/overlay
//     evidence) on a scene completion path — and many readers comparing it against ASCENDING thresholds (6/16/28 in
//     beh_area_threshold_ptr_swap.cpp, 20 elsewhere, 7 and 17 here). One incrementer plus ordered
//     readers is a progress counter.
//   * WHAT I DID NOT ESTABLISH, and so did not put in the name: WHICH objects these are. The output
//     field +0x56 is what game/object/actor.h calls rotY, which would make this a yaw sway — but
//     that same lens calls +0x4A/+0x50 velY/accelY, and this family demonstrably uses them as an
//     angle and its step, so the 0x48..0x5A block is family-reused and I will not lean on the rotY
//     name. The name says what the function DOES to the oscillator, which is measured, not what the
//     object IS, which is not.
//
// TRUE EXTENT: [0x8012D27C, 0x8012D404). Established two ways, neither of them "the next gen
// function in the shard" (a false test on this project — the shard split is not address order):
//   * authenticated instruction extents reports 0x8012D27C as authenticated executable/overlay evidence,
//     82 live body lines, one epilogue;
//   * real overlay disassembly (A00.BIN, base
//     0x80108F9C) shows the single epilogue at 0x8012D3F4 `lw ra,0x10(sp)` / 0x8012D3FC `jr ra` /
//     0x8012D400 `addiu sp,sp,0x18` in the delay slot, with 0x8012D404 starting a fresh prologue
//     (`addiu sp,sp,-0x18`) — that address is FUN_8012D404, the caller, already owned.
// The jump table is real data, not a binary-boundary artifact: 0x80109D7C holds exactly the six words
// 8012D2B4 / 8012D314 / 8012D334 / 8012D350 / 8012D370 / 8012D39C, read straight out of A00.BIN at
// file offset 0xDE0. With the `phase < 6` guard above it, the switch's default arm is unreachable;
// it is kept because the guest's `jr $v0` is kept.
//
// MODULE: defined only by the A00 overlay (`overlay guest 0x8012D27C`, authenticated executable/overlay evidence), so
// it registers with A00 tomba::native::declareOverride. Using the main-module tomba::native::declareOverride would
// leave the direct `overlay guest 0x8012D27C(c)` call sites on the substrate.
//
// GUEST STACK: frame 24, one spill (ra at +16) — mirrored via GuestFrame, per "MIRROR THE GUEST
// STACK". Nothing is live in a callee-saved register across any of the three tail calls (the guest
// keeps the node in a0 and never touches it), so no GuestReg proxies are needed.
#include "sway_schedule.h"
#include "guest_call.h"

#include "core.h"
#include "core/overrides/guest_jal.h"
#include "core/overrides/native_override_catalog.h"
#include "guest_abi.h"

namespace {

// tools/binary ABI evidence 0x8012D27C --scaffold --guestabi
constexpr GuestFrameSpill kSpills_8012D27C[1] = {
    {31 /*ra*/, 16},
};

// The schedule's jump table — six words at 0x80109D7C, indexed by the node's phase byte. Named by
// what each phase body does; index order is the order below.
constexpr uint32_t kScheduleTable = 0x80109D7Cu;
constexpr uint32_t kSchedulePhases = 6;
constexpr uint32_t kPhaseSeed = 0x8012D2B4u;       // 0 — pick the starting rate for this run
constexpr uint32_t kPhaseHoldFast = 0x8012D314u;   // 1 — rock fast until the sequence reaches step 7
constexpr uint32_t kPhaseEaseMedium = 0x8012D334u; // 2 — ease the rate down to the middle speed
constexpr uint32_t kPhaseHoldMedium = 0x8012D350u; // 3 — hold there until the sequence reaches step 17
constexpr uint32_t kPhaseEaseIdle = 0x8012D370u;   // 4 — ease the rate down to the slow idle speed
constexpr uint32_t kPhaseSettled = 0x8012D39Cu;    // 5 — done; just keep rocking at the idle rate

// Phase index written directly by the seed step (it does not walk through the ramp).
constexpr uint8_t kPhaseIndexHoldFast = 1;
constexpr uint8_t kPhaseIndexHoldMedium = 3;
constexpr uint8_t kPhaseIndexSettled = 5;

// The three rates the schedule moves between, in oscillator phase units per frame (4096 = one full
// rock cycle, so 256 is a 16-frame cycle and 16 is a 256-frame one).
constexpr uint16_t kRateFast = 256;
constexpr uint16_t kRateMedium = 192;
constexpr uint16_t kRateIdle = 16;

// Ramp steps, and the guest's own `slti` thresholds. Comparing "< rate + 1" is how the guest says
// "keep easing while we are still above the target", so each ramp lands exactly on its target rate.
constexpr uint16_t kEaseMediumStep = 16;
constexpr int32_t kEaseMediumStop = kRateMedium + 1; // guest: slti 193
constexpr uint16_t kEaseIdleStep = 8;
constexpr int32_t kEaseIdleStop = kRateIdle + 1; // guest: slti 17

// Event-sequence gates. The seed step asks whether the sequence is already past the first slowdown;
// the two hold steps wait for the slowdown points themselves.
constexpr uint8_t kEventStepPastFirstSlowdown = 8; // guest: sltiu 8, i.e. "still below 8"
constexpr uint8_t kEventStepFirstSlowdown = 7;     // guest: sltiu 7
constexpr uint8_t kEventStepSecondSlowdown = 17;   // guest: sltiu 17

// Which sway body the node's type byte selects. 0 and 1 share the widest one.
constexpr uint8_t kVariantNarrow = 2;  // 0x8012D05C — centre +768,  half the amplitude
constexpr uint8_t kVariantReverse = 3; // 0x8012D16C — centre -768,  rocks the other way
constexpr uint8_t kVariantWideMax = 3; // guest: slti 3 — anything below this (and not 2) is wide

// Return addresses of the three tail calls. They are three separate call sites rather than one
// table-driven call because each carries its own RE'd `ra` constant, and the guest-stack bytes a
// callee spills depend on it.
constexpr uint32_t kRaAfterWideSway = 0x8012D3D4u;    // jal 0x8012cf4c
constexpr uint32_t kRaAfterNarrowSway = 0x8012D3E4u;  // jal 0x8012d05c
constexpr uint32_t kRaAfterReverseSway = 0x8012D3F4u; // jal 0x8012d16c

} // namespace

// ORACLE: overlay guest 0x8012D27C
void SwaySchedule::advanceRateThenSway(Core *c) {
  GuestFrame<24, 1> frame(c, kSpills_8012D27C);

  const SwayNode self{c, c->r[4]};
  const AreaSequence seq{c};

  // A phase byte past the end of the table means "no schedule work this frame" — go straight to the
  // sway. Only then is the table read, so the load can never run off the end of it.
  bool phaseComplete = false;
  const uint8_t phase = self.phase();
  if (phase < kSchedulePhases) {
    const uint32_t phaseBody = c->mem_r32(kScheduleTable + (uint32_t)phase * 4u);
    switch (phaseBody) {
    case kPhaseSeed:
      // Where this object joins the schedule depends on how far the sequence has already got.
      if (seq.isFinished()) {
        self.setPhase(kPhaseIndexSettled);
        self.setRate(kRateIdle);
      } else if (seq.eventStep() >= kEventStepPastFirstSlowdown) {
        self.setPhase(kPhaseIndexHoldMedium);
        self.setRate(kRateMedium);
      } else {
        self.setPhase(kPhaseIndexHoldFast);
        self.setRate(kRateFast);
      }
      break;

    case kPhaseHoldFast:
      phaseComplete = seq.eventStep() >= kEventStepFirstSlowdown;
      break;

    case kPhaseEaseMedium:
      if (self.rate() < kEaseMediumStop) {
        phaseComplete = true;
      } else {
        self.setRate((uint16_t)(self.rate_u() - kEaseMediumStep));
      }
      break;

    case kPhaseHoldMedium:
      phaseComplete = seq.eventStep() >= kEventStepSecondSlowdown;
      break;

    case kPhaseEaseIdle:
      if (self.rate() < kEaseIdleStop) {
        phaseComplete = true;
      } else {
        self.setRate((uint16_t)(self.rate_u() - kEaseIdleStep));
      }
      break;

    case kPhaseSettled:
      break;

    // Unreachable: the table holds exactly the six entries above (verified in A00.BIN) and the
    // phase byte is gated to 0..5. Kept because the guest's `jr $v0` is kept — with no `ra` to
    // set, because a jump is not a call, and on ONE line because that is the construct
    // tools/dynamic differential evidence excludes from the compared call sequence on both sides.
    default:
      psx::cpu::dispatchGuestToReturn0(*c, phaseBody, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
      return;
    }
    if (phaseComplete) {
      self.setPhase((uint8_t)(self.phase() + 1));
    }
  }

  // Now the motion itself. a0 still holds the node — the schedule above never touches it — but set
  // it explicitly so the argument is visible at the call rather than inherited.
  const uint8_t variant = self.variant();
  if (variant < kVariantWideMax && variant != kVariantNarrow) {
    c->r[4] = self.mAt;
    tomba::guest::dispatchJalToReturn(*c, 0x8012CF4Cu, kRaAfterWideSway);
  } else if (variant == kVariantNarrow) {
    c->r[4] = self.mAt;
    tomba::guest::dispatchJalToReturn(*c, 0x8012D05Cu, kRaAfterNarrowSway);
  } else if (variant == kVariantReverse) {
    c->r[4] = self.mAt;
    tomba::guest::dispatchJalToReturn(*c, 0x8012D16Cu, kRaAfterReverseSway);
  }
}

void SwaySchedule::registerOverrides() {
  // A00, from the image: of 23 authenticated MODE images only A00 holds a function at 0x8012D27C
  // (98 words), and the address is inside A00's loaded text range. This is the MODULE note above,
  // applied.
  tomba::native::declareOverlayOverride(
      "A00", 0x8012D27Cu, "SwaySchedule::advanceRateThenSway", &SwaySchedule::advanceRateThenSway);
}
