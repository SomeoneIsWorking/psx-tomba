// game/scene/script_object.h — a TYPED LENS over the guest object block the cutscene script
// interpreter drives.
//
// WHY THIS EXISTS. Every `ScriptInterp` body reached into guest RAM as
// `c->mem_r8(obj + OBJ_PROGRESS_70)` / `c->mem_r16(obj + OBJ_ARG_A_72)`, with the offsets declared
// as a flat list of constants two hundred lines above the code that used them. That is a
// transcript: a reader could not see that `OBJ_PROGRESS_70` and `OBJ_FLAGS_71` are the two bytes
// the step loop's exit test and pause bit actually live in, that `OBJ_ARG_A_72` is the current
// entry's first argument while `OBJ_SCRIPT_PTR` is where the cursor is, or that four different
// scripts read the same word with four different names. This class is the one place the block's
// shape is written down, and it is a VIEW — no cache, no copy. It reads and writes exactly the
// guest addresses the raw calls did.
//
// SCOPE, AND THE HONEST CAVEAT THAT COMES WITH IT. Some fields below are the SCRIPT MACHINE's own
// (the cursor, the progress byte, the argument halfwords, the phase byte). Others are the generic
// actor fields the movement opcodes happen to borrow (`positionX/Y/Z`, `facingAngle`). Those actor
// offsets are OVERLOADED elsewhere in the game with a different meaning — `obj+0x46` is an
// animation mode to `Engine::walkStart` and a 0xFF marker to `ScriptInterp::init` — so the names
// here are scoped to the SCRIPT's reading of them and say so. Unifying them across owners would
// misname one caller or the other; see `Engine::animTick`'s own note on the same hazard.
//
// A lens has no lifetime to bound and holds no ownership of guest RAM. It is a value: construct one
// per use (`ScriptObject so{c, obj};`) exactly like the raw `mem_r32` it stands in for.
#pragma once
#include "core.h"
#include <cstdint>

namespace tomba::scene {

class ScriptObject {
public:
  ScriptObject(Core *core, uint32_t base) : core_(core), base_(base) {}

  // ---- the script machine's own fields --------------------------------------------------------------

  // +0x70 s8 — the step loop's guard. `ScriptInterp::step` runs while this is POSITIVE and leaves
  // v0 = 0 on the frame it reaches zero or below. The advance sub-machine is the only other writer.
  int8_t progress() const {
    return static_cast<int8_t>(core_->mem_r8(base_ + kProgress));
  }
  void setProgress(int8_t v) const {
    core_->mem_w8(base_ + kProgress, static_cast<uint8_t>(v));
  }
  // The advance sub-machine's stop arm stores the raw byte 0xFF, which sign-extends to -1: the same
  // "no longer positive" exit the loop test already takes. Named so that write does not read as a
  // different quantity from `progress()`.
  void setProgressStopped() const {
    core_->mem_w8(base_ + kProgress, kProgressStoppedByte);
  }

  // +0x71 u8 — bitfield. Bit 0 is the PAUSE bit step() sets on a `kPause` return; bit 1 is set at
  // init from `kFlagValidEntry`; bit 2 is set at init from `kFlagSetsFlagBit2`. The advance
  // sub-machine overwrites the whole byte with 0, 2, 4 or 6.
  uint8_t stateFlags() const {
    return core_->mem_r8(base_ + kStateFlags);
  }
  void setStateFlags(uint8_t v) const {
    core_->mem_w8(base_ + kStateFlags, v);
  }
  void pauseRequested() const {
    setStateFlags(static_cast<uint8_t>(stateFlags() | kFlagBitPaused));
  }

  // +0x72/+0x74/+0x76 u16 — the current entry's three argument halfwords. For op3E the pair
  // (+0x74, +0x76) is instead the LOW and HIGH halves of a 32-bit function pointer.
  uint16_t argA() const {
    return core_->mem_r16(base_ + kArgA);
  }
  void setArgA(uint16_t v) const {
    core_->mem_w16(base_ + kArgA, v);
  }
  uint16_t argB() const {
    return core_->mem_r16(base_ + kArgB);
  }
  void setArgB(uint16_t v) const {
    core_->mem_w16(base_ + kArgB, v);
  }
  uint16_t argC() const {
    return core_->mem_r16(base_ + kArgC);
  }
  void setArgC(uint16_t v) const {
    core_->mem_w16(base_ + kArgC, v);
  }
  // The op3E function pointer, composed the way the guest's `lw` composes it: low half first.
  uint32_t functionPointer() const {
    return (static_cast<uint32_t>(argC()) << 16) | static_cast<uint32_t>(argB());
  }

