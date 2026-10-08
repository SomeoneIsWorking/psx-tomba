// game/scene/script_interp.cpp — PC-native cutscene SCRIPT INTERPRETER.
//
// RE'd from the four resident MAIN.EXE fns 0x80040CDC / 0x80040DE0 / 0x80040E54 / 0x80041098 plus
// the op-table entry at 0x800412CC. See docs/findings/scene.md "Cutscene SCRIPT INTERPRETER" for
// the full RE.
//
// TOP-DOWN DOCTRINE — the leaves it does not own (56 of the 63-entry handler table at 0x800A3B78)
// stay reachable through address dispatch, so a guest body a script reaches keeps running as the
// guest body. The dispatch loop is owned here, and so is op 0x3E, which routes through the
// behaviour dispatcher: a script-driven fade registered as a native `beh_*` runs native
// automatically and an unregistered one falls through to the guest leaf.
//
// WHAT IS OWNED: the dispatch loop, the entry advance, the advance sub-machine, the six verified
// opcode handlers (op04/op05/op06/op31/op34/op36), and five resident leaves that had no prior home.
// The other 56 opcode handlers are still the guest's own bodies.
#include "scene/script_interp.h"
#include "cfg.h"
#include "core.h"
#include "core/engine/engine.h"
#include "core/entry/game_ctx.h"
#include "core/overrides/native_override_catalog.h" // tomba::native::declareOverride — the one native-override registry
#include "execution_services.h"
#include "game.h"
#include "guest_abi.h" // GuestFrame — mirror the guest stack frame (CLAUDE.md)
#include "guest_call.h"
#include "object/behavior_dispatch.h"
#include "scene/scene_flags.h"
#include "scene/script_globals.h"
#include "scene/script_object.h"
#include "scene/script_opcode.h"
#include <cstdint>
#include <cstdio>

// Everything the bodies below reach for is owned by exactly one of three headers:
//   script_object.h   — the driven object's block, as typed accessors (ScriptObject).
//   script_opcode.h   — the bytecode vocabulary: opcode ids, entry flags, ret codes, advance
//                       kinds, and the guest return addresses each arm arms.
//   script_globals.h  — the fixed scratchpad / resident addresses that are not object fields.
// A reader who wants to know what `0x70` or `0x2000` or `0x1F800214` is now has one place to look,
// per kind, instead of a flat constant list two hundred lines above its first use.
namespace {
using tomba::scene::ScriptObject;
using tomba::scene::script_globals::kClaimGate;
using tomba::scene::script_globals::kEventHandledMask;
using tomba::scene::script_globals::kLookAnchorX;
using tomba::scene::script_globals::kLookAnchorZ;
using tomba::scene::script_globals::kSecondaryActor;
using tomba::scene::script_globals::kStatusByteMirror;
using tomba::scene::script_globals::kStatusByteSource;

using tomba::script::AdvanceKind;
using tomba::script::AdvanceStepIndex;
using tomba::script::ScriptOpcode;
using tomba::script::StepReturn;

// op04's two phases on the object's phase byte (the same slot op34 uses as its claim/poll counter).
constexpr uint8_t kRendezvousPost = 0u;  // post postValue into the slot, then pause
constexpr uint8_t kRendezvousAwait = 1u; // poll the slot until it reads awaitValue

// The progress byte value the advance sub-machine's two GATED arms test. Only this exact value
// keeps a script that lacked the valid-entry bit running; anything else stops it.
constexpr int8_t kProgressGateValue = 2;

} // namespace

// The entry-ADVANCE decode's cursor-move kinds, the advance sub-machine's switch indices and the
// guest return addresses each arm arms all live in scene/script_opcode.h, with the rest of the
// bytecode vocabulary. The scene-flag array's base and its signed index rule live in
// scene/scene_flags.h. The claim gate's address — wrong ONCE in the wide RE, 0x60 low because an
// unrelated table's address was copied — lives in scene/script_globals.h with the recomputation
// that fixed it. Nothing in this file spells a guest field as hex any more.

// op3E — the "call fnptr" trampoline, guest 0x800412CC. It composes the 32-bit function pointer
// from the entry's argB/argC halfwords and routes it through the behaviour dispatcher, so any fnptr
// registered as a native behaviour runs native and an unregistered one falls through to the guest
// body. The script-driven cutscene fade family rides this.
//
// GUEST FRAME MIRROR, and why: the guest trampoline is NOT frameless — it descends sp by 24, spills
// ra@+16, loads the pointer, arms r31 and jalrs. Skipping that frame ran every op3E callee 24 bytes
// high on the guest stack, so the callees' own spills landed at shifted addresses (watch-cut 735).
int ScriptInterp::callFnptr(uint32_t obj) {
  Core *c = core;
  const ScriptObject so{c, obj};
  static constexpr GuestFrameSpill kSpills[] = {{31, 16}};
  GuestFrame<24, 1> frame(c, kSpills);
  // The ret code is EXACTLY what the callee left in v0. Address dispatch sets v0 to the guest
  // body's own return, so the substrate path is transparent; a native `beh_*` handler is
  // void-returning, so a native fade MUST set `c->r[2]` before returning, exactly as the guest
  // instruction path would have. Every fade in game/ai/beh_a06_fade_*.cpp follows that convention.
  c->r[31] = tomba::script::kJalReturnFromCallFnptr; // the trampoline's own post-jalr constant
  eng(c).behaviors.dispatchObj(obj, so.functionPointer());
  return (int)c->r[2];
}

// C-ABI wrapper for the behaviour-dispatch table. It takes the object from a0 (c->r[4]), matching
// the guest instruction path's calling convention, so a native ancestor that already used
// dispatchObj lands here transparently. Deliberately trivial: read a0, call step().
static constexpr GuestFrameSpill kSpills_80041098[5] = {
    {17, 20},
    {19, 28},
    {18, 24},
    {31 /*ra*/, 32},
    {16, 16},
}; // frame=40, abi_extract --scaffold --guestabi
void beh_script_interp_step(Core *c) {
  GuestFrame<40, 5> frame(c, kSpills_80041098);
  eng(c).script.step(c->r[4]);
}

