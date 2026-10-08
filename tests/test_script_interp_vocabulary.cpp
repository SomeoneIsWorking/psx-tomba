// tests/test_script_interp_vocabulary.cpp — the cutscene script interpreter's THREE owners, and the
// decision boundaries each one owns.
//
// The three classes this file exercises are not renames; each one makes a decision the raw guest
// accessors left to the reader, and each of those decisions can be wrong in a way that RUNS:
//
//   ScriptObject      — the driven object's block. Its decisions are WHICH word is which field, what
//                       width and sign each one has, and that the extended block's first halfword is
//                       read two different ways on purpose (the step-count request before a move is
//                       solved, the step divisor after).
//   ScriptOpcode      — the bytecode vocabulary. Its decisions are the opcode id mask, the five flag
//                       bits, what a handler's return does to guest state, how the cursor moves, and
//                       which opcode names this repository actually claims.
//   decodeAdvanceKind — how the entry's top three bits become a cursor move.
//
// It also pins the guest address the interpreter uses for its contended claim gate, which was wrong
// ONCE in the wide RE (0x60 low, because an unrelated table's address was copied). A lens that reads
// the right bytes at the wrong address is the exact failure this file exists to make loud.
//
// These are production seams: the tests drive `ScriptObject` and the vocabulary headers directly,
// and `ScriptInterp`'s own handlers are left to the behavioural gates rather than reimplemented here.
#include "core.h"
#include "scene/script_globals.h"
#include "scene/script_object.h"
#include "scene/script_opcode.h"

#include <cstdint>
#include <cstdio>
#include <iterator>
#include <string>

using tomba::scene::ScriptObject;
using tomba::script::AdvanceKind;
using tomba::script::AdvanceStepIndex;
using tomba::script::ScriptOpcode;
using tomba::script::StepReturn;