  // +0x6C u32 — where the cursor sits. Written by init, and moved by the entry advance.
  uint32_t cursor() const {
    return core_->mem_r32(base_ + kCursor);
  }
  void setCursor(uint32_t v) const {
    core_->mem_w32(base_ + kCursor, v);
  }

  // +0x78 u8 — the per-opcode PHASE byte. init clears it, and every multi-call opcode reuses it as
  // its own state machine: op04's post/await pair, op34's claim/poll pair, op36's init/turn/move
  // trio, op31's aim/turn pair. A reader who does not know which opcode is running cannot know
  // what the number means, so each handler names the phase it is looking at.
  uint8_t phase() const {
    return core_->mem_r8(base_ + kPhase);
  }
  void setPhase(uint8_t v) const {
    core_->mem_w8(base_ + kPhase, v);
  }
  void advancePhase() const {
    setPhase(static_cast<uint8_t>(phase() + 1u));
  }

  // +0x7C u32 — the script's secondary table pointer, handed to init and never read again by the
  // interpreter. Kept because init writes it and guest code reads it.
  uint32_t secondaryTable() const {
    return core_->mem_r32(base_ + kSecondaryTable);
  }
  void setSecondaryTable(uint32_t v) const {
    core_->mem_w32(base_ + kSecondaryTable, v);
  }

  // +0x10 u32 — runtime scratch. init zeroes it; nothing in this interpreter reads it back.
  uint32_t runtimeScratch() const {
    return core_->mem_r32(base_ + kRuntimeScratch);
  }
  void clearRuntimeScratch() const {
    core_->mem_w32(base_ + kRuntimeScratch, 0u);
  }

  // +0x46 u8 — init stamps 0xFF here. NOTHING in this interpreter reads it. The same offset is
  // `Engine::walkStart`'s animation-mode byte and `advanceGauge`'s count-up/count-down selector, so
  // the name here names only the write and says the read is somebody else's field.
  void stampInitMarker() const {
    core_->mem_w8(base_ + kInitMarker, kInitMarkerByte);
  }

  // ---- the extended-entry block (+0x64..+0x6A), loaded only for a `kFlagHasExtraBlock` entry --
  //
  // This is four halfwords, and it does NOT mean the same thing to every opcode that reads it.
  uint16_t extraHalfword0() const {
    return core_->mem_r16(base_ + kExtraBlock);
  }
  void setExtraHalfword0(uint16_t v) const {
    core_->mem_w16(base_ + kExtraBlock, v);
  }
  uint16_t extraHalfword1() const {
    return core_->mem_r16(base_ + kExtraBlock + 2u);
  }
  void setExtraHalfword1(uint16_t v) const {
    core_->mem_w16(base_ + kExtraBlock + 2u, v);
  }
  uint16_t extraHalfword2() const {
    return core_->mem_r16(base_ + kExtraBlock + 4u);
  }
  void setExtraHalfword2(uint16_t v) const {
    core_->mem_w16(base_ + kExtraBlock + 4u, v);
  }
  uint16_t extraHalfword3() const {
    return core_->mem_r16(base_ + kExtraBlock + 6u);
  }
  void setExtraHalfword3(uint16_t v) const {
    core_->mem_w16(base_ + kExtraBlock + 6u, v);
  }

