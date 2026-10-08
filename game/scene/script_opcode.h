// game/scene/script_opcode.h — the cutscene SCRIPT BYTECODE's vocabulary, in one place.
//
// WHY THIS FILE EXISTS. `ScriptInterp` used to spell its bytecode as bare hex at every use site:
// `if (oid == 0x3Eu)`, `op0 & 0x2000u`, `case RET_ADVANCE_1:`, `c->r[31] = 0x80040F40u`, and
// `if (opWord & OP_FLAG_COND)`. A reader of the dispatch loop could not tell an opcode from a flag
// from a jal-site return constant without counting bits by hand, and the four vocabularies were
// interleaved through the same file. This header is where they are named, once.
//
// The ENTRY is 8 bytes — { u16 opcodeWord, u16 argA, u16 argB, u16 argC } — and 16 bytes when the
// opcode word carries `kFlagHasExtraBlock` (the extra four halfwords sit at entry+8). The low 11
// bits of the opcode word are the OPCODE ID; the top 5 are the flags below.
//
// WHAT IS KNOWN AND WHAT IS NOT, stated once instead of per call site:
//   * Seven opcode IDs have a native owner in this repository. `ScriptOpcode` names exactly those
//     seven. The other 56 of the 63-entry handler table at `ScriptInterp::kHandlerTableBase` have
//     NO native owner: they run the guest's own body. They are not enumerated here, because naming
//     them would be inventing a meaning nobody has read out of the image. `scriptOpcodeName`
//     reports those as `unowned_op_NN`, which is the honest answer, not a gap in the table.
//   * The STEP RETURN codes are the guest handler's `v0` value, and its switch is a four-way
//     decision plus an "anything else exits" default. `StepReturn` names what each one DOES to
//     guest state, not the number it happens to be.
//   * `AdvanceStepIndex` indexes the guest's own 7-entry switch table at 0x8001534C. Index 0 and 1
//     share a gate and differ only in which flag byte they leave behind; that is stated in the
//     comments rather than papered over with a name that claims more than is known.
#pragma once
#include <cstdint>

namespace tomba::script {

// ---- the opcode word's bit layout ------------------------------------------------------------------

// The low 11 bits carry the opcode ID. The guest handler table has 63 entries, so 0..62 are live and
// 63..2047 are unreachable — the id is masked, never range-checked, exactly as the guest does.
inline constexpr uint16_t kOpcodeIdMask = 0x07FFu;

// The top 5 bits. Each says something specific about the entry, and every one of them is read by a
// different part of the interpreter, which is why they are five names and not one "flags" word.
inline constexpr uint16_t kFlagConditionalEnd = 0x0800u; // last entry of a conditional arm
inline constexpr uint16_t kFlagValidEntry = 0x1000u;     // "this entry is live" — drives the advance gate
inline constexpr uint16_t kFlagHasExtraBlock = 0x2000u;  // the entry is 16 bytes (extra block at +8)
inline constexpr uint16_t kFlagSetsFlagBit2 = 0x4000u;   // init ORs 0x04 into the flag byte
inline constexpr uint16_t kFlagStopAfterEntry = 0x8000u; // the cursor stops on this entry

// ---- opcode IDs ------------------------------------------------------------------------------------

// The seven opcode IDs this repository owns natively. NOT an exhaustive list of the table: the
// remaining 56 are guest bodies, and `kUnownedOpcode` stands for all of them.
enum class ScriptOpcode : uint16_t {
  kSceneFlagRendezvous = 0x04u,    // op04: post a byte into the scene-flag array, then block until it reads back
  kWaitFrames = 0x05u,             // op05: count argA down one frame at a time
  kTestSceneFlag = 0x06u,          // op06: test a scene-flag byte against argC under argA's 3-way mode
  kTurnTowardTarget = 0x31u,       // op31: turn self-or-global-actor toward a computed angle
  kClaimGate = 0x34u,              // op34: claim a shared one-byte gate, then poll until it clears
  kMoveTowardScriptTarget = 0x36u, // op36: walk the actor toward the entry's literal target position
  kCallFunctionPointer = 0x3Eu,    // op3E: jalr the 32-bit fnptr the entry loaded