namespace {

int failures = 0;
int checks = 0;

void check(bool condition, const char *detail) {
  checks++;
  if (!condition) {
    std::printf("FAIL: %s\n", detail);
    failures++;
  }
}

// Strings get their own comparison: the generic one takes its arguments by value, which clang-tidy
// correctly refuses for std::string, and an opcode name is the one case where a temporary is the
// natural thing to pass.
void checkEqStr(const char *actual, const char *expected, const char *detail) {
  check(std::string(actual) == std::string(expected), detail);
}

template <typename A, typename B> void checkEq(A actual, B expected, const char *detail) {
  checks++;
  if (!(actual == static_cast<A>(expected))) {
    std::printf("FAIL: %s\n", detail);
    failures++;
  }
}

constexpr uint32_t kObjectBase = 0x80100000u;
constexpr uint32_t kSentinelWord = 0xDEADBEEFu;

// The guest's own field offsets, spelled as the numbers they are, so a test failure points at the
// one number that moved rather than at a name that moved with it.
constexpr uint32_t kGuestProgress = 0x70u;
constexpr uint32_t kGuestStateFlags = 0x71u;
constexpr uint32_t kGuestArgA = 0x72u;
constexpr uint32_t kGuestArgB = 0x74u;
constexpr uint32_t kGuestArgC = 0x76u;
constexpr uint32_t kGuestCursor = 0x6Cu;
constexpr uint32_t kGuestPhase = 0x78u;
constexpr uint32_t kGuestMatchByte = 0x79u;
constexpr uint32_t kGuestExtra0 = 0x64u;
constexpr uint32_t kGuestExtra1 = 0x66u;
constexpr uint32_t kGuestExtra2 = 0x68u;
constexpr uint32_t kGuestExtra3 = 0x6Au;
constexpr uint32_t kGuestPositionX = 0x36u;
constexpr uint32_t kGuestTailNode = 0x3Cu;
constexpr uint32_t kGuestTailCache = 0x40u;

// ---- ScriptObject: the block's shape ------------------------------------------------------------

static void test_fields_land_on_the_guest_offsets(void) {
  Core core;
  const ScriptObject so{&core, kObjectBase};
  core.mem_w16(kObjectBase + kGuestArgA, 0x1234u);
  core.mem_w16(kObjectBase + kGuestArgB, 0x5678u);
  core.mem_w16(kObjectBase + kGuestArgC, 0x9ABCu);
  checkEq(so.argA(), 0x1234u, "argA does not read obj+0x72");
  checkEq(so.argB(), 0x5678u, "argB does not read obj+0x74");
  checkEq(so.argC(), 0x9ABCu, "argC does not read obj+0x76");

  so.setCursor(0x80123456u);
  checkEq(core.mem_r32(kObjectBase + kGuestCursor), 0x80123456u, "setCursor does not write obj+0x6C");
  checkEq(so.cursor(), 0x80123456u, "cursor does not read obj+0x6C");

  so.setPhase(3);
  checkEq(core.mem_r8(kObjectBase + kGuestPhase), 3u, "setPhase does not write obj+0x78");
  so.advancePhase();
  checkEq(core.mem_r8(kObjectBase + kGuestPhase), 4u, "advancePhase did not increment obj+0x78");
}

static void test_progress_is_SIGNED_and_the_stop_byte_stops_the_loop(void) {
  Core core;
  const ScriptObject so{&core, kObjectBase};
  // The stop arm stores the raw byte 0xFF, which sign-extends to -1. Reading it as unsigned 255
  // would leave the step loop's `<= 0` test never firing, so the sign is the decision under test.
  core.mem_w8(kObjectBase + kGuestProgress, ScriptObject::kProgressStoppedByte);
  check(static_cast<int32_t>(so.progress()) == -1, "the stop byte does not read back as a negative progress");
  check(so.progress() <= 0, "the stop byte does not satisfy the step loop's exit test");

  core.mem_w8(kObjectBase + kGuestProgress, 0x7Fu);
  check(static_cast<int32_t>(so.progress()) == 127, "a positive progress byte does not read back positive");
  check(so.progress() > 0, "a positive progress byte fails the step loop's continue test");
}

static void test_pause_is_a_set_not_an_assign(void) {
  Core core;
  const ScriptObject so{&core, kObjectBase};
  so.setStateFlags(ScriptObject::kFlagBitFromOpcode); // bit 2 already set by init
  so.pauseRequested();
  checkEq(so.stateFlags(), 0x05u, "pauseRequested overwrote a bit the opcode had already set");
  // ...and the other direction: pausing must not clear the valid-entry bit.
  so.setStateFlags(ScriptObject::kFlagBitValidEntry);
  so.pauseRequested();
  checkEq(so.stateFlags(), 0x03u, "pauseRequested cleared the valid-entry bit");
}

static void test_the_first_extra_halfword_reads_as_two_different_things(void) {
  Core core;
  const ScriptObject so{&core, kObjectBase};
  // The request lives in the HIGH byte and is read SIGNED; the divisor is the whole halfword.
  // Collapsing them into one accessor is the mistake this pair of assertions exists to catch.
  core.mem_w16(kObjectBase + kGuestExtra0, 0x0A00u); // request 10, divisor 2560
  checkEq(so.requestedStepCount(), 10, "the step-count request is not the high byte");
  checkEq(so.stepDivisor(), 0x0A00u, "the step divisor is not the whole halfword");

  // A request byte of 0x80 is -128 steps, not +128: the guest sign-extends it. This is compared as
  // a SIGNED 32-bit value on purpose. `checkEq` narrows its expected argument to the actual's type,
  // so passing -128 against a uint8_t actual would compare 128 == 128 and pass either way — a
  // vacuous assertion, which is exactly what the first version of this test was.
  core.mem_w16(kObjectBase + kGuestExtra0, 0x8000u);
  check(static_cast<int32_t>(so.requestedStepCount()) == -128, "the step-count request is not read signed");
  checkEq(so.stepDivisor(), 0x8000u, "the divisor must still be the raw halfword after a signed read");
  // ...and the same for the packed event argument. 0xFF0F is 65295 unsigned, so it is -241 as a
  // signed 16-bit value; writing it as `static_cast<int32_t>(0xFF0Fu)` would be 65295, which is the
  // trap this assertion exists to avoid in the test as well as in the code.
  core.mem_w16(kObjectBase + kGuestExtra3, 0xFF0Fu);
  check(static_cast<int32_t>(so.eventPackedArg()) == -241, "the packed event argument is not signed");
  core.mem_w16(kObjectBase + kGuestExtra3, 0x0009u);
  check(static_cast<int32_t>(so.eventPackedArg()) == 9, "the packed event argument is not signed when positive");

  so.setStepDivisor(0x0100u);
  checkEq(core.mem_r16(kObjectBase + kGuestExtra0), 0x0100u, "setStepDivisor does not write obj+0x64");
}

static void test_the_remaining_extra_halfwords(void) {
  Core core;
  const ScriptObject so{&core, kObjectBase};
  so.setExtraHalfword1(0x1234u);
  so.setExtraHalfword2(0x5678u);
  so.setExtraHalfword3(0x9ABCu);
  checkEq(core.mem_r16(kObjectBase + kGuestExtra1), 0x1234u, "extra halfword 1 is not obj+0x66");
  checkEq(core.mem_r16(kObjectBase + kGuestExtra2), 0x5678u, "extra halfword 2 is not obj+0x68");
  checkEq(core.mem_r16(kObjectBase + kGuestExtra3), 0x9ABCu, "extra halfword 3 is not obj+0x6A");
  // The event pair's first member is the ADDRESS of the flags word, not the word's value. Reading
  // the value instead would hand the event gate a plausible-looking integer where a pointer belongs,
  // and the gate would then read and write whatever that integer names.
  checkEq(so.eventFlagsPointer(), kObjectBase + kGuestExtra2, "the event flags member is not obj+0x68's address");
  core.mem_w32(kObjectBase + kGuestExtra2, 0x80001234u); // a plausible value, a different address
  checkEq(so.eventFlagsPointer(), kObjectBase + kGuestExtra2, "the event flags member read the word's value");
}

static void test_the_function_pointer_composes_low_half_first(void) {
  Core core;
  const ScriptObject so{&core, kObjectBase};
  core.mem_w16(kObjectBase + kGuestArgB, 0x8123u); // low half
  core.mem_w16(kObjectBase + kGuestArgC, 0x8001u); // high half
  // The guest composes it with one `lw`, so the low half is the LOW half. Swapping them would call
  // a different address entirely and still "work".
  checkEq(so.functionPointer(), 0x80018123u, "the op3E function pointer is composed high-half-first");
}

static void test_the_position_is_SIGNED(void) {
  Core core;
  const ScriptObject so{&core, kObjectBase};
  core.mem_w16(kObjectBase + kGuestPositionX, 0xFF9Cu); // -100
  check(static_cast<int32_t>(so.positionX()) == -100, "the borrowed actor position is not read signed");
  so.setPositionX(-100);
  checkEq(core.mem_r16(kObjectBase + kGuestPositionX), 0xFF9Cu, "setPositionX does not store two's complement");
}

static void test_clearing_the_tail_writes_both_slots(void) {
  Core core;
  const ScriptObject so{&core, kObjectBase};
  core.mem_w32(kObjectBase + kGuestTailNode, 0x80001234u);
  core.mem_w32(kObjectBase + kGuestTailCache, 0x80005678u);
  so.clearTail();
  checkEq(core.mem_r32(kObjectBase + kGuestTailNode), 0u, "clearTail left the node pointer set");
  checkEq(core.mem_r32(kObjectBase + kGuestTailCache), 0u, "clearTail left the cache pointer set");
}

static void test_two_lenses_over_two_objects_do_not_alias(void) {
  // A lens holds a base, not a cache. Two of them over two objects must not share state, which is
  // the failure a caching lens would introduce.
  Core core;
  const ScriptObject a{&core, kObjectBase};
  const ScriptObject b{&core, kObjectBase + 0x100u};
  a.setArgA(0x1111u);
  b.setArgA(0x2222u);
  checkEq(a.argA(), 0x1111u, "the first lens saw the second lens's write");
  checkEq(b.argA(), 0x2222u, "the second lens saw the first lens's write");
  checkEq(a.base(), kObjectBase, "lens::base does not report the base it was constructed with");
}

static void test_a_sentinel_word_is_untouched(void) {
  // The negative case for the whole lens: an address the interpreter never touches must keep its
  // value. Without this, a test that only checked the fields it writes would pass even if the lens
  // were scribbling over the object.
  Core core;
  core.mem_w32(kObjectBase + 0x24u, kSentinelWord);
  const ScriptObject so{&core, kObjectBase};
  so.stampInitMarker();
  so.clearRuntimeScratch();
  so.setProgress(1);
  so.setStateFlags(2u);
  so.setCursor(0x80001000u);
  so.setArgA(7u);
  so.setPhase(1u);
  so.setDeltas(1, 2, 3);
  so.setStepsRemaining(0x1000u);
  so.setTurnAngle2(0x40u);
  checkEq(core.mem_r32(kObjectBase + 0x24u), kSentinelWord, "the lens wrote outside the fields it names");
}

static void test_the_match_byte_offset_and_the_kind_selector_are_the_guest_ones(void) {
  // The active-actor query reaches these two by their RECORD index (114 and 121) and not by offset.
  // The two index names and the two offsets are the same numbers; a lens that kept only one form
  // would make one of the two readings unreachable.
  checkEq(ScriptObject::kMatchByteOffset, kGuestMatchByte, "the match byte is not the object's +0x79");
  checkEq(ScriptObject::kKindSelectorOffset, kGuestArgA, "the kind selector is not the object's +0x72");
}

// ---- the bytecode vocabulary --------------------------------------------------------------------

static void test_the_opcode_id_is_the_low_eleven_bits(void) {
  // Every flag bit must be OUTSIDE the id, or a flag would rename the opcode. This is a sweep, not
  // a sample: it walks every one of the 32 possible flag combinations and requires the id to be the
  // same for all of them.
  const uint16_t id = 0x03Eu;
  for (uint32_t flags = 0; flags < 0x20u; flags++) {
    const uint16_t word = static_cast<uint16_t>(id | (flags << 11));
    checkEq(static_cast<uint32_t>(word & tomba::script::kOpcodeIdMask), id, "a flag bit leaked into the opcode id");
  }
}

static void test_the_five_flag_bits_are_five_distinct_words(void) {
  // Two flags sharing a bit would make one of them unreachable, and the file's whole claim is that
  // each flag says something a DIFFERENT part of the interpreter reads.
  const uint16_t bits[] = {
      tomba::script::kFlagConditionalEnd,
      tomba::script::kFlagValidEntry,
      tomba::script::kFlagHasExtraBlock,
      tomba::script::kFlagSetsFlagBit2,
      tomba::script::kFlagStopAfterEntry,
  };
  for (size_t i = 0; i < std::size(bits); i++) {
    check(bits[i] != 0u, "a flag bit is zero, so it could never be tested");
    for (size_t j = i + 1; j < std::size(bits); j++) {
      check((bits[i] & bits[j]) == 0u, "two flag bits share a bit");
    }
  }
  // The five flags and the id mask must exactly tile the 16-bit word. If they did not, either a flag
  // would be unreachable or a flag bit would leak into the opcode id — both of which the file's
  // central claim depends on not happening.
  uint16_t tiled = tomba::script::kOpcodeIdMask;
  for (const uint16_t bit : bits) {
    tiled = static_cast<uint16_t>(tiled | bit);
  }
  checkEq(tiled, 0xFFFFu, "the id mask and the five flag bits do not tile the opcode word");
}

static void test_the_four_named_opcodes_are_the_four_the_dispatch_loop_special_cases(void) {
  // op04/op05/op06 are the two-phase/three-way scene-flag pair and the wait; op31/op36 are the
  // movement pair; op3E is the fnptr the dispatch loop routes natively. Each id is load-bearing: a
  // change here would make step() dispatch a different handler.
  checkEq(static_cast<uint32_t>(ScriptOpcode::kCallFunctionPointer),
          0x3Eu,
          "the fnptr opcode the dispatch loop special-cases is no longer 0x3E");
  checkEq(static_cast<uint32_t>(ScriptOpcode::kTurnTowardTarget), 0x31u, "op31 changed id");
  checkEq(static_cast<uint32_t>(ScriptOpcode::kMoveTowardScriptTarget), 0x36u, "op36 changed id");
  checkEq(static_cast<uint32_t>(ScriptOpcode::kClaimGate), 0x34u, "op34 changed id");
}

static void test_a_handler_return_names_its_effect_and_keeps_the_guests_numbers(void) {
  // The four arms of the ret-code switch. The NUMBERS are the guest's v0 values, so they cannot
  // move; what is under test is that the names still line up with them, because a reader trusting
  // the name and not the number would otherwise be misled.
  checkEq(static_cast<uint32_t>(StepReturn::kPause), 0u, "the pause arm is no longer the guest's 0");
  checkEq(static_cast<uint32_t>(StepReturn::kAdvanceKind0), 1u, "advance-with-kind-0 is no longer the guest's 1");
  checkEq(static_cast<uint32_t>(StepReturn::kAdvanceKind1), 2u, "advance-with-kind-1 is no longer the guest's 2");
  checkEq(
      static_cast<uint32_t>(StepReturn::kAdvanceKind0Again), 3u, "the second kind-0 arm is no longer the guest's 3");
  // The two kind-0 arms are the same effect reached through two guest switch arms. If they ever
  // diverged, "the same" in the name above would be a lie.
  checkEq(static_cast<uint32_t>(StepReturn::kAdvanceKind0),
          static_cast<uint32_t>(StepReturn::kAdvanceKind0Again) - 2u,
          "the two kind-0 arms are no longer adjacent");
}

static void test_the_advance_indices_are_the_guests_seven_table_entries(void) {
  // The sub-machine switches on an index read out of the guest's own 7-entry table. Index 3 is the
  // table's literal -1 arm and index 7-or-above is out of range, so -1 is the RETURN for both and
  // the named out-of-range member must not collide with a real index.
  checkEq(static_cast<int>(AdvanceStepIndex::kGated), 0, "the gated arm is no longer index 0");
  checkEq(static_cast<int>(AdvanceStepIndex::kStopWritingFF), 2, "the stop arm is no longer index 2");
  checkEq(static_cast<int>(AdvanceStepIndex::kAbortNoWrites), 3, "the no-writes arm is no longer index 3");
  checkEq(static_cast<int>(AdvanceStepIndex::kContinue), 4, "the unconditional arm is no longer index 4");
  checkEq(static_cast<int>(AdvanceStepIndex::kEndConditional), 6, "the conditional-end arm is no longer index 6");
  checkEq(static_cast<int>(AdvanceStepIndex::kOutOfRange), -1, "out of range is no longer the guest's -1");
  for (int index = 0; index <= 6; index++) {
    check(static_cast<int>(AdvanceStepIndex::kOutOfRange) != index, "out of range collides with a real table index");
  }
}

// ---- decodeAdvanceKind: the cursor move the entry's top bits choose -----------------------------

static void test_advance_kind_is_a_sweep_over_every_top_bit_pattern(void) {
  // Not a sample: all 16 combinations of the top three bits, with the id and the low flags varied,
  // so a decode that leaked either of them would be caught.
  for (uint32_t topBits = 0; topBits < 8u; topBits++) {
    for (uint32_t low = 0; low < 0x800u; low += 0x137u) {
      const uint16_t word = static_cast<uint16_t>((topBits << 13) | low);
      const AdvanceKind kind = tomba::script::decodeAdvanceKind(word);
      switch (kind) {
      case AdvanceKind::kNext8:
        checkEq(static_cast<uint32_t>(topBits), 0u, "kNext8 decoded from a non-zero top bit pattern");
        break;
      case AdvanceKind::kNext16:
        checkEq(static_cast<uint32_t>(topBits), 1u, "kNext16 decoded from the wrong top bit pattern");
        break;
      case AdvanceKind::kBranchAt12:
        checkEq(static_cast<uint32_t>(topBits), 2u, "kBranchAt12 decoded from the wrong top bit pattern");
        break;
      case AdvanceKind::kBranchAt20:
        checkEq(static_cast<uint32_t>(topBits), 3u, "kBranchAt20 decoded from the wrong top bit pattern");
        break;
      case AdvanceKind::kStop:
        check(topBits >= 4u, "kStop decoded from a top bit pattern with the top bit clear");
        break;
      }
    }
  }
  // Every top bit pattern with the top bit set is a STOP. The guest gives no name to the four of
  // them, and inventing four would be claiming a distinction the image does not make.
  for (uint32_t topBits = 4u; topBits < 8u; topBits++) {
    check(tomba::script::decodeAdvanceKind(static_cast<uint16_t>(topBits << 13)) == AdvanceKind::kStop,
          "a top-bit-set pattern is not a stop");
  }
}

// ---- the opcode names, including the honest "unowned" answer ------------------------------------

static void test_the_named_opcodes_report_their_names(void) {
  checkEqStr(
      tomba::script::scriptOpcodeName(0x3Eu), "call_function_pointer", "op3E is not named call_function_pointer");
  checkEqStr(tomba::script::scriptOpcodeName(0x3Eu | 0x2000u),
             "call_function_pointer",
             "the extra-block flag changed the opcode's name");
  checkEqStr(tomba::script::scriptOpcodeName(0x0031u), "turn_toward_target", "op31 is not named turn_toward_target");
  checkEqStr(tomba::script::scriptOpcodeName(0x0036u),
             "move_toward_script_target",
             "op36 is not named move_toward_script_target");
}

static void test_an_unowned_opcode_says_so_rather_than_guessing(void) {
  // 56 of the 63 handler-table entries have no native owner and NO name read out of the image.
  // Naming one of them would be inventing a meaning, so the answer must be the honest one — and it
  // must be the SAME honest answer for all of them, not a per-entry guess.
  for (uint32_t id = 0; id < 63u; id++) {
    const bool owned =
        id == 0x3Eu || id == 0x31u || id == 0x36u || id == 0x34u || id == 0x04u || id == 0x05u || id == 0x06u;
    if (owned) {
      continue;
    }
    const char *name = tomba::script::scriptOpcodeName(static_cast<uint16_t>(id));
    check(name != nullptr, "the name lookup returned null instead of an answer");
    checkEqStr(name, "unowned", "an unowned opcode was given a confident name");
  }
}

static void test_the_name_never_depends_on_the_flags(void) {
  // A name that changed when a flag was set would make a log line mean two different things.
  for (uint32_t id = 0; id < 64u; id++) {
    const char *bare = tomba::script::scriptOpcodeName(static_cast<uint16_t>(id));
    const char *flagged = tomba::script::scriptOpcodeName(static_cast<uint16_t>(id | 0xE800u));
    checkEqStr(flagged, bare, "an opcode's name changed when a flag bit was set");
  }
}

// ---- the fixed guest globals, including the address that was wrong once --------------------------

static void test_the_claim_gate_is_the_recomputed_address(void) {
  // MEASURED: the wide RE first recorded 0x800BF86F, which is 0x60 LOW — an unrelated table's
  // address got copied. It was recomputed from the guest's own constant arithmetic (32780<<16 +
  // -2040 + 7) and confirmed independently against the poll path's -2033. If this ever reads
  // 0x800BF86F again, this assertion is the one that fires.
  checkEq(tomba::scene::script_globals::kClaimGate, 0x800BF80Fu, "the claim gate is not the recomputed address");
  check(tomba::scene::script_globals::kClaimGate != 0x800BF86Fu,
        "the claim gate reverted to the 0x60-low draft address");
  // And the arithmetic that fixed it, re-derived here rather than trusted.
  checkEq((32780u << 16) - 2040u + 7u, 0x800BF80Fu, "the claim gate no longer matches the guest's own arithmetic");
}

static void test_the_scratchpad_globals_are_where_the_guest_puts_them(void) {
  checkEq(tomba::scene::script_globals::kScratchpadBase, 0x1F800000u, "the scratchpad base moved");
  checkEq(tomba::scene::script_globals::kSecondaryActor, 0x1F800214u, "the secondary actor slot moved");
  checkEq(tomba::scene::script_globals::kEventHandledMask, 0x1F80017Cu, "the event-handled mask moved");
  checkEq(tomba::scene::script_globals::kLookAnchorX, 0x1F800164u, "the look anchor X moved");
  checkEq(tomba::scene::script_globals::kLookAnchorZ, 0x1F800160u, "the look anchor Z moved");
  checkEq(tomba::scene::script_globals::kStatusByteMirror, 0x1F800207u, "the status byte mirror moved");
  checkEq(tomba::scene::script_globals::kStatusByteSource, 0x800E7EAau, "the status byte source moved");
  // Every scratchpad slot must be INSIDE the 1 KB scratchpad. A constant that is not is a
  // transcription slip, and this is the cheapest place that could ever be caught.
  for (uint32_t addr : {tomba::scene::script_globals::kSecondaryActor,
                        tomba::scene::script_globals::kEventHandledMask,
                        tomba::scene::script_globals::kLookAnchorX,
                        tomba::scene::script_globals::kLookAnchorZ,
                        tomba::scene::script_globals::kStatusByteMirror}) {
    check(addr >= tomba::scene::script_globals::kScratchpadBase &&
              addr < tomba::scene::script_globals::kScratchpadBase + 1024u,
          "a scratchpad slot is outside the scratchpad");
  }
}

} // namespace