  // op36 reuses halfword 0 twice over: the HIGH BYTE holds the requested step count before the move
  // is initialised, and the whole halfword is then overwritten with the solved step divisor. The
  // two names below are the two readings, so a reader cannot mistake the divisor for the request.
  // The request is read SIGNED: the guest sign-extends the high byte, so 0x80 means -128 steps.
  int8_t requestedStepCount() const {
    return static_cast<int8_t>(extraHalfword0() >> 8);
  }
  uint16_t stepDivisor() const {
    return extraHalfword0();
  }
  void setStepDivisor(uint16_t v) const {
    setExtraHalfword0(v);
  }
  // op36's second turn target angle.
  uint16_t turnAngle2() const {
    return extraHalfword1();
  }
  void setTurnAngle2(uint16_t v) const {
    core_->mem_w16(base_ + kExtraBlock + 2u, v);
  }
  // op36's per-move event pair: a flags-word ADDRESS and a packed (sound id, pitch) argument. The
  // first is the ADDRESS of the word, not its value — op36 hands the address to the event gate,
  // which then reads and writes the word in place.
  uint32_t eventFlagsPointer() const {
    return base_ + kExtraBlock + 4u;
  }
  int16_t eventPackedArg() const {
    return static_cast<int16_t>(core_->mem_r16(base_ + kExtraBlock + 6u));
  }

  // ---- the generic actor fields the movement opcodes borrow ---------------------------------------

  int16_t positionX() const {
    return static_cast<int16_t>(core_->mem_r16(base_ + kPositionX));
  }
  void setPositionX(int16_t v) const {
    core_->mem_w16(base_ + kPositionX, static_cast<uint16_t>(v));
  }
  int16_t positionY() const {
    return static_cast<int16_t>(core_->mem_r16(base_ + kPositionY));
  }
  void setPositionY(int16_t v) const {
    core_->mem_w16(base_ + kPositionY, static_cast<uint16_t>(v));
  }
  int16_t positionZ() const {
    return static_cast<int16_t>(core_->mem_r16(base_ + kPositionZ));
  }
  void setPositionZ(int16_t v) const {
    core_->mem_w16(base_ + kPositionZ, static_cast<uint16_t>(v));
  }

  // op36's solved target-minus-self vector, in 16-bit signed units.
  int16_t deltaX() const {
    return static_cast<int16_t>(core_->mem_r16(base_ + kDeltaX));
  }
  int16_t deltaY() const {
    return static_cast<int16_t>(core_->mem_r16(base_ + kDeltaY));
  }
  int16_t deltaZ() const {
    return static_cast<int16_t>(core_->mem_r16(base_ + kDeltaZ));
  }
  void setDeltas(int16_t dz, int16_t dy, int16_t dx) const {
    core_->mem_w16(base_ + kDeltaZ, static_cast<uint16_t>(dz));
    core_->mem_w16(base_ + kDeltaY, static_cast<uint16_t>(dy));
    core_->mem_w16(base_ + kDeltaX, static_cast<uint16_t>(dx));
  }

  // op36's remaining-arc counter, in Q12 (0x1000 == the whole move).
  uint16_t stepsRemaining() const {
    return core_->mem_r16(base_ + kStepsRemaining);
  }
  void setStepsRemaining(uint16_t v) const {
    core_->mem_w16(base_ + kStepsRemaining, v);
  }

  // The angle the turning opcodes step toward, in Q12 over a 4096-unit circle.
  int16_t facingAngle() const {
    return static_cast<int16_t>(core_->mem_r16(base_ + kFacingAngle));
  }
  void setFacingAngle(int16_t v) const {
    core_->mem_w16(base_ + kFacingAngle, static_cast<uint16_t>(v));
  }
  // op31 clears this companion word while it is still turning. Its meaning is NOT established —
  // it is written and cleared, never read by anything this repository owns. Named for the write.
  void clearFacingCompanion() const {
    core_->mem_w16(base_ + kFacingCompanion, 0u);
  }

  // +0x38 u32 — the owner/parent pointer op36's event pulse reads +4 of.
  uint32_t ownerPointer() const {
    return core_->mem_r32(base_ + kOwnerPointer);
  }

  // ---- the cached-tail pair the resident-leaf sweep touches ----------------------------------------