// The DISPATCH LOOP — FUN_80041098. Read the cursor's opcode, run its handler, act on what the
// handler returned, advance the cursor, repeat. Runs while the object's progress byte is POSITIVE.
//
// GUEST FRAME MIRROR, and why the register lifetimes matter: the guest descends sp by 40, spills the
// incoming s0-s3/ra at +16/+20/+24/+28/+32 BEFORE stepping, and holds r17=obj, r18=1 and
// r19=handler-table live across the loop. Every substrate handler — and its callees, e.g.
// FUN_80041718 -> FUN_80077C40 — spills relative to THIS sp with THESE register values, so without
// the mirror this core's downstream stack bytes land 40 high carrying stale register contents
// (side-by-side watch-cut 153, Charles' anim-install chain, 2026-07-10). `ra` is armed per call
// site with the guest's own return constants.
void ScriptInterp::step(uint32_t obj) {
  Core *c = core;
  const ScriptObject so{c, obj};
  static constexpr GuestFrameSpill kSpills[] = {{17, 20}, {19, 28}, {18, 24}, {31, 32}, {16, 16}};
  GuestFrame<40, 5> frame(c, kSpills);
  c->r[17] = obj;
  c->r[19] = kHandlerTableBase;
  c->r[18] = 1u;

  for (;;) {
    const int8_t prog = so.progress();
    c->r[16] = 0u; // the guest's delay slot at the loop-top test
    if (prog <= 0) {
      c->r[2] = 0u;
      return;
    } // the guest's exit path leaves v0 = 0
    const uint32_t scriptPtr = so.cursor();
    const uint16_t opWord = c->mem_r16(scriptPtr + 0);
    const uint32_t opcodeId = static_cast<uint32_t>(opWord & tomba::script::kOpcodeIdMask);

    uint32_t ret;
    if (opcodeId == static_cast<uint32_t>(ScriptOpcode::kCallFunctionPointer)) {
      // The "call fnptr" mechanism the script-driven cutscene fade family rides: any fade
      // registered in the behaviour table runs native, transparently. The guest does NOT
      // special-case this opcode — its handler table maps 0x3E to the 0x800412CC trampoline,
      // reached by the same jalr return constant as every other opcode. `callFnptr` mirrors that
      // trampoline's real 24-byte frame and arms the callee's own return address.
      c->r[31] = tomba::script::kJalReturnFromHandler;
      ret = static_cast<uint32_t>(callFnptr(obj));
    } else {
      // Every OTHER opcode: reach its guest body through the resident handler table. Guest ABI:
      // a0 = obj, result in v0, and the handler runs with `ra` armed to the loop's own return
      // constant — handlers and their callees spill it.
      const uint32_t handler = c->mem_r32(kHandlerTableBase + opcodeId * 4u);
      c->r[4] = obj;
      c->r[31] = tomba::script::kJalReturnFromHandler;
      psx::cpu::dispatchGuestToReturn0(*c, handler, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
      ret = c->r[2];
    }

    // PSXPORT_DEBUG=script — one line per opcode dispatch, naming the opcode as well as its word
    // so a log can be read without a second pass over the handler table. Both execution paths run
    // this same native loop for natively-dispatched parents, so the two logs diff directly.
    cfg_logf("script",
             "obj=%08X ptr=%08X op=%04X (%s) ret=%u",
             obj,
             scriptPtr,
             opWord,
             tomba::script::scriptOpcodeName(opWord),
             ret);

    // What the handler's return DOES to guest state. The guest calls the advance with a kind byte
    // chosen here, and then continues ONLY if the advance returns exactly 1; any other advance
    // return leaves the loop with v0 = 0.
    uint32_t advanceKindArg;
    switch (static_cast<StepReturn>(ret)) {
    case StepReturn::kPause: {
      so.pauseRequested();
      c->r[2] = 1u; // the guest's pause exit leaves v0 = 1
      return;
    }
    case StepReturn::kAdvanceKind0:
    case StepReturn::kAdvanceKind0Again:
      advanceKindArg = 0u;
      break;
    case StepReturn::kAdvanceKind1:
      advanceKindArg = 1u;
      break;
    default:
      // 4 and above: the guest falls through to the loop test with the compare register still 0,
      // so it exits with v0 = 0.
      c->r[2] = 0u;
      return;
    }
    c->r[31] = tomba::script::kJalReturnFromAdvance; // the guest's return constant at that call site
    const int advanceRet = advanceEntry(obj, advanceKindArg);
    c->r[16] = static_cast<uint32_t>(advanceRet); // s0 = the advance's return, live across the test
    if (advanceRet != 1) {
      c->r[2] = 0u;
      return;
    }
  }
}

// =================================================================================================
// The MOVEMENT opcodes — op36, op31, and the angle stepper and event gate they share.
//
// Every body below is instruction-exact against authenticated executable/overlay evidence, and each
// one carries its own note where a transcription was wrong and what corrected it. Both frames are
// guest-stack-mirrored per CLAUDE.md's "MIRROR THE GUEST STACK", so the eventual wiring's
// side-by-side gate has nothing left to fix on the stack-fidelity front.
// =================================================================================================

// The ANGLE domain, in Q12 over a 4096-unit circle. These three constants are the whole vocabulary
// every turning opcode shares, and they used to be spelled as bare hex at four separate sites.
constexpr uint32_t kAngleMask = 0xFFFu;       // one full turn
constexpr uint32_t kHalfTurn = 0x800u;        // two quarters: the short-way-round decision boundary
constexpr uint32_t kTurnStepDefault = 0x100u; // the step op36 hands its own turn

// FUN_8004139C — the leaf angle-stepper (no guest frame). Nudges the 16-bit angle at `anglePtr`
// toward `targetAngle` by up to `step`, taking whichever direction is SHORT around the circle, and
// snaps exactly onto the target when the remainder is within one step. Returns 1 iff it snapped
// exactly this call, which is what op36 and op31 each test to decide whether to advance.
int ScriptInterp::stepAngleToward(uint32_t anglePtr, int16_t targetAngle, int16_t step) {
  Core *c = core;
  const int16_t cur = static_cast<int16_t>(c->mem_r16(anglePtr));
  const int32_t straightDelta = static_cast<int32_t>(targetAngle) - static_cast<int32_t>(cur);
  int32_t magnitude = straightDelta < 0 ? -straightDelta : straightDelta;
  if (magnitude <= static_cast<int32_t>(step)) {
    c->mem_w16(anglePtr, static_cast<uint16_t>(targetAngle));
    return 1;
  }
  // A masked delta below the half turn means the short way round is FORWARD; above it, backward.
  const int16_t dirStep =
      ((static_cast<uint32_t>(straightDelta) & kAngleMask) > kHalfTurn - 1u) ? static_cast<int16_t>(-step) : step;
  const int16_t moved = static_cast<int16_t>(cur + dirStep);
  c->mem_w16(anglePtr, static_cast<uint16_t>(moved));
  const int32_t remaining = static_cast<int32_t>(
      static_cast<uint32_t>(static_cast<int32_t>(targetAngle) - static_cast<int32_t>(moved)) & kAngleMask);
  if (remaining <= static_cast<int32_t>(step)) {
    c->mem_w16(anglePtr, static_cast<uint16_t>(targetAngle));
    return 1;
  }
  return 0;
}

// FUN_80041438 — "turn this actor's facing toward an angle". The actor here is whichever one is
// being turned, NOT always the script-driven object: op31 calls it on the actor it resolved (self or
// the global secondary), op36 calls it on itself.
int ScriptInterp::turnFacing(uint32_t obj, int16_t targetAngle, int16_t step) {
  return stepAngleToward(obj + ScriptObject::kFacingAngle, targetAngle, step);
}

// Guest-ABI twin of turnFacing, mirroring the guest's own 24-byte frame NESTED on whatever the
// caller (op36/op31) has already descended c->r[29] to. Arguments arrive in a0..a2, result in v0.
void ScriptInterp::turnFacingFramed() {
  Core *c = core;
  const uint32_t obj = c->r[4];
  const int16_t targetAngle = static_cast<int16_t>(c->r[5]);
  const int16_t step = static_cast<int16_t>(c->r[6]);
  static constexpr GuestFrameSpill kSpills[] = {{31, 16}};
  GuestFrame<24, 1> frame(c, kSpills);
  c->r[2] = static_cast<uint32_t>(turnFacing(obj, targetAngle, step));
}

// op36's per-move EVENT gate — FUN_80042EA4. `flagsPtr` points at a 16-bit flags word, and the
// packed argument carries (low byte = sound id, high byte = pitch bend) for `Sfx::trigger`.
// Three distinct gates, and which one applies is decided by the flags word's OWN bits:
//   low six bits nonzero — a per-frame repeat pulse. It fires whenever the shared "already handled
//     this frame" mask carries none of the flags word's bits, so a sound that is already playing
//     suppresses itself.
//   low six bits clear, bit 0x80 set — an ARM-ONCE latch. It is gated on the owner's own flag byte,
//     sets bit 0x40 as the latch, and fires the sound exactly once on the 0 -> 1 edge. A set bit
//     0x40 means the consumer has not cleared it yet, so it waits rather than re-firing.
//   low six bits clear, bit 0x80 clear — a no-op.
//
// Returns 1 only on the frame the arm-once latch fires; the repeat arm always returns 0.
int ScriptInterp::stepEventPulse(uint32_t obj, uint32_t flagsPtr, uint32_t packedArg) {
  Core *c = core;
  const ScriptObject so{c, obj};
  const uint16_t flagsWord = c->mem_r16(flagsPtr);
  const uint8_t soundId = static_cast<uint8_t>(packedArg & 0xFFu);
  const int8_t pitchBend = static_cast<int8_t>((packedArg >> 8) & 0xFFu);
  constexpr uint16_t kRepeatMask = 0x3Fu;
  constexpr uint16_t kArmOnceBit = 0x80u;
  constexpr uint16_t kLatchedBit = 0x40u;
  if (flagsWord == 0) {
    return 0;
  }
  if ((flagsWord & kRepeatMask) == 0) {
    if ((flagsWord & kArmOnceBit) == 0) {
      return 0;
    }
    if (c->mem_r8(so.ownerPointer() + 4u) == 0) {
      c->mem_w16(flagsPtr, static_cast<uint16_t>(flagsWord & ~kLatchedBit));
      return 0;
    }
    if ((flagsWord & kLatchedBit) != 0) {
      return 0; // already latched — wait for the consumer to clear it
    }
    c->mem_w16(flagsPtr, static_cast<uint16_t>(flagsWord | kLatchedBit));
    eng(c).sfx.trigger(soundId, 0, pitchBend);
    return 1;
  }
  // low 6 bits nonzero: fire every call the shared scratchpad mask has none of flagsWord's bits set.
  if ((c->mem_r16(kEventHandledMask) & flagsWord) == 0) {
    eng(c).sfx.trigger(soundId, 0, pitchBend);
  }
  return 0;
}

// Guest-ABI twin of stepEventPulse, mirroring the guest's own 24-byte frame. Arguments from a0..a2.
void ScriptInterp::stepEventPulseFramed() {
  Core *c = core;
  const uint32_t obj = c->r[4];
  const uint32_t flagsPtr = c->r[5];
  const uint32_t packedArg = c->r[6];
  static constexpr GuestFrameSpill kSpills[] = {{31, 16}};
  GuestFrame<24, 1> frame(c, kSpills);
  c->r[2] = static_cast<uint32_t>(stepEventPulse(obj, flagsPtr, packedArg));
}

namespace {
// ---- the op36 movement vocabulary ------------------------------------------------------------------
// The two retained guest leaves op36 calls, and the two arithmetic conventions its move uses.
constexpr uint32_t kGuestIsqrt = 0x80084080u;  // the GTE-LZCS square-root leaf (still substrate)
constexpr uint32_t kGuestRatan2 = 0x80085690u; // Trig::ratan2, wired as an override
constexpr int32_t kWholeMoveQ12 = 0x1000;      // stepsRemaining starts at "the whole move" (Q12)
constexpr int32_t kQ12RoundUp = 0xFFF;         // the guest's round-toward-zero nudge for a negative
constexpr int32_t kDefaultStepCount = 10;      // requestedStepCount == 0 means "ten steps"
constexpr int32_t kTurnModeSnapAndStop = 0;    // turn-mode 0: snap the facing, then stop after phase 1
constexpr int32_t kTurnModeSnapAndTurn = 1;    // turn-mode 1: snap the facing, then keep turning
// The entry's turn-mode flag lives in the extended block at entry+10, i.e. halfword 1. op36 reads it
// from the SCRIPT each call and later overwrites the object's copy of the same halfword with the
// second turn angle — the guest really does use one field for two things, so the two are named apart.
constexpr uint32_t kTurnModeOffset = 10u;

// op36's solve of the per-call step size, from the straight-line distance and the requested step
// count. The two divisions are the guest's own and both are wrapped in its bounds checks, because
// `hi`/`lo` and the break are guest-VISIBLE state. Neither check can fire at this call site — the
// dividend is 16-bit-sourced in the first and the literal 0x1000 in the second, so neither can be
// INT32_MIN — and they are kept anyway: a body that reproduces the guest's arithmetic exactly is
// what makes the surrounding code readable, and dropping a check that cannot fire is a silent
// behaviour change to anyone who later widens the dividend.
uint16_t solveStepDivisor(Core *c, int16_t distance, int32_t stepsRequested) {
  cpu_div(c, static_cast<uint32_t>(static_cast<int32_t>(distance)), static_cast<uint32_t>(stepsRequested));
  if (stepsRequested == 0) {
    psx::cpu::handleBreak(*c, 7168u);
  }
  if (stepsRequested == -1 && static_cast<int32_t>(distance) == static_cast<int32_t>(0x80000000)) {
    psx::cpu::handleBreak(*c, 6144u);
  }
  uint16_t stepDiv = static_cast<uint16_t>(c->lo);
  if (stepDiv == 0) {
    stepDiv = 1; // the guest re-tests the truncated quotient and clamps it to 1
  }
  return stepDiv;
}

// The second division: the whole move in Q12 over the per-call step, giving the per-call decrement
// of stepsRemaining. Same bounds checks, same reasoning about them being unreachable.
uint16_t solvePerCallStep(Core *c, uint16_t stepDiv) {
  cpu_div(c,
          static_cast<uint32_t>(kWholeMoveQ12),
          static_cast<uint32_t>(static_cast<int32_t>(static_cast<int16_t>(stepDiv))));
  if (static_cast<int16_t>(stepDiv) == 0) {
    psx::cpu::handleBreak(*c, 7168u);
  }
  if (static_cast<int16_t>(stepDiv) == -1 && kWholeMoveQ12 == static_cast<int32_t>(0x80000000)) {
    psx::cpu::handleBreak(*c, 6144u);
  }
  return static_cast<uint16_t>(c->lo);
}

// The Q12 interpolation the move commits each frame: remaining-arc times delta, shifted back down,
// with the guest's round-toward-zero nudge on a negative product. `from` is the target coordinate
// read out of the entry's arguments, which is where the guest reads it from rather than from the
// stored delta.
int16_t interpolateQ12(int16_t remaining, int16_t delta, int16_t from) {
  int32_t scaled = static_cast<int32_t>(remaining) * static_cast<int32_t>(delta);
  if (scaled < 0) {
    scaled += kQ12RoundUp;
  }
  return static_cast<int16_t>(from - static_cast<int16_t>(scaled >> 12));
}
} // namespace

// op36 — FUN_80043108. "Walk this actor toward the target position the entry spells out." The
// entry's argA/argB/argC ARE the target Z/Y/X — not a pointer to another object, whatever the
// original mapped-only pass guessed. Three phases on the object's phase byte:
//   0 — solve the move: the straight-line distance, the per-call step size, and the turn angle(s).
//   1 — turn self toward the second angle until it snaps.
//   2 — interpolate self's position one step toward the target, pulsing the movement event.
//
// The `goto commit` and the phase tests below preserve the guest's own branch-delay-slot control
// flow EXACTLY: a MIPS instruction after a compare executes unconditionally before the branch
// decision takes effect, and "cleaning that up" changes which arm runs.
//
// RE note kept because it changed the answer once: a Ghidra decompile of this body placed a THIRD
// div-by-zero trap right after the first division's clamp. There is no such trap in the guest's own
// code at that point — the decompiler had moved the SECOND division's trap up into that slot. The
// guest has no trap between the clamp and the second division, which is what `solvePerCallStep`
// below reproduces.
int ScriptInterp::op36MoveTowardScriptTarget(uint32_t obj) {
  Core *c = core;
  const ScriptObject so{c, obj};
  // The guest's 40-byte frame: s0@+16, s1@+20, s2@+24, s3@+28, s4@+32, ra@+36, all LIVE values.
  // s1 in particular survives the square-root call, so it has to be a register and not a local.
  static constexpr GuestFrameSpill kSpills[] = {{16, 16}, {17, 20}, {18, 24}, {19, 28}, {20, 32}, {31, 36}};
  GuestFrame<40, 6> frame(c, kSpills);
  c->r[16] = obj; // s0 = obj
  c->r[20] = 1u;  // s4 = a constant 1 the body reuses

  const uint32_t scriptPtr = so.cursor();
  c->r[18] = scriptPtr; // s2 — the CURRENT entry; +2/+4/+6 are its target Z/Y/X
  const uint8_t phase = so.phase();
  const int16_t turnMode = static_cast<int16_t>(c->mem_r16(scriptPtr + kTurnModeOffset)); // s3
  c->r[19] = static_cast<uint32_t>(static_cast<uint16_t>(turnMode));

  if (phase != 1) {
    if (phase > 1) {
      if (phase != 2) {
        return 0; // dead per the guest's byte shape, kept for parity
      }
      goto commit; // the guest's direct jump to its interpolation tail
    }
    if (phase != 0) {
      return 0; // dead: the phase is guaranteed 0 in this arm
    }

    // ---- phase 0: solve the move. ---------------------------------------------------------------
    {
      int32_t stepsRequested = static_cast<int32_t>(so.requestedStepCount());
      if (stepsRequested == 0) {
        stepsRequested = kDefaultStepCount;
      }
      c->r[17] = static_cast<uint32_t>(stepsRequested); // s1 — live across the square-root call

      const int16_t targetZ = static_cast<int16_t>(so.argA());
      const int16_t targetY = static_cast<int16_t>(so.argB());
      const int16_t targetX = static_cast<int16_t>(so.argC());
      const int16_t dz = static_cast<int16_t>(targetZ - so.positionZ());
      const int16_t dy = static_cast<int16_t>(targetY - so.positionY());
      const int16_t dx = static_cast<int16_t>(targetX - so.positionX());
      so.setDeltas(dz, dy, dx);

      // The straight-line distance on the X/Z plane, via the still-substrate GTE square-root leaf.
      // This is NOT Math::isqrt16: the guest's own leaf is what sets the GTE result registers.
      c->r[4] = static_cast<uint32_t>(static_cast<int32_t>(dx * dx + dz * dz));
      psx::cpu::dispatchGuestToReturn0(*c, kGuestIsqrt, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
      const int16_t distance = static_cast<int16_t>(c->r[2]);

      uint16_t stepDiv = solveStepDivisor(c, distance, stepsRequested);
      so.setStepDivisor(stepDiv);
      so.setStepsRemaining(static_cast<uint16_t>(kWholeMoveQ12));
      stepDiv = solvePerCallStep(c, stepDiv);
      so.setStepDivisor(stepDiv); // the per-call step size overwrites the same field again

      // The turn mode decides whether the actor's OWN facing is written before the shared tail runs.
      if (turnMode == -1) {
        c->r[4] = static_cast<uint32_t>(static_cast<int32_t>(-(static_cast<int32_t>(dx))));
        c->r[5] = static_cast<uint32_t>(static_cast<int32_t>(dz));
        psx::cpu::dispatchGuestToReturn0(*c, kGuestRatan2, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
        so.setFacingAngle(static_cast<int16_t>(c->r[2] & kAngleMask));
      } else if (turnMode == kTurnModeSnapAndStop) {
        so.setPhase(2);
        return 0;
      }
      // turnMode == -1 falls through from the arm above; a turn mode outside {-1, 0} skips the
      // actor's own facing write but still reaches this SECOND ratan2 call, which is the guest's
      // own fallthrough shape and not an oversight.
      c->r[4] = static_cast<uint32_t>(static_cast<int32_t>(-(static_cast<int32_t>(dx))));
      c->r[5] = static_cast<uint32_t>(static_cast<int32_t>(dz));
      psx::cpu::dispatchGuestToReturn0(*c, kGuestRatan2, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
      so.setTurnAngle2(static_cast<int16_t>(c->r[2] & kAngleMask));
      so.advancePhase();
    }
  }

  // ---- phase 0's fallthrough, and phase 1's entry: turn self toward the second angle. -------------
  {
    c->r[4] = obj;
    c->r[5] = static_cast<uint32_t>(static_cast<int32_t>(so.turnAngle2()));
    c->r[6] = kTurnStepDefault;
    turnFacingFramed();
    if (c->r[2] != 0) {
      so.advancePhase();
    }
    if (turnMode == kTurnModeSnapAndTurn) {
      return 0;
    }
  }

commit:
  // ---- the interpolation tail: one step of the remaining arc, then the movement event. -----------
  {
    uint16_t stepsRemaining = static_cast<uint16_t>(so.stepsRemaining() - so.stepDivisor());
    if (static_cast<int16_t>(stepsRemaining) <= 0) {
      stepsRemaining = 0;
    }
    so.setStepsRemaining(stepsRemaining);

    // The cursor is re-read fresh here, per the guest's own access pattern — the target coordinate
    // is read from the ENTRY, not from the delta that was stored at phase 0.
    const uint32_t entry = so.cursor();
    so.setPositionZ(interpolateQ12(
        static_cast<int16_t>(stepsRemaining), so.deltaZ(), static_cast<int16_t>(c->mem_r16(entry + 2u))));
    so.setPositionY(interpolateQ12(
        static_cast<int16_t>(stepsRemaining), so.deltaY(), static_cast<int16_t>(c->mem_r16(entry + 4u))));
    so.setPositionX(interpolateQ12(
        static_cast<int16_t>(stepsRemaining), so.deltaX(), static_cast<int16_t>(c->mem_r16(entry + 6u))));

    if (stepsRemaining != 0) {
      c->r[4] = obj;
      c->r[5] = so.eventFlagsPointer();
      c->r[6] = static_cast<uint32_t>(static_cast<int32_t>(so.eventPackedArg()));
      stepEventPulseFramed();
      return 0;
    }
    return 1;
  }
}

// op31 — FUN_80041468. "Turn an actor toward a computed angle." The acted-upon actor is SELF when
// argA's sign bit is CLEAR, and the single global secondary-actor slot when it is SET — not a
// per-entry table, which is what the original mapped-only pass guessed.
//
// The actor selection was mislabelled once, in this repository's own favour: a first hand-trace read
// "sign set -> self", the opposite of the truth. It was caught by deriving it a second time from the
// guest's delay-slot ordering AND cross-checking Ghidra's decompile — both agree that the sign bit
// SET selects the global and CLEAR selects self. The corrected reading is what ships.
//
// Five turn modes, selected by argA's low 15 bits, each reading a different position pair and each
// writing the target angle into argC's field:
//   0 — use the angle already sitting there; no arithmetic at all.
//   1 — aim from the scratchpad look anchors at the actor.
//   2 — the same aim, rotated a half turn (2048) away.
//   3 — aim from SELF at the actor.
//   10 — continuously re-aim, and reset argB to a fixed step: a "track a live point" mode.
// Anything else is a no-op that falls straight to the commit tail, which is the guest's own
// unconditional fallthrough for every other value and not an oversight.
int ScriptInterp::op31TurnTowardTarget(uint32_t obj) {
  Core *c = core;
  const ScriptObject so{c, obj};
  // The guest's 48-byte frame: s0@+32, s1@+36, ra@+40, all LIVE values.
  static constexpr GuestFrameSpill kSpills[] = {{16, 32}, {17, 36}, {31, 40}};
  GuestFrame<48, 3> frame(c, kSpills);
  c->r[16] = obj; // s0 = obj

  const int16_t argA = static_cast<int16_t>(so.argA());
  const uint32_t target = ((static_cast<uint16_t>(argA) & 0x8000u) != 0u) ? c->mem_r32(kSecondaryActor) : obj;
  c->r[17] = target; // s1 = the actor actually being turned

  const uint8_t phase = so.phase();
  if (phase != 0) {
    if (phase != 1) {
      return 0;
    }
    // ---- phase 1: step the facing toward the angle phase 0 already computed. ---------------------
    c->r[4] = target;
    c->r[5] = static_cast<uint32_t>(static_cast<int32_t>(so.argC())); // the target angle
    c->r[6] = static_cast<uint32_t>(static_cast<int32_t>(so.argB())); // the turn threshold
    turnFacingFramed();
    return static_cast<int>(c->r[2]);
  }

  // ---- phase 0: compute a fresh target angle into argC's field under the mode switch. -------------
  {
    const ScriptObject targetSo{c, target};
    const uint16_t mode = static_cast<uint16_t>(argA) & 0x7FFFu;
    constexpr uint16_t kModeUseCurrent = 0;
    constexpr uint16_t kModeAimAtActorFromAnchor = 1;
    constexpr uint16_t kModeAimAtActorFromAnchorFlipped = 2;
    constexpr uint16_t kModeAimFromSelf = 3;
    constexpr uint16_t kModeTrackLivePoint = 10;
    constexpr int32_t kHalfTurnAngle = 2048; // half of a 4096-unit circle

    if (mode == kModeAimAtActorFromAnchorFlipped) {
      const int32_t y = static_cast<int32_t>(targetSo.positionX()) - static_cast<int32_t>(c->mem_r16s(kLookAnchorX));
      const int32_t x = static_cast<int32_t>(c->mem_r16s(kLookAnchorZ)) - static_cast<int32_t>(targetSo.positionZ());
      c->r[4] = static_cast<uint32_t>(y);
      c->r[5] = static_cast<uint32_t>(x);
      psx::cpu::dispatchGuestToReturn0(*c, kGuestRatan2, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
      const int32_t flipped = static_cast<int32_t>(c->r[2]) - kHalfTurnAngle;
      so.setArgC(static_cast<uint16_t>(so.argC() + (static_cast<uint32_t>(flipped) & kAngleMask)));
    } else if (mode < kModeAimFromSelf) {
      if (mode == kModeAimAtActorFromAnchor) {
        // The same aim as the mode above, WITHOUT the half-turn.
        const int32_t y = static_cast<int32_t>(targetSo.positionX()) - static_cast<int32_t>(c->mem_r16s(kLookAnchorX));
        const int32_t x = static_cast<int32_t>(c->mem_r16s(kLookAnchorZ)) - static_cast<int32_t>(targetSo.positionZ());
        c->r[4] = static_cast<uint32_t>(y);
        c->r[5] = static_cast<uint32_t>(x);
        psx::cpu::dispatchGuestToReturn0(*c, kGuestRatan2, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
        so.setArgC(static_cast<uint16_t>(so.argC() + (c->r[2] & kAngleMask)));
      }
      // mode 0 writes nothing — it keeps the angle already in argC's field.
    } else if (mode == kModeAimFromSelf) {
      const int32_t y = static_cast<int32_t>(targetSo.positionX()) - static_cast<int32_t>(so.positionX());
      const int32_t x = static_cast<int32_t>(so.positionZ()) - static_cast<int32_t>(targetSo.positionZ());
      c->r[4] = static_cast<uint32_t>(y);
      c->r[5] = static_cast<uint32_t>(x);
      psx::cpu::dispatchGuestToReturn0(*c, kGuestRatan2, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
      so.setArgC(static_cast<uint16_t>(so.argC() + (c->r[2] & kAngleMask)));
    } else if (mode == kModeTrackLivePoint) {
      // The aim point MOVES: it is read from argC/argB themselves, which a previous call left set.
      // So the result is an OVERWRITE rather than an accumulate, and argB is reset to a fixed step.
      const int32_t y = static_cast<int32_t>(targetSo.positionX()) - static_cast<int32_t>(so.argC());
      const int32_t x = static_cast<int32_t>(so.argB()) - static_cast<int32_t>(targetSo.positionZ());
      c->r[4] = static_cast<uint32_t>(y);
      c->r[5] = static_cast<uint32_t>(x);
      psx::cpu::dispatchGuestToReturn0(*c, kGuestRatan2, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
      so.setArgC(static_cast<uint16_t>(c->r[2] & kAngleMask));
      so.setArgB(static_cast<uint16_t>(kTurnStepDefault));
    }
  }

  // ---- the commit tail: snap the acted-upon actor's facing, or arm another turn. ------------------
  {
    const int16_t targetAngle = static_cast<int16_t>(so.argC());
    const ScriptObject targetSo{c, target};
    const int16_t currentAngle = static_cast<int16_t>(targetSo.facingAngle());
    const int16_t threshold = static_cast<int16_t>(so.argB());
    const uint32_t maskedDelta =
        static_cast<uint32_t>(static_cast<int32_t>(targetAngle) - static_cast<int32_t>(currentAngle)) & kAngleMask;
    // SNAP unless the remaining arc is still at least the threshold AND the threshold is positive.
    // A non-positive threshold forces an instant snap, because the guest falls through to the snap
    // whenever NEITHER of its two branch tests is taken. An earlier reading treated that fallthrough
    // as "keep turning", which inverts the polarity; it was caught before the code was written.
    if (static_cast<int32_t>(maskedDelta) < static_cast<int32_t>(threshold) || threshold <= 0) {
      targetSo.setFacingAngle(targetAngle);
      return 1;
    }
    targetSo.clearFacingCompanion();
    so.advancePhase();
    return 0;
  }
}

// =================================================================================================
// ---- the gauge leaf's record fields (guest 0x80073194) -------------------------------------------
// The gauge's own RECORD is a different struct from the script object's: the leaf takes it in a1
// and steps three of its own fields. It is named here because only this leaf touches it.
namespace {
constexpr uint32_t kGaugeResult = 10u; // written every call: base + gauge
constexpr uint32_t kGaugeBase = 14u;   // u16, read only
constexpr uint32_t kGaugeValue = 18u;  // u16, stepped by kGaugeStepPerCall
constexpr uint16_t kGaugeStepPerCall = 64u;
// The guest's own mode selector, on the SCRIPT object at the same offset this interpreter stamps its
// init marker into. Two owners, one word — see ScriptObject's header note on overloaded offsets.
constexpr uint32_t kGaugeModeOffset = ScriptObject::kInitMarker;
constexpr uint32_t kGaugePulseGate = 0xBFu;            // nonzero lets the wrap event fire
constexpr uint32_t kGuestWrapEventPulse = 0x80074590u; // still-substrate leaf
constexpr uint32_t kJalReturnFromWrapPulse = 0x80073238u;
constexpr int32_t kWrapEventArg0 = 24;
constexpr int32_t kWrapEventArg2 = 15;

// The two cached-tail leaves, guest 0x80031708 and 0x80031744, are the SAME machine: a null head
// node is a no-op, a node whose flag byte carries 0x80 is dead and both pointers are cleared, and
// otherwise the cache slot is refreshed to a pointer a fixed distance past the flag byte. The only
// thing that differs between them is WHICH header byte carries the flag and how far past it the
// cached pointer starts — so that is the leaf's only argument, and it is named.
void refreshCachedTail(Core *c, const ScriptObject &so, uint32_t flagByteOffset, uint32_t cacheDistance) {
  const uint32_t node = so.tailNode();
  if (node == 0) {
    return;
  }
  if ((c->mem_r8(node + flagByteOffset) & ScriptObject::kNodeDeadFlagBit) != 0) {
    so.clearTail();
    return;
  }
  so.setTailCache(node + cacheDistance);
}
} // namespace

// The cached-tail refresh, guest 0x80031708. Flag byte at node+3, cache pointer at node+4.
void ScriptInterp::refreshCachedTailHi(uint32_t obj) {
  refreshCachedTail(core, ScriptObject{core, obj}, ScriptObject::kTailFlagAtHi, ScriptObject::kTailCachePastFlagHi);
}

// Its twin, guest 0x80031744: flag byte at node+0, cache pointer at node+1.
void ScriptInterp::refreshCachedTailLo(uint32_t obj) {
  refreshCachedTail(core, ScriptObject{core, obj}, ScriptObject::kTailFlagAtLo, ScriptObject::kTailCachePastFlagLo);
}

// The active-actor query, guest 0x80042170. "Does the acted-upon actor's match byte read 1?"
// argA's own value selects WHICH actor:
//   0 — the object itself;
//   1 — the global actor record in the scratchpad, followed through its own pointer;
//   anything else — no match.
// The selector and the match byte are the SAME two object fields argA and argC are in the rest of
// this interpreter; only the widths and the reads differ.
int ScriptInterp::matchesActiveByKind(uint32_t obj) {
  Core *c = core;
  const ScriptObject so{c, obj};
  const int16_t kind = static_cast<int16_t>(c->mem_r16(obj + ScriptObject::kKindSelectorOffset));
  if (kind == 0) {
    return (c->mem_r8(obj + ScriptObject::kMatchByteOffset) == 1u) ? 1 : 0;
  }
  if (kind == 1) {
    const uint32_t record = c->mem_r32(kSecondaryActor);
    return (c->mem_r8(record + ScriptObject::kMatchByteOffset) == static_cast<uint32_t>(kind)) ? 1 : 0;
  }
  return 0;
}

// The status-byte mirror, guest 0x80044090: copy one byte from a fixed main-RAM global into a fixed
// scratchpad slot, and return 1. It takes no argument — the guest's signature has none.
int ScriptInterp::mirrorGlobalStatusByte() {
  Core *c = core;
  c->mem_w8(kStatusByteMirror, c->mem_r8(kStatusByteSource));
  return 1;
}

// The wrapping gauge, guest 0x80073194. It steps a u16 gauge in a SEPARATE record (taken in a1) up
// or down by 64 depending on a mode byte on the script object, wraps it to 0 at the sign boundary,
// fires a substrate event on the wrap when a gate byte allows it, and finally publishes
// `base + gauge` into the record's result field. Returns 1 iff the gauge wrapped this call.
//
// GUEST FRAME MIRROR: 32 bytes, s0 (the record) at +16, s1 (the wrap flag) at +20 and ra at +24,
// all LIVE incoming values. s0 in particular is read back after the substrate call, so it has to be
// the register and not a local.
int ScriptInterp::advanceGauge(uint32_t obj, uint32_t rec) {
  Core *c = core;
  const ScriptObject so{c, obj};
  static constexpr GuestFrameSpill kSpills[] = {{16, 16}, {17, 20}, {31, 24}};
  GuestFrame<32, 3> frame(c, kSpills);
  c->r[16] = rec; // s0 = the gauge record

  const uint8_t mode = c->mem_r8(obj + kGaugeModeOffset);
  c->r[17] = 0; // s1 = "did it wrap this call"
  if (mode == 0) {
    const uint32_t stepped = static_cast<uint16_t>(c->mem_r16(c->r[16] + kGaugeValue) + kGaugeStepPerCall);
    c->mem_w16(c->r[16] + kGaugeValue, static_cast<uint16_t>(stepped));
    if (static_cast<int32_t>(stepped << 16) > 0) { // crossed into the positive half
      c->mem_w16(c->r[16] + kGaugeValue, 0);
      c->r[17] = 1;
    }
  } else if (mode == 1) {
    const uint32_t stepped = static_cast<uint16_t>(c->mem_r16(c->r[16] + kGaugeValue) - kGaugeStepPerCall);
    c->mem_w16(c->r[16] + kGaugeValue, static_cast<uint16_t>(stepped));
    if (static_cast<int32_t>(stepped << 16) < 0) { // crossed below zero
      c->mem_w16(c->r[16] + kGaugeValue, 0);
      c->r[17] = 1;
    }
  }

  if (c->r[17] != 0 && c->mem_r8(obj + kGaugePulseGate) != 0) {
    c->r[4] = static_cast<uint32_t>(kWrapEventArg0);
    c->r[5] = 0;
    c->r[31] = kJalReturnFromWrapPulse;
    c->r[6] = static_cast<uint32_t>(kWrapEventArg2);
    psx::cpu::dispatchGuestToReturn0(*c, kGuestWrapEventPulse, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
  }

  const uint16_t base = c->mem_r16(c->r[16] + kGaugeBase);
  const uint16_t gauge = c->mem_r16(c->r[16] + kGaugeValue);
  const int ret = static_cast<int>(c->r[17]);
  c->mem_w16(c->r[16] + kGaugeResult, static_cast<uint16_t>(base + gauge));
  return ret;
}

// =================================================================================================
// The override REGISTRY — one row per guest address this class owns.
//
// Guest ABI per the registry's contract: arguments arrive in c->r[4]/c->r[5], the result in c->r[2].
// Registering here is the whole wiring step. `step()`'s dispatch loop reaches every non-0x3E opcode
// by guest address through the resident handler table, and address dispatch consults the override
// registry BEFORE falling to the guest's own body — so nothing in the loop had to change to make
// these seven handlers native. The other five are reached by direct same-module callers as well, so
// installing the setter is what intercepts both routes.
//
// The rows are a TABLE rather than a run of near-identical calls because the table is the only place
// a reader can see the whole ownership set at once, and because a twelfth handler added as a copied
// call is a twelfth place the declaration can be got wrong.
namespace {
// The C-ABI shims the registry takes. Each is the two lines of ABI marshalling the registry's
// signature demands; the LOGIC is the method named in the row.
void eov_loadNextEntry(Core *c) {
  c->r[2] = static_cast<uint32_t>(eng(c).script.loadNextEntry(c->r[4], c->r[5]));
}
void eov_op04SceneFlagRendezvous(Core *c) {
  c->r[2] = static_cast<uint32_t>(eng(c).script.op04SceneFlagRendezvous(c->r[4]));
}
void eov_op05WaitFrames(Core *c) {
  c->r[2] = static_cast<uint32_t>(eng(c).script.op05WaitFrames(c->r[4]));
}
void eov_op06TestSceneFlag(Core *c) {
  c->r[2] = static_cast<uint32_t>(eng(c).script.op06TestSceneFlag(c->r[4]));
}
void eov_op31TurnTowardTarget(Core *c) {
  c->r[2] = static_cast<uint32_t>(eng(c).script.op31TurnTowardTarget(c->r[4]));
}
void eov_op34ClaimGate(Core *c) {
  c->r[2] = static_cast<uint32_t>(eng(c).script.op34ClaimGate(c->r[4]));
}
void eov_op36MoveTowardScriptTarget(Core *c) {
  c->r[2] = static_cast<uint32_t>(eng(c).script.op36MoveTowardScriptTarget(c->r[4]));
}

// The two cached-tail leaves return nothing, so their shims write no result register.
void eov_refreshCachedTailHi(Core *c) {
  eng(c).script.refreshCachedTailHi(c->r[4]);
}
void eov_refreshCachedTailLo(Core *c) {
  eng(c).script.refreshCachedTailLo(c->r[4]);
}
void eov_matchesActiveByKind(Core *c) {
  c->r[2] = static_cast<uint32_t>(eng(c).script.matchesActiveByKind(c->r[4]));
}
void eov_mirrorGlobalStatusByte(Core *c) {
  c->r[2] = static_cast<uint32_t>(eng(c).script.mirrorGlobalStatusByte());
}
void eov_advanceGauge(Core *c) {
  c->r[2] = static_cast<uint32_t>(eng(c).script.advanceGauge(c->r[4], c->r[5]));
}

struct OverrideDeclaration {
  uint32_t guestAddress;
  const char *owner;
  void (*entry)(Core *);
};

constexpr OverrideDeclaration kOverrides[] = {
    // The entry advance. The guest reaches it by a DIRECT jal into the still-substrate classifier,
    // which only the installed setter intercepts — address dispatch never sees that call at all.
    {ScriptInterp::kAdvanceAddr, "ScriptInterp::loadNextEntry", eov_loadNextEntry},
    // The six verified opcode handlers, keyed by the opcode ids `ScriptOpcode` already names.
    {ScriptInterp::kOp04Addr, "ScriptInterp::op04SceneFlagRendezvous", eov_op04SceneFlagRendezvous},
    {ScriptInterp::kOp05Addr, "ScriptInterp::op05WaitFrames", eov_op05WaitFrames},
    {ScriptInterp::kOp06Addr, "ScriptInterp::op06TestSceneFlag", eov_op06TestSceneFlag},
    {ScriptInterp::kOp31Addr, "ScriptInterp::op31TurnTowardTarget", eov_op31TurnTowardTarget},
    {ScriptInterp::kOp34Addr, "ScriptInterp::op34ClaimGate", eov_op34ClaimGate},
    {ScriptInterp::kOp36Addr, "ScriptInterp::op36MoveTowardScriptTarget", eov_op36MoveTowardScriptTarget},
    // The five resident leaves, reached by direct same-module callers as well as by address
    // dispatch, so each needs the setter installed to intercept both routes.
    {0x80031708u, "ScriptInterp::refreshCachedTailHi", eov_refreshCachedTailHi},
    {0x80031744u, "ScriptInterp::refreshCachedTailLo", eov_refreshCachedTailLo},
    {0x80042170u, "ScriptInterp::matchesActiveByKind", eov_matchesActiveByKind},
    {0x80044090u, "ScriptInterp::mirrorGlobalStatusByte", eov_mirrorGlobalStatusByte},
    {0x80073194u, "ScriptInterp::advanceGauge", eov_advanceGauge},
};
} // namespace

void ScriptInterp::registerOverrides() {
  for (const OverrideDeclaration &row : kOverrides) {
    tomba::native::declareOverride(row.guestAddress, row.owner, row.entry);
  }
}