int main() {
  test_fields_land_on_the_guest_offsets();
  test_progress_is_SIGNED_and_the_stop_byte_stops_the_loop();
  test_pause_is_a_set_not_an_assign();
  test_the_first_extra_halfword_reads_as_two_different_things();
  test_the_remaining_extra_halfwords();
  test_the_function_pointer_composes_low_half_first();
  test_the_position_is_SIGNED();
  test_clearing_the_tail_writes_both_slots();
  test_two_lenses_over_two_objects_do_not_alias();
  test_a_sentinel_word_is_untouched();
  test_the_match_byte_offset_and_the_kind_selector_are_the_guest_ones();

  test_the_opcode_id_is_the_low_eleven_bits();
  test_the_five_flag_bits_are_five_distinct_words();
  test_the_four_named_opcodes_are_the_four_the_dispatch_loop_special_cases();
  test_a_handler_return_names_its_effect_and_keeps_the_guests_numbers();
  test_the_advance_indices_are_the_guests_seven_table_entries();
  test_advance_kind_is_a_sweep_over_every_top_bit_pattern();
  test_the_named_opcodes_report_their_names();
  test_an_unowned_opcode_says_so_rather_than_guessing();
  test_the_name_never_depends_on_the_flags();

  test_the_claim_gate_is_the_recomputed_address();
  test_the_scratchpad_globals_are_where_the_guest_puts_them();

  std::printf("script interpreter vocabulary: %d checks, %s\n", checks, failures == 0 ? "PASS" : "FAIL");
  return failures == 0 ? 0 : 1;
}