  // +0x3C/+0x40 — a linked-node head pointer and the cache slot holding a pointer one or four bytes
  // into that node's header. Both leaves do the same thing: null node is a no-op, a node whose flag
  // byte has bit 0x80 set is dead and BOTH pointers are cleared, otherwise the cache is refreshed.
  // The two leaves differ only in WHICH node header byte carries the flag and how far past it the
  // cached pointer starts — so that difference is the leaf's only argument.
  uint32_t tailNode() const {
    return core_->mem_r32(base_ + kTailNode);
  }
  uint32_t tailCache() const {
    return core_->mem_r32(base_ + kTailCache);
  }
  void clearTail() const {
    core_->mem_w32(base_ + kTailCache, 0u);
    core_->mem_w32(base_ + kTailNode, 0u);
  }
  void setTailCache(uint32_t v) const {
    core_->mem_w32(base_ + kTailCache, v);
  }

  uint32_t base() const {
    return base_;
  }

  // ---- the block's offsets, declared once, at the name ----------------------------------------------
  //
  // These are guest addresses, not this repository's: every one is `base + offset` in the guest's
  // own object struct, recovered from authenticated MAIN.EXE/overlay evidence.
  static constexpr uint32_t kRuntimeScratch = 0x10u;
  static constexpr uint32_t kPositionZ = 0x2Eu;
  static constexpr uint32_t kPositionY = 0x32u;
  static constexpr uint32_t kPositionX = 0x36u;
  static constexpr uint32_t kOwnerPointer = 0x38u;
  static constexpr uint32_t kTailNode = 0x3Cu;
  static constexpr uint32_t kTailCache = 0x40u;
  static constexpr uint32_t kStepsRemaining = 0x44u;
  static constexpr uint32_t kInitMarker = 0x46u;
  static constexpr uint32_t kDeltaZ = 0x48u;
  static constexpr uint32_t kDeltaY = 0x4Au;
  static constexpr uint32_t kDeltaX = 0x4Cu;
  static constexpr uint32_t kFacingAngle = 0x56u;
  static constexpr uint32_t kFacingCompanion = 0x58u;
  static constexpr uint32_t kExtraBlock = 0x64u;
  static constexpr uint32_t kCursor = 0x6Cu;
  static constexpr uint32_t kProgress = 0x70u;
  static constexpr uint32_t kStateFlags = 0x71u;
  static constexpr uint32_t kArgA = 0x72u;
  static constexpr uint32_t kArgB = 0x74u;
  static constexpr uint32_t kArgC = 0x76u;
  static constexpr uint32_t kPhase = 0x78u;
  static constexpr uint32_t kMatchByte = 0x79u;
  static constexpr uint32_t kSecondaryTable = 0x7Cu;

  // The byte init stamps at +0x46. It is a marker, not a state: nothing reads it back.
  static constexpr uint8_t kInitMarkerByte = 0xFFu;
  // The raw byte the advance sub-machine's stop arm stores at +0x70. It sign-extends to -1, which
  // is what stops the step loop.
  static constexpr uint8_t kProgressStoppedByte = 0xFFu;
  // Bit 0 of +0x71, the bit step() sets on a pause return.
  static constexpr uint8_t kFlagBitPaused = 0x01u;
  // Bit 1 of +0x71: init's `kFlagValidEntry` state, per the guest's own flag scan.
  static constexpr uint8_t kFlagBitValidEntry = 0x02u;
  // Bit 2 of +0x71: init's `kFlagSetsFlagBit2` state.
  static constexpr uint8_t kFlagBitFromOpcode = 0x04u;

  // A node header's flag byte carries bit 0x80 when the node is dead. The cached-tail leaves check
  // it at node+0 and node+3 respectively, and both store the cache pointer one byte past the flag.
  static constexpr uint8_t kNodeDeadFlagBit = 0x80u;
  static constexpr uint32_t kTailCachePastFlagLo = 1u;
  static constexpr uint32_t kTailCachePastFlagHi = 4u;
  static constexpr uint32_t kTailFlagAtLo = 0u;
  static constexpr uint32_t kTailFlagAtHi = 3u;

  // +0x79 — the match byte `matchesActiveByKind` compares. The offset's other name, "byte 121", is
  // how the RE recorded it; the number and the offset are the same thing.
  static constexpr uint32_t kKindSelectorOffset = kArgA;
  static constexpr uint32_t kMatchByteOffset = kMatchByte;

private:
  Core *core_;
  uint32_t base_;
};

} // namespace tomba::scene
