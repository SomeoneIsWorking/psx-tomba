// game/scene/script_interp_entry.cpp — how a cutscene script STREAM is read and advanced.
//
// One half of the interpreter, split from `script_interp.cpp` at the seam the two halves already
// share: this half answers "which entry is current, and where does the cursor go next"; the other
// answers "what does this opcode do". Entry loading (`init`, `loadCurrentEntry`), the GATED advance
// arms (`advanceEntry`), the next-entry walk (`loadNextEntry`), the advance sub-machine
// (`advanceStep`), and the entry-level flag opcodes (op04/op05/op06/op34) live here. The dispatch
// loop and the per-opcode bodies stay with `step`. `ScriptInterp` remains ONE class with ONE
// declaration header (`scene/script_interp.h`); only the bodies moved, unchanged.

#include "cfg.h"
#include "core.h"
#include "core/engine/engine.h"
#include "core/entry/game_ctx.h"
#include "core/overrides/native_override_catalog.h"
#include "execution_services.h"
#include "game.h"
#include "guest_abi.h"
#include "guest_call.h"
#include "object/behavior_dispatch.h"
#include "scene/scene_flags.h"
#include "scene/script_globals.h"
#include "scene/script_interp.h"
#include "scene/script_object.h"
#include "scene/script_opcode.h"

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
constexpr uint8_t kRendezvousPost = 0u;                // post postValue into the slot, then pause
constexpr uint8_t kRendezvousAwait = 1u;               // poll the slot until it reads awaitValue
constexpr uint32_t kAngleMask = 0xFFFu;                // one full turn
constexpr uint32_t kHalfTurn = 0x800u;                 // two quarters: the short-way-round decision boundary
constexpr uint32_t kTurnStepDefault = 0x100u;          // the step op36 hands its own turn
constexpr uint32_t kGuestIsqrt = 0x80084080u;          // the GTE-LZCS square-root leaf (still substrate)
constexpr uint32_t kGuestRatan2 = 0x80085690u;         // Trig::ratan2, wired as an override
constexpr int32_t kWholeMoveQ12 = 0x1000;              // stepsRemaining starts at "the whole move" (Q12)
constexpr int32_t kQ12RoundUp = 0xFFF;                 // the guest's round-toward-zero nudge for a negative
constexpr int32_t kDefaultStepCount = 10;              // requestedStepCount == 0 means "ten steps"
constexpr int32_t kTurnModeSnapAndStop = 0;            // turn-mode 0: snap the facing, then stop after phase 1
constexpr int32_t kTurnModeSnapAndTurn = 1;            // turn-mode 1: snap the facing, then keep turning
constexpr int32_t kHalfTurnAngle = 2048;               // half of a 4096-unit circle
constexpr uint32_t kGaugeResult = 10u;                 // written every call: base + gauge
constexpr uint32_t kGaugeBase = 14u;                   // u16, read only
constexpr uint32_t kGaugeValue = 18u;                  // u16, stepped by kGaugeStepPerCall
constexpr uint32_t kGaugePulseGate = 0xBFu;            // nonzero lets the wrap event fire
constexpr uint32_t kGuestWrapEventPulse = 0x80074590u; // still-substrate leaf
constexpr int8_t kProgressGateValue = 2;
constexpr uint16_t kRepeatMask = 0x3Fu;
constexpr uint16_t kArmOnceBit = 0x80u;
constexpr uint16_t kLatchedBit = 0x40u;
constexpr uint32_t kTurnModeOffset = 10u;
constexpr uint16_t kModeUseCurrent = 0;
constexpr uint16_t kModeAimAtActorFromAnchor = 1;
constexpr uint16_t kModeAimAtActorFromAnchorFlipped = 2;
constexpr uint16_t kModeAimFromSelf = 3;
constexpr uint16_t kModeTrackLivePoint = 10;
constexpr uint16_t kGaugeStepPerCall = 64u;
constexpr uint32_t kGaugeModeOffset = ScriptObject::kInitMarker;
constexpr uint32_t kJalReturnFromWrapPulse = 0x80073238u;
constexpr int32_t kWrapEventArg0 = 24;
constexpr int32_t kWrapEventArg2 = 15;

