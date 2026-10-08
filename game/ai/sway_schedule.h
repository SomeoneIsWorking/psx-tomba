// game/ai/sway_schedule.h — the SWAY SCHEDULE: a field object that rocks back and forth, whose
// rocking WINDS DOWN in stages as the area's scripted event sequence advances.
//
// See sway_schedule.cpp for the identification evidence. This header holds only the two typed lenses
// the leaf needs — the object node, and the two save-block bytes the schedule waits on — plus a named
// constant per field, so the body reads as "the sequence has reached step 7, so start slowing the
// rocking down" rather than as a chain of mem_r8(0x800BF870 + 0x170).
//
// The write accessors are deliberately ONE-LINERS: tools/dynamic differential evidence harvests a lens setter's
// mem_wN width by regex and counts `self.setRate(v)` as exactly the store it performs, but ONLY for
// lenses defined in a game/**/*.h header whose setters are single statements. A setter that grew a
// second statement or a nested brace would silently stop counting, and the gate would then compare a
// short store sequence against the oracle's full one (see docs/findings/tooling.md).
#pragma once
#include "core.h"
#include <cstdint>

// ------------------------------------------------------------------------------------------------
// The rocking object's node. Standard object-node shape (game/object/actor.h): +3 is the per-object
// TYPE index, +5 the sub-state/jump-table selector Actor calls `triggerSub`.
//
// +0x50 IS NOT actor.h's `accelY` FOR THIS FAMILY. The three sway bodies this leaf tails into
// (0x8012CF4C / 0x8012D05C / 0x8012D16C) use +0x4A as a 4096-unit ANGLE accumulator (`& 0xFFF` every
// tick) and add +0x50 to it each frame — and they early-out doing nothing at all when +0x50 is zero.
// So in this family +0x4A/+0x50 are an oscillator phase and its per-frame step, not actor.h's
// velY/accelY. Offsets in the 0x48..0x5A block are reused per object family; this lens names them
// only for the family measured here.
namespace swaynode {
constexpr uint32_t kVariant = 0x03; // u8  — which of the three sway bodies runs (Actor::type)
constexpr uint32_t kPhase = 0x05;   // u8  — schedule phase, 0..5, the jump-table selector
constexpr uint32_t kRate = 0x50;    // u16 — the oscillator's per-frame phase step ("how fast it rocks")
} // namespace swaynode

struct SwayNode {
  Core *mCore;
  uint32_t mAt;

  uint8_t variant() const {
    return mCore->mem_r8(mAt + swaynode::kVariant);
  }
  uint8_t phase() const {
    return mCore->mem_r8(mAt + swaynode::kPhase);
  }
  // The guest reads the rate BOTH ways in the same breath — signed for the "have we wound down far
  // enough" compare, unsigned for the subtraction that produces the next rate. Keep both.
  int16_t rate() const {
    return mCore->mem_r16s(mAt + swaynode::kRate);
  }
  uint16_t rate_u() const {
    return mCore->mem_r16(mAt + swaynode::kRate);
  }

  void setPhase(uint8_t v) const {
    mCore->mem_w8(mAt + swaynode::kPhase, v);
  }
  void setRate(uint16_t v) const {
    mCore->mem_w16(mAt + swaynode::kRate, v);
  }
};

// ------------------------------------------------------------------------------------------------
// The two bytes of the save/state block this schedule waits on. The block base 0x800BF870 is the
// same one game/items/inventory.h, game/scene/sop.cpp and game/ai/beh_toy_spawn_family.cpp already
// name (there as `GBASE` / `kAreaByte`).
namespace areaseq {
constexpr uint32_t kBlockBase = 0x800BF870u;
// +0x48: a "this scripted sequence is already over" sentinel. Read-only here; 0xFF means done.
// game/ai/beh_two_child_steer.cpp (guest 0x80131D08) tests the SAME byte for the SAME 255 and also
// uses it to skip a state forward, so the sentinel reading is not local to this leaf.
constexpr uint32_t kFinishedFlag = kBlockBase + 0x48u; // 0x800BF8B8
// +0x170: the area's scripted EVENT-STEP counter. Incremented by exactly 1 at guest 0x80117084
// (authenticated executable/overlay evidence, inside overlay guest 0x80116F64) when a scene event completes, and
// compared against ascending thresholds by several already-owned behaviours (6/16/28 in
// beh_area_threshold_ptr_swap.cpp, 20 elsewhere). That is what makes it a progress counter rather
// than a mode byte: one writer, +1, many ascending readers.
constexpr uint32_t kEventStep = kBlockBase + 0x170u; // 0x800BF9E0

constexpr uint8_t kFinishedSentinel = 255;
} // namespace areaseq

struct AreaSequence {
  Core *mCore;

  uint8_t eventStep() const {
    return mCore->mem_r8(areaseq::kEventStep);
  }
  bool isFinished() const {
    return mCore->mem_r8(areaseq::kFinishedFlag) == areaseq::kFinishedSentinel;
  }
};

// ------------------------------------------------------------------------------------------------
class SwaySchedule {
public:
  // 0x8012D27C — the per-type tick of behaviour FUN_8012D404 (game/ai/beh_cull_tick_render.cpp),
  // called with a0 = the object node once the object survives the frame's cull.
  static void advanceRateThenSway(Core *c);

  static void registerOverrides();
};