  // The guest's own handler body runs this one. It is a real, distinct opcode (it is the only one
  // step() special-cases), so it is named rather than lumped in with the 56 table entries.
  kUnownedOpcode = 0x3Fu,
};

// The stable slug for an opcode word, for logs and for a test that asserts the vocabulary did not
// drift. NEVER null and never allocates: the 56 handler-table entries with no native owner all
// answer "unowned", and a caller that needs the id prints `opcodeWord` beside it. A truthful answer
// rather than a placeholder for a meaning nobody has read out of the image.
const char *scriptOpcodeName(uint16_t opcodeWord);

// ---- what a handler RETURN does to guest state ------------------------------------------------------

// The guest handler's `v0`. The step loop reads it as: pause (0), advance with a kind byte (1/2/3),
// or exit the loop (4 and above). The names say the effect, because the numbers repeat.
enum class StepReturn : uint32_t {
  kPause = 0u,             // set the flag byte's pause bit and leave the loop with v0 = 1
  kAdvanceKind0 = 1u,      // run the advance sub-machine with kindArg = 0
  kAdvanceKind1 = 2u,      // run the advance sub-machine with kindArg = 1
  kAdvanceKind0Again = 3u, // a SECOND guest switch arm with the same effect as kAdvanceKind0
  // 4 and above: leave the loop with v0 = 0, no pause bit, no advance. That is the default arm,
  // not a named member, because the guest names nothing there either.
};
static_assert(static_cast<uint32_t>(StepReturn::kAdvanceKind0Again) ==
                  static_cast<uint32_t>(StepReturn::kAdvanceKind0) + 2u,
              "step()'s ret-code switch must still see 1, 2 and 3 as the three advance arms");

// ---- the advance sub-machine's switch index ---------------------------------------------------------

// The index the guest's own 7-entry switch table (at 0x8001534C, read out of MAIN.EXE) is taken
// with. `kOutOfRange` is what the table's literal -1 arm returns; it is a RETURN VALUE here, not an
// index, so it is named for that.
enum class AdvanceStepIndex : int {
  kGated = 0,                // keep running only while progress == 2; else clear the flag byte and progress
  kGatedKeepingFlagBit2 = 1, // the same gate, leaving bit 2 (0x04) set in the flag byte
  kStopWritingFF = 2,        // progress and the flag byte are both set to 0xFF; the phase byte is left alone
  kAbortNoWrites = 3,        // writes nothing at all
  kContinue = 4,             // unconditional: flag byte := 2, phase := 0
  kContinueWithFlagBit2 = 5, // unconditional: flag byte := 6, phase := 0
  kEndConditional = 6,       // flag byte := 0, progress := 0, phase := 0
  kOutOfRange = -1,          // index 7 or above: return -1 and touch nothing
};

// ---- how the cursor moves once an entry is done -----------------------------------------------------

// The entry's own top three bits decide where the cursor goes. `decodeAdvanceKind` reads them; a
// value with the top bit set is `kStop`, and all FOUR of the top-bit-set encodings (0x8000, 0xA000,
// 0xC000, 0xE000) decode to it. The guest gives those four no individual meaning, so inventing four
// would be claiming a distinction the image does not make — and, worse, leaving them unmapped would
// make an entry that means "stop" move the cursor instead, because no arm would match.
enum class AdvanceKind : uint16_t {
  kNext8 = 0x0000u,      // cursor += 8  (a plain 8-byte entry)
  kNext16 = 0x2000u,     // cursor += 16 (the entry carried an extra block)
  kBranchAt12 = 0x4000u, // kindArg == 0 -> follow the pointer at entry+12; else cursor += 16
  kBranchAt20 = 0x6000u, // kindArg == 0 -> follow the pointer at entry+20; else cursor += 24
  kStop = 0x8000u,       // top bit set: the script ends here and the cursor does not move
};

inline constexpr uint16_t kAdvanceKindMask = 0xE000u;
inline constexpr uint16_t kAdvanceStopBit = 0x8000u;

constexpr AdvanceKind decodeAdvanceKind(uint16_t opcodeWord) {
  const uint16_t bits = static_cast<uint16_t>(opcodeWord & kAdvanceKindMask);
  return (bits & kAdvanceStopBit) != 0u ? AdvanceKind::kStop : static_cast<AdvanceKind>(bits);
}

// ---- the guest's own return addresses ---------------------------------------------------------------

// The guest arms `ra` with one of these before each jump, because a body that pushes a frame writes
// the return address into guest stack bytes the oracle compares. They are addresses of the
// instruction AFTER the jump, one per advance arm.
inline constexpr uint32_t kJalReturnAfterNext8 = 0x80040F1Cu;
inline constexpr uint32_t kJalReturnAfterNext16 = 0x80040F40u;
inline constexpr uint32_t kJalReturnAfterBranch = 0x80040F5Cu;
inline constexpr uint32_t kJalReturnAfterBranchElse16 = 0x80040F6Cu;
// step()'s single jalr return, armed for every handler and for the op3E trampoline alike.
inline constexpr uint32_t kJalReturnFromHandler = 0x80040FCu;
// step()'s return address at the advance call site.
inline constexpr uint32_t kJalReturnFromAdvance = 0x80041168u;
// The op3E trampoline's own post-jalr constant, armed AFTER its stack descent.
inline constexpr uint32_t kJalReturnFromCallFnptr = 0x800412E4u;

} // namespace tomba::script