// The GATED advance arms, shared. Index 0 and index 1 of the sub-machine's table are the same
// machine; the second leaves bit 2 (0x04) of the flag byte set in BOTH of its outcomes. Naming the
// difference as the bit it is — rather than as two near-identical blocks — is exact: 2|0x04 == 6 and
// 0|0x04 == 4, which are the two values index 1 writes.
//
// Returns 1 when the script keeps running (progress was the gate value) and 0 when it stops.
int applyGatedAdvance(const ScriptObject &so, uint8_t flagBit) {
  if (so.progress() == kProgressGateValue) {
    so.setStateFlags(static_cast<uint8_t>(ScriptObject::kFlagBitValidEntry | flagBit));
    so.setPhase(0);
    return 1;
  }
  so.setStateFlags(flagBit);
  so.setProgress(0);
  so.setPhase(0);
  return 0;
}
void ScriptInterp::init(uint32_t obj, uint32_t tableA, uint32_t scriptPtr) {
  Core *c = core;
  const ScriptObject so{c, obj};
  // FUN_80040CDC prologue-equivalent state writes on `obj`.
  so.setSecondaryTable(tableA);
  so.stampInitMarker();
  so.clearRuntimeScratch();
  so.setProgress(0);
  so.setPhase(0);
  so.setStateFlags(0);
  // Load the first entry so subsequent step() calls see argA/B/C and the (optional) extra block.
  loadCurrentEntry(obj, scriptPtr);
  // Reload the flag byte from the first entry's flag bits (matches the guest instruction path's
  // post-load flag scan). The two bits are SET, not assigned: the second is an OR onto whatever
  // the first left behind.
  const uint16_t op0 = c->mem_r16(scriptPtr + 0);
  if (op0 & tomba::script::kFlagValidEntry) {
    so.setStateFlags(ScriptObject::kFlagBitValidEntry);
  }
  if (op0 & tomba::script::kFlagSetsFlagBit2) {
    so.setStateFlags(static_cast<uint8_t>(so.stateFlags() | ScriptObject::kFlagBitFromOpcode));
  }
}
void ScriptInterp::loadCurrentEntry(uint32_t obj, uint32_t scriptPtr) {
  Core *c = core;
  const ScriptObject so{c, obj};
  // FUN_80040DE0 body — copy the 3 u16 args and record the cursor.
  so.setCursor(scriptPtr);
  so.setArgA(c->mem_r16(scriptPtr + 2));
  so.setArgB(c->mem_r16(scriptPtr + 4));
  so.setArgC(c->mem_r16(scriptPtr + 6));
  // If the opcode says "extra block", copy 4 more halfwords into the object's extended block.
  const uint16_t op0 = c->mem_r16(scriptPtr + 0);
  if (op0 & tomba::script::kFlagHasExtraBlock) {
    const uint32_t ex = scriptPtr + 8;
    so.setExtraHalfword0(c->mem_r16(ex + 0));
    so.setExtraHalfword1(c->mem_r16(ex + 2));
    so.setExtraHalfword2(c->mem_r16(ex + 4));
    so.setExtraHalfword3(c->mem_r16(ex + 6));
  }
}
int ScriptInterp::advanceEntry(uint32_t obj, uint32_t kindArg) {
  // NAMING: this method's registry row is keyed at kAdvanceAddr (0x80040E54), but its behaviour has
  // always been FUN_80040FA0's — the advance sub-machine `step()` actually calls. The guest's raw
  // entry-advance body at 0x80040E54 stays a substrate leaf that `advanceStep` still dispatches to;
  // this forwards straight to the verified native body instead of taxi-ing through the substrate for
  // a leaf this class already owns.
  return advanceStep(obj, kindArg);
}
// op05 — FUN_80042090. "Wait N frames": count argA down by one every call.
//
// The ret-code decision here is subtle and was transcribed wrong once. The guest's `r[2]` is
// `uint32_t`, so `(decremented << 16) >> 31` is a LOGICAL shift: it extracts bit 15 of the
// decremented 16-bit counter as 0 (still counting) or 1 (expired). It is NOT a sign-replicate 0/-1
// idiom, and reading it as one changes WHICH of step()'s four ret-code arms a wait lands in. 0 is
// `StepReturn::kPause` (set the pause bit, exit without advancing); 1 is `StepReturn::kAdvanceKind0`,
// so a wait DOES advance the script cursor on expiry — correct behaviour, confirmed against the
// substrate ground truth.
int ScriptInterp::op05WaitFrames(uint32_t obj) {
  const ScriptObject so{core, obj};
  const uint16_t decremented = static_cast<uint16_t>(so.argA() - 1u);
  so.setArgA(decremented);
  return (static_cast<int16_t>(decremented) < 0) ? 1 : 0;
}
// op06 — FUN_800420AC. "Test a scene-flag byte": the predicate the conditional-arm entries use.
// argA selects one of three compares against the byte at `scene_flags::flagAddr(argB)`:
//   0 — exact match against the FULL sign-extended argC, NOT a byte-truncated compare. A negative
//       or >255 argC can never equal a byte, so it must not be truncated into one.
//   1 — any masked bit set. Truncating argC to a byte IS safe here: the AND's upper 24 bits are
//       always 0 because the table byte carries no high bits.
//   2 — every masked bit clear.
// argA >= 3 is unreachable in the guest's own byte-shape and answers 0, for parity.
int ScriptInterp::op06TestSceneFlag(uint32_t obj) {
  Core *c = core;
  const ScriptObject so{c, obj};
  const int16_t argA = static_cast<int16_t>(so.argA());
  const int16_t argB = static_cast<int16_t>(so.argB());
  const int16_t argC = static_cast<int16_t>(so.argC());
  const uint8_t tableByte = c->mem_r8(scene_flags::flagAddr(argB));
  if (argA == 1) {
    return ((tableByte & static_cast<uint8_t>(argC)) != 0) ? 1 : 0;
  }
  if (argA == 0) {
    return (static_cast<uint32_t>(tableByte) == static_cast<uint32_t>(static_cast<int32_t>(argC))) ? 1 : 0;
  }
  if (argA == 2) {
    return ((tableByte & static_cast<uint8_t>(argC)) == 0) ? 1 : 0;
  }
  return 0;
}
// op04 — FUN_8004201C. The SCENE-FLAG RENDEZVOUS, and op06's write side. op06 only TESTS a
// scene-flag byte; op04 both posts one and blocks until it reads back, which is how two scripted
// actors in one scene hand a cutscene back and forth. Two phases on the object's phase byte, exactly like op34:
//   phase 0 (POST)  — flags[slot] = postValue, then phase++ and PAUSE (always one frame minimum).
//   phase 1 (AWAIT) — advance iff flags[slot] == awaitValue, else PAUSE and poll again next call.
//
// The slot index is read SIGNED (guest `lh`, not `lhu`), so a negative argA indexes BELOW the flag
// array. Preserved rather than "cleaned up": scripts do use the low-index end of that array.
//
// THE POST/AWAIT ASYMMETRY, which matters: the post is an unconditional byte STORE, not a
// compare-and-set. Whichever actor runs its phase 0 last wins the slot outright, so the handshake is
// only correct while the two actors' step() calls stay in the guest's own per-frame ORDER. A clobbered
// post is indistinguishable from "the partner has not posted yet" by state alone, which is why this
// handler logs the value it overwrote.
int ScriptInterp::op04SceneFlagRendezvous(uint32_t obj) {
  Core *c = core;
  const ScriptObject so{c, obj};
  const int16_t slot = static_cast<int16_t>(so.argA());
  const uint8_t postValue = static_cast<uint8_t>(so.argB());
  const int16_t awaitValue = static_cast<int16_t>(so.argC());
  const uint32_t flagAddr = scene_flags::flagAddr(slot);
  const uint8_t phase = so.phase();

  if (phase == kRendezvousPost) {
    const uint8_t was = c->mem_r8(flagAddr);
    c->mem_w8(flagAddr, postValue);
    so.advancePhase();
    cfg_logf("rendez",
             "post obj=%08X ptr=%08X slot=%d %u->%u (await %d)",
             obj,
             so.cursor(),
             (int)slot,
             was,
             postValue,
             (int)awaitValue);
    return static_cast<int>(StepReturn::kPause);
  }
  if (phase == kRendezvousAwait) {
    const uint8_t have = c->mem_r8(flagAddr);
    // The guest compares the zero-extended byte against the FULL sign-extended awaitValue — the same
    // shape as op06's exact-match arm. Do not truncate awaitValue to a byte.
    const bool met = (static_cast<uint32_t>(have) == static_cast<uint32_t>(static_cast<int32_t>(awaitValue)));
    cfg_logf("rendez", "meet obj=%08X ptr=%08X slot=%d == %d", obj, so.cursor(), (int)slot, (int)awaitValue);
    return met ? static_cast<int>(StepReturn::kAdvanceKind0) : static_cast<int>(StepReturn::kPause);
  }
  return static_cast<int>(StepReturn::kPause); // phase >= 2: unreachable in the guest's byte shape
}
// FUN_80040E54 — loadNextEntry(obj, kindArg): THE ENTRY ADVANCE. 1:1 with guest 0x80040E54.
//
// Every other part of this interpreter was already owned and readable; this was the hole. It reads
// the CURRENT entry's opcode word (before moving), decides where the cursor goes from the word's
// top 3 bits, loads the new entry, and returns the index advanceStep() then uses to decide whether
// the script keeps running. Owning it means the `script` channel finally covers scripts that are
// stepped by the SUBSTRATE loop too, since typed runtime address dispatch routes here regardless of the caller —
// which is exactly what kanban #60 needed and could not see.
//
// GUEST FRAME MIRROR: gen descends sp by 32 and spills s0/s1/ra at +16/+20/+24. The only callee
// (loadCurrentEntry / FUN_80040DE0) is frameless, so the loop state can live in C++ locals; the
// SPILL SLOTS still have to be written, and s0/s1 restored, or core A's stack bytes drift.
// The entry ADVANCE — FUN_80040E54. This was the last unreadable link in the interpreter: it reads
// the CURRENT entry's opcode word (BEFORE moving), decides where the cursor goes from that word's
// top three bits, loads the new entry, and returns the index the advance sub-machine switches on.
// Owning it is what makes the `script` channel cover scripts stepped by the SUBSTRATE loop too,
// because address dispatch routes here regardless of the caller.
//
// GUEST FRAME MIRROR: the guest descends sp by 32 and spills s0/s1/ra at +16/+20/+24. The only
// callee (`loadCurrentEntry`) is frameless, so the loop state can live in C++ locals; the SPILL
// SLOTS still have to be written and s0/s1 restored, or this core's stack bytes drift from the
// reference. `GuestFrame` is that contract, spelled once.
int ScriptInterp::loadNextEntry(uint32_t obj, uint32_t kindArg) {
  Core *c = core;
  static constexpr GuestFrameSpill kSpills[] = {{16, 16}, {17, 20}, {31, 24}};
  GuestFrame<32, 3> frame(c, kSpills);

  const ScriptObject so{c, obj};
  const uint32_t cursor = so.cursor();
  const uint16_t opWord = c->mem_r16(cursor + 0);
  const uint16_t kindBits = static_cast<uint16_t>(opWord & tomba::script::kAdvanceKindMask);
  // An entry WITHOUT the valid-entry bit only keeps the script alive while progress == 2; with it,
  // the script continues unconditionally. The branch arms add 1 to reach the sub-machine's 1/5
  // twins, which is the same gate with the flag byte's bit 2 left set.
  int result = (opWord & tomba::script::kFlagValidEntry) ? static_cast<int>(AdvanceStepIndex::kContinue)
                                                         : static_cast<int>(AdvanceStepIndex::kGated);

  switch (tomba::script::decodeAdvanceKind(opWord)) {
  case AdvanceKind::kNext8:
    c->r[31] = tomba::script::kJalReturnAfterNext8;
    loadCurrentEntry(obj, cursor + 8u);
    break;
  case AdvanceKind::kNext16:
    c->r[31] = tomba::script::kJalReturnAfterNext16;
    loadCurrentEntry(obj, cursor + 16u);
    break;
  case AdvanceKind::kBranchAt12:
    if (kindArg != 0u) {
      c->r[31] = tomba::script::kJalReturnAfterNext16;
      loadCurrentEntry(obj, cursor + 16u);
    } else {
      c->r[31] = tomba::script::kJalReturnAfterBranch;
      result += 1;
      loadCurrentEntry(obj, c->mem_r32(cursor + 12u));
    }
    break;
  case AdvanceKind::kBranchAt20:
    if (kindArg != 0u) {
      c->r[31] = tomba::script::kJalReturnAfterBranchElse16;
      loadCurrentEntry(obj, cursor + 24u);
    } else {
      c->r[31] = tomba::script::kJalReturnAfterBranch;
      result += 1;
      loadCurrentEntry(obj, c->mem_r32(cursor + 20u));
    }
    break;
  case AdvanceKind::kStop:
    // The cursor is deliberately left pointing at THIS entry; the sub-machine's stop arm writes
    // 0xFF into progress and the flag byte, and step() exits on the next loop test.
    cfg_logf("script",
             "advance obj=%08X ptr=%08X op=%04X (%s) -> STOP",
             obj,
             cursor,
             opWord,
             tomba::script::scriptOpcodeName(opWord));
    return static_cast<int>(AdvanceStepIndex::kStopWritingFF);
  }
  // The conditional-end bit on the entry just executed overrides everything above: it marks the last
  // entry of a conditional arm, and the sub-machine's index-6 arm clears progress and the flag byte
  // — ending the script rather than falling into whatever entry follows in memory.
  if (opWord & tomba::script::kFlagConditionalEnd) {
    result = static_cast<int>(AdvanceStepIndex::kEndConditional);
  }

  cfg_logf("script",
           "advance obj=%08X ptr=%08X op=%04X (%s) kind=%04X arg=%u -> %08X (idx %d)",
           obj,
           cursor,
           opWord,
           tomba::script::scriptOpcodeName(opWord),
           kindBits,
           kindArg,
           so.cursor(),
           result);
  return result;
}
// op34 — FUN_80042E10. "Claim a shared one-byte gate, then wait for it to clear." A two-phase
// machine on the object's phase byte:
//   phase 0 (CLAIM) — argA's low three bits are the claim tag. A tag of 0 jumps STRAIGHT to the
//     phase-increment tail, skipping both the write and the sign-bit test: the guest gates both on
//     `tag != 0`, and reproducing that skip matters because it is the difference between "wait" and
//     "do not wait" for a script that asks for no wait. Otherwise: if the gate byte reads 0, claim
//     it by writing the tag; then if argA's sign bit is set, return 1 and advance immediately.
//   phase 1 (POLL)  — advance once the gate byte reads 0 again, else pause and poll next call.
//   phase >= 2      — unreachable in the guest's own byte shape; 0, for parity.
int ScriptInterp::op34ClaimGate(uint32_t obj) {
  Core *c = core;
  const ScriptObject so{c, obj};
  const uint8_t phase = so.phase();
  if (phase == 0) {
    const uint16_t argARaw = so.argA();
    const uint8_t claimTag = static_cast<uint8_t>(argARaw & 7u);
    if (claimTag != 0) {
      if (c->mem_r8(kClaimGate) == 0) {
        c->mem_w8(kClaimGate, claimTag);
      }
      if (argARaw & 0x8000u) {
        return static_cast<int>(StepReturn::kAdvanceKind0); // "no-wait" argA: advance immediately
      }
    }
    so.advancePhase();
    return static_cast<int>(StepReturn::kPause);
  }
  if (phase == 1) {
    return (c->mem_r8(kClaimGate) == 0) ? static_cast<int>(StepReturn::kAdvanceKind0)
                                        : static_cast<int>(StepReturn::kPause);
  }
  return static_cast<int>(StepReturn::kPause);
}
// The advance SUB-MACHINE — FUN_80040FA0, which is what `advanceEntry` above actually calls. It
// runs the still-substrate entry classifier (FUN_80040E54) to get an index, then applies that
// index's effect to the object's own bytes.
//
// GUEST FRAME MIRROR, and why it is NOT optional: the guest descends sp by 24 and spills s0@+16
// and ra@+20. An earlier revision of this file asserted the frame was "not shared/observable beyond
// the call" and skipped it. That was wrong: the still-substrate classifier — and anything IT calls
// — spills relative to this frame's sp, so without the descent every downstream spill landed 24
// bytes high and carried this core's stale register bytes instead of the reference's (side-by-side
// watch-cut 153, Charles' anim-install chain, 2026-07-10).
int ScriptInterp::advanceStep(uint32_t obj, uint32_t kindArg) {
  Core *c = core;
  const ScriptObject so{c, obj};
  static constexpr GuestFrameSpill kSpills[] = {{16, 16}, {31, 20}};
  GuestFrame<24, 2> frame(c, kSpills);
  c->r[16] = obj;
  c->r[31] = 0x80040FB4u;
  c->r[4] = obj;
  c->r[5] = kindArg;
  psx::cpu::dispatchGuestToReturn0(*c,
                                   0x80040E54u,
                                   psx::cpu::ExecutionBudget::currentTurn(*c),
                                   __func__); // still-substrate entry classifier, out of band for this pass
  const uint32_t ret = c->r[2];
  if (ret >= 7u) {
    return -1; // out of range: the guest's table has 7 arms and index 3 is its literal -1
  }
  switch (static_cast<AdvanceStepIndex>(ret)) {
  case AdvanceStepIndex::kGated:
    return applyGatedAdvance(so, 0u);
  case AdvanceStepIndex::kGatedKeepingFlagBit2:
    return applyGatedAdvance(so, ScriptObject::kFlagBitFromOpcode);
  case AdvanceStepIndex::kStopWritingFF:
    // Jumps straight to the return and SKIPS the phase-byte clear the other arms do.
    so.setProgressStopped();
    so.setStateFlags(ScriptObject::kProgressStoppedByte);
    return 0;
  case AdvanceStepIndex::kAbortNoWrites:
    return -1;
  case AdvanceStepIndex::kContinue:
    so.setStateFlags(ScriptObject::kFlagBitValidEntry);
    so.setPhase(0);
    return 1;
  case AdvanceStepIndex::kContinueWithFlagBit2:
    so.setStateFlags(static_cast<uint8_t>(ScriptObject::kFlagBitValidEntry | ScriptObject::kFlagBitFromOpcode));
    so.setPhase(0);
    return 1;
  case AdvanceStepIndex::kEndConditional:
    so.setStateFlags(0);
    so.setProgress(0);
    so.setPhase(0);
    return 0;
  case AdvanceStepIndex::kOutOfRange:
    return -1; // unreachable: `ret` is already bounded to 0..6 above
  }
  return -1;
}
