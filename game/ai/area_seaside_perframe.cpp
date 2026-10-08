// game/ai/area_seaside_perframe.cpp — seaside per-frame update fn FUN_80113C5C (A00 overlay).
//
// This is the PER-AREA PER-FRAME HANDLER `Engine::modePerFrameDispatch` fires for the seaside
// field (area 0). The 24-entry function-pointer table at 0x8009D1D4 (indexed by 0x800BF870) picks
// which of 24 area-specific per-frame handlers runs; area[0] = seaside = this function. It reads
// the master G MODE byte at G+0 (= 0x800E7E80, u8) to gate which sub-behaviors run, then walks
// the aux render list at *0x1F800154 (count *0x1F80015C) for per-item collision-style processing.
//
// ★ G IS TOMBA'S NODE (verified this pass — see docs/port-progress.md's "KEY FINDING"). So this
// function is the "seaside Tomba per-frame tick" — it runs Tomba's mode-specific pre-update
// (FUN_80022760(G) or FUN_8011334C(G) or FUN_801130C4(G) depending on G[0] mode byte), a fixed
// mid-update trio (FUN_80022554 / FUN_80113700 / FUN_801138E8), a variable aux-list walk
// (FUN_80112A60), and a fixed post-update pair (FUN_80112C0C / FUN_80112F14) + FUN_8010E904(G).
//
// STATE FLOW (mode = G[0]):
//   mode == 0 or 6            : ONLY the fixed trio + aux walk + post pair.
//   mode == 2                 : if *0x800E7FD8 < 2 (32-bit UNSIGNED — it is the attach ptr,
//                               so this means "holding nothing") also run FUN_8011334C(G), then falls
//                               through to the fixed trio + aux walk + post pair.
//   any other value           : run FUN_80022760(G) + FUN_801130C4(G), then falls through to the
//                               "case 2" branch (may also fire FUN_8011334C(G)) and onward.
//
// AUX-LIST WALK — pull items in reverse from the shared aux render list at *0x1F800154 (max
// count comes from *0x1F80015C, stored to DAT_1F800183 as loop counter). For each item ptr:
//   * skip if item[0] & 2 (already-processed guard)
//   * item[2] is the type: 0/4/7 → FUN_80112A60(item) (default interaction leaf); type 1 →
//     the same leaf ONLY IF item[3] == 1; any other type → skip. (The item[3] guard belongs to
//     type 1, not type 4 — verified against the guest at LAB_80113d7c, 2026-07-21. The code below
//     was already right; this comment was not.)
//
// SUB-BEHAVIOR LEAVES kept substrate (top-down: own the CONTROL FLOW here, descend into each
// callee's own tree only after their parent (this fn) is on the live path):
//     0x80022760, 0x80022554, 0x801130C4, 0x80113700, 0x801138E8, 0x8011334C,
//     0x80112A60, 0x80112C0C, 0x80112F14, 0x8010E904.
//
// Ghidra decomp scratch/decomp/seaside_perframe_113c5c.c.

#include "behaviors.h"
#include "cfg.h"
#include "core.h"
#include "core/engine/engine.h"
#include "core/entry/game_ctx.h"
#include "guest_call.h"
#include "player/actor_tomba.h" // eng(c).actorTomba.interactWalk (FUN_80022760)

