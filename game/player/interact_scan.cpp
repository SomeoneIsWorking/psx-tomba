// class InteractScan — "is the player activating something right now?"
//
// Guest FUN_80024794, RE'd 2026-07-21 (Ghidra headless + guest 0x80024794). Leaf, no guest frame.
//
// WHAT IT IS. This is the INTERACTION SCANNER. Once per call, while Tomba is in an interact-capable
// action state, it walks the scratchpad candidate list and looks for one object that is already an
// in-range candidate. If it finds one it ACTIVATES it — and that activation is what a save sign, a
// door or a pickup reacts to on its next tick.
//
// It is worth being precise about the byte it writes, because "obj+0x2b" was carried for a long time
// under the name `subFlag`, which says nothing. It is an INTERACTION STATE with three known values:
//
//     0  none          — not a candidate
//     1  in-range      — this object is offering an interaction. Set by the object's OWN handler:
//                     there is no single proximity pass (a scan of authenticated executable/overlay evidence finds 97
//                     distinct writers of the literal 1 into +0x2b, spread across the per-area overlays)
//     3  ACTIVATED     — the player has just acted on it; set HERE, consumed by the object's handler
//
// The consuming handler tests for 3 inside its sub-state switch and clears it in its render tail.
// Clearing it any earlier destroys the activation and the object latches forever — that was the
// save-sign softlock (kanban #5, docs/findings/scene.md). Knowing what the byte MEANS is what makes
// that obvious; knowing only that it is called `subFlag` does not.
//
// It also publishes the VERB at 0x800BF840 — which activation just happened — chosen by object type:
// 0x84 for the sign/plaque family (types 0x0E/0x0F/0x39), 0x85 for everything else it accepts.
#include "core.h"
#include "core/entry/game_ctx.h"
#include "core/overrides/native_override_catalog.h"
#include "game.h"
#include "object/actor.h"
#include <cstdint>

namespace {

// The scratchpad candidate list the proximity pass leaves for us: an array of object pointers and a
// count. Same addresses the guest reads (0x1F80014C / 0x1F800152); 0x1F800182 is the guest's own
// working counter, mirrored because it is guest-visible state, not a local.
// 0x1F80014C holds a POINTER to the candidate array — it is not the array itself. Ghidra renders it
// as `puVar4 = puRam1f80014c`, which reads like a base address; gen makes the indirection explicit
// (`r5 = mem_r32(0x1F80014C); r4 = mem_r32(r5)`). Getting this wrong scans arbitrary memory and
// writes the interaction state into whatever it lands on.
constexpr uint32_t CANDIDATE_LIST_PTR = 0x1F80014Cu; // u32 -> array of candidate object pointers
constexpr uint32_t CANDIDATE_COUNT = 0x1F800152u;    // u8  number of candidates
constexpr uint32_t SCAN_CURSOR = 0x1F800182u;        // u8  guest's live loop counter
constexpr uint32_t PENDING_VERB = 0x800BF840u;       // u8  which activation just happened

// Tomba's action state lives at player+0x14A. States 6 and 7 are the two in which an activation may
// start; every other state ignores the button entirely.
constexpr uint32_t PLAYER_ACTION_STATE = 0x14Au;
constexpr uint8_t ACTION_STATE_LO = 6, ACTION_STATE_COUNT = 2;

// Object header fields this scan reads. `alive` bit 0 of +0, class at +0x0C, type at +2.
constexpr uint8_t CLASS_INTERACTABLE = 4;

// The object types this scan will activate. Kept as an explicit list, in guest order, because it is
// exactly the guest's chain of equality tests — a range would be a guess.
bool activatableType(uint8_t type) {
  return (uint8_t)(type - 0x0E) < 2 // 0x0E, 0x0F — sign / plaque family
         || type == 0x39 || type == 0x32 || type == 0x59 || type == 0x4B || type == 0x4F || type == 0x66 ||
         type == 0x09 || type == 0x33;
}

// The sign/plaque family reports a different verb than everything else.
bool signFamily(uint8_t type) {
  return (uint8_t)(type - 0x0E) < 2 || type == 0x39;
}

} // namespace

// FUN_80024794(player) -> 1 if something was activated this call, else 0.
uint32_t interact_scan(Core *c) {
  const uint32_t player = c->r[4];

  const uint8_t action = c->mem_r8(player + PLAYER_ACTION_STATE);
  if ((uint8_t)(action - ACTION_STATE_LO) >= ACTION_STATE_COUNT) {
    return 0; // not able to activate
  }

  uint32_t ptrs = c->mem_r32(CANDIDATE_LIST_PTR); // deref: the array, not the slot holding it
  uint8_t remaining = c->mem_r8(CANDIDATE_COUNT);

  // The guest keeps its loop counter in guest memory (0x1F800182) rather than a register, and other
  // code can observe it, so mirror the write rather than using a C++ local.
  c->mem_w8(SCAN_CURSOR, remaining); // gen seeds the cursor once, before the loop
  if (remaining == 0) {
    return 0;
  }

  for (;;) {
    const uint32_t obj = c->mem_r32(ptrs);
    // gen re-reads the cursor from guest memory each pass rather than keeping it in a register.
    remaining = (uint8_t)(c->mem_r8(SCAN_CURSOR) - 1);
    c->mem_w8(SCAN_CURSOR, remaining);
    ptrs += 4;

    Actor a(c, obj);
    const uint8_t type = c->mem_r8(obj + 2);
    if (c->mem_r8(obj + 0x0C) != CLASS_INTERACTABLE) {
      if (remaining == 0) {
        return 0;
      }
      continue;
    }
    if (!activatableType(type)) {
      if (remaining == 0) {
        return 0;
      }
      continue;
    }
    if ((a.alive() & 1) == 0) {
      if (remaining == 0) {
        return 0;
      }
      continue;
    }
    if (a.interactState() != Actor::kInteractInRange) {
      if (remaining == 0) {
        return 0;
      }
      continue;
    }

    a.setInteractState(Actor::kInteractActivated); // 1 -> 3: the object acts on this
    c->mem_w8(PENDING_VERB, signFamily(type) ? 0x84 : 0x85);
    return 1;
  }
}

static void ov_interact_scan(Core *c) {
  c->r[2] = interact_scan(c);
}

void interact_scan_install() {
  // The SETTER is required, not optional. This function's only caller reaches it through the direct
  // `guest 0x80024794(c)` thunk (authenticated executable/overlay evidence), not through typed runtime address dispatch
  // — so registering without a setter leaves the native body registered and NEVER RUN (ovhit: "registered but
  // unreached"), with the substrate quietly still doing the work.
  tomba::native::declareOverride(0x80024794u, "InteractScan::scan", ov_interact_scan);
}