namespace {

constexpr uint32_t G = 0x800E7E80u;                     // master G block (= Tomba's node)
constexpr uint32_t AUX_LIST_HEAD_SPAD = 0x1F800154u;    // ptr to aux-list u32 array
constexpr uint32_t AUX_LIST_COUNT_SPAD = 0x1F80015Cu;   // u8 count of items in that array
constexpr uint32_t AUX_WALK_COUNTER_SPAD = 0x1F800183u; // per-frame counter DAT_1F800183

// Guest addresses of sub-behavior leaves this handler dispatches.
constexpr uint32_t LEAF_G_TOMBA_TICK_22760 = 0x80022760u; // Tomba tick on G (default mode)
// (LEAF_G_POST_130C4 = FUN_801130C4 → moved to ActorTomba::postInteractWalk)
constexpr uint32_t LEAF_G_MODE2_1334C = 0x8011334Cu;      // mode-2-only tick on G (gated by 800E7FD8<2)
constexpr uint32_t LEAF_TRIO_22554 = 0x80022554u;         // fixed trio 1/3 (no-arg engine tick)
constexpr uint32_t LEAF_TRIO_113700 = 0x80113700u;        // fixed trio 2/3
constexpr uint32_t LEAF_TRIO_1138E8 = 0x801138E8u;        // fixed trio 3/3
constexpr uint32_t LEAF_AUX_ITEM_112A60 = 0x80112A60u;    // per-aux-item leaf (interaction)
constexpr uint32_t LEAF_POST_112C0C = 0x80112C0Cu;        // fixed post 1/2
constexpr uint32_t LEAF_POST_112F14 = 0x80112F14u;        // fixed post 2/2
constexpr uint32_t LEAF_G_POSTFRAME_10E904 = 0x8010E904u; // Tomba post-frame tick on G

// Dispatch a G-arg sub-leaf.
inline void gLeaf(Core *c, uint32_t leaf) {
  c->r[4] = G;
  psx::cpu::dispatchGuestToReturn0(*c, leaf, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
}

// Walk the aux render list, dispatching FUN_80112A60(item) per item type-gate.
// Returns after the counter reaches 0 or an item's dispatch decides not to continue.
inline void aux_list_walk(Core *c) {
  const uint32_t listPtr = c->mem_r32(AUX_LIST_HEAD_SPAD);
  c->mem_w8(AUX_WALK_COUNTER_SPAD, c->mem_r8(AUX_LIST_COUNT_SPAD));
  uint32_t cursor = listPtr;
  while (c->mem_r8(AUX_WALK_COUNTER_SPAD) != 0) {
    const uint32_t item = c->mem_r32(cursor);
    c->mem_w8(AUX_WALK_COUNTER_SPAD, (uint8_t)(c->mem_r8(AUX_WALK_COUNTER_SPAD) - 1));
    cursor += 4;
    if (c->mem_r8(item) & 2) {
      continue; // already-processed guard
    }
    const uint8_t typ = c->mem_r8(item + 2);
    if (typ == 1) { // guarded skip on subtype==1
      if (c->mem_r8(item + 3) != 1) {
        continue;
      }
      // fall through into leaf dispatch (matches the guest instruction path's LAB_80113D8C join point)
    } else if (typ >= 2) {
      if (typ != 4 && typ != 7) {
        continue; // only types 4/7 dispatch beyond
      }
    } // typ == 0 → dispatch below
    c->r[4] = item;
    psx::cpu::dispatchGuestToReturn0(*c, LEAF_AUX_ITEM_112A60, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
  }
}

} // namespace

void Behaviors::areaSeasidePerframe(Core *c) {
  psx::cpu::dispatchGuestToReturn1(*c, 0x8002288Cu, G, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);

  const uint8_t mode = c->mem_r8(G);
  const bool defaultMode = !(mode == 0 || mode == 2 || mode == 6);
  bool runMode2Tick = (mode == 2);

  if (defaultMode) {
    eng(c).actorTomba.interactWalk();     // native FUN_80022760 (Tomba interaction walk)
    eng(c).actorTomba.postInteractWalk(); // native FUN_801130C4 (default-mode post-tick)
    runMode2Tick = true;                  // guest instruction path falls through into `case 2`
  }
  // G+0x158 is the RIDE/ATTACH POINTER — the object Tomba is currently holding onto (written by
  // FUN_80057a68 as `player[0x158] = FUN_80024548(player, 0)`), 0 when he is attached to nothing.
  // The guest gate is a 32-bit UNSIGNED test (0x80113CC0: `lw v0,0x158(s1)` + `sltiu v0,v0,2`), so it
  // asks "is the attach slot empty?" — a real object pointer (0x800Fxxxx) is a huge unsigned value
  // and the gate is FALSE.
  //
  // This was ported as a SIGNED 16-BIT read, which inverts the answer exactly when Tomba is attached:
  // (int16)0x800FB960 = -18080 < 2 is TRUE, so the sub-tick fired precisely in the state where the
  // guest suppresses it. Unattached (slot == 0) both agree, which is why it went unnoticed — the bug
  // is invisible until something is grabbed. Found chasing kanban #8 (the water-pump seesaw not
  // sinking under Tomba's weight); verified against the instruction stream, not the decompiler, which
  // rendered the operand as a pointer comparison.
  if (runMode2Tick && c->mem_r32(0x800E7FD8u) < 2u) {
    gLeaf(c, LEAF_G_MODE2_1334C); // mode-2 sub-tick (gated)
  }

  // `cullq` — sample the CONTACT-STAMP GATE at the only phase where it means anything. FUN_80113700
  // (LEAF_TRIO_113700) is the contact stamper, and its whole body is `if (*(u8)0x1F800144 != 0)` —
  // queue A's count. A frame-boundary RAM dump cannot answer whether that gate is open: the scratchpad
  // is reused outside the render window, so an empty queue there is indistinguishable from "filled and
  // consumed inside it". Sampling HERE, immediately before the dispatch, is what distinguishes them.
  // Also grabs Cull::decide's own queue-assignment gate (0x1F800080) and the other two queue counts.
  if (cfg_dbg("cullq")) {
    // Also walk the stamper's OUTER list (head at *(0x800F2738), chained by +0x24) and report whether
    // a given object is on it — FUN_80113700 only stamps objects it actually visits, and it skips any
    // whose +0x2b is already non-zero. PSXPORT_CULLQ_OBJ=<hex> names the object to look for.
    uint32_t want = (uint32_t)cfg_int("PSXPORT_CULLQ_OBJ", 0);
    uint32_t head = c->mem_r32(0x800F2738u), node = head;
    int len = 0, hit = -1;
    while (node >= 0x80000000u && node < 0x80200000u && len < 256) {
      if (want && node == want) {
        hit = len;
      }
      node = c->mem_r32(node + 0x24u);
      len++;
      if (node == head) {
        break; // defensive: cyclic chain
      }
    }
    cfg_logf("cullq",
             "pre-trio: qA=%u qB=%u qC=%u gate=%08X mode=%u outer{head=%08X len=%d idx=%d}",
             c->mem_r8(0x1F800144u),
             c->mem_r8(0x1F800150u),
             c->mem_r8(0x1F80015Cu),
             c->mem_r32(0x1F800080u),
             mode,
             head,
             len,
             hit);
  }

  // Fixed mid-update trio (fires in every mode 0/2/6 + default's fall-through).
  psx::cpu::dispatchGuestToReturn0(
      *c, LEAF_TRIO_22554, psx::cpu::ExecutionBudget::currentTurn(*c), __func__); // no-args engine tick
  psx::cpu::dispatchGuestToReturn0(
      *c, LEAF_TRIO_113700, psx::cpu::ExecutionBudget::currentTurn(*c), __func__); // no-args
  psx::cpu::dispatchGuestToReturn0(
      *c, LEAF_TRIO_1138E8, psx::cpu::ExecutionBudget::currentTurn(*c), __func__); // no-args

  aux_list_walk(c);

  // Fixed post-update pair + Tomba post-frame tick.
  psx::cpu::dispatchGuestToReturn0(*c, LEAF_POST_112C0C, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
  psx::cpu::dispatchGuestToReturn0(*c, LEAF_POST_112F14, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
  eng(c).actorTomba.postFrameWaterCheck(); // native FUN_8010E904 (Tomba post-frame water/sea)
}
