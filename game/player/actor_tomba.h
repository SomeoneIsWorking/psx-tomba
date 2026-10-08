// game/player/actor_tomba.h — class ActorTomba: the PC-native Tomba actor.
//
// G IS TOMBA'S NODE — the master block at 0x800E7E80 that pool_init's sub-init FUN_8007A810 zeroes
// (0x184 bytes). Tomba is not dispatched by walkAll / walkList2 / walkAux; his state lives on that
// block, which the lens in game/player/tomba_state.h names. Per frame he ticks through the per-area
// callback Engine::modePerFrameDispatch fires (seaside area 0 = area_seaside_perframe, which invokes
// interactWalk).
//
// One instance per Core, embedded as `Core::engine::actorTomba`. What touching an item MEANS is
// ActorInteraction (game/player/actor_interaction.{h,cpp}); this class owns the two walks that choose
// which items to hand it, the G-block driver, growth, motion and the outer-transition/asset gating.
// Addresses, field layouts and the per-cluster call graph are in docs/engine_re.md's "ActorTomba
// G-block" section and docs/codemap.md.
#pragma once

#include "player/actor_interaction.h"

#include <cstdint>

class Core;
class Game;

namespace tomba::player {

class ActorTomba {
public:
  ActorTomba() = default;

  // The CPU whose guest memory every method reaches. Owned by Engine and wired once per Core.
  Core *core = nullptr;

  // Guest address of Tomba's node = the master G block.
  static constexpr uint32_t G_ADDR = 0x800E7E80u;

  // ----------------------------------------------------------------------------
  // Per-frame
  // ----------------------------------------------------------------------------
  // interactWalk — guest FUN_80022760. Walks the aux render list at *0x1F800154 and dispatches
  //   per-item collision checks against G. Early-outs on the "Tomba disabled" gates
  //   (G+0x16E == 0 / 0x800BF80D != 0 / G+0x17E & 0x200). Type-keyed dispatch to
  //   `proximityCheck` (types 0/1/2/3/7), `type4GuardedCheck` (type 4 subtype 2),
  //   `subHitboxCheck` (type 6).
  void interactWalk();

  // postInteractWalk — guest FUN_801130C4. Runs immediately after `interactWalk` (default-mode
  //   branch only). Walks a DIFFERENT list — the *0x1F80013C / *0x1F800144 pair — and per item
  //   dispatches by type:
  //     item[0xC] == 9 (special) → FUN_80111304(G, item) IF !(G+0x17E & 0x8200), continue
  //     item[2] == 3             → FUN_8010E258(G, item)
  //     item[2] == 4             → detailed guarded state-transition (see cpp)
  //     item[2] == 7             → FUN_800235A0(G, item)
  //     item[2] == 8             → FUN_800205CC(G, item)
  //     item[2] in {0xF, 0x14, 0x56} → FUN_80020364(G, item, 0)
  //     item[2] == 0x13          → FUN_8010EA80(G, item)
  //     item[2] == 0x2F          → FUN_80020364(G, item, 2)
  //   Sub-handler leaves stay substrate. Ghidra decomp scratch/decomp/fun_801130c4.c.
  void postInteractWalk();

  // ----------------------------------------------------------------------------
  // Growth / shrink (Tomba's transformation state)
  // ----------------------------------------------------------------------------
  // growthStep(mode) — guest FUN_80057DC0. Toggles G+0x17E bit 0x8000 (grown flag) with a
  //   G+0x32 Y-position ±0x46 compensation so his feet stay on the ground; then rescales the
  //   bounds/physics fields at G+0xB8/BA/BC (Q12 world scale), G+0x80/82/84/86 (bounding box),
  //   G+0x62/64/66/68 (physics constants), and DAT_800E802A by `1/(mode+1)`. Called by
  //   Engine::gStateMutate cases 6 (grow, mode=1) and 7 (shrink, mode=0).
  void growthStep(int32_t mode);

  // ----------------------------------------------------------------------------
  // Movement
  // ----------------------------------------------------------------------------
  // velocityIntegrate(suppressY) — guest FUN_80056B48. Integrates a per-frame velocity into
  //   Tomba's 16.16 master position: posX += dirX*speed (G+0x2C += (s16 G+0x48)*(s16 G+0x44)),
  //   posZ similarly (G+0x34 / G+0x4C), and posY (G+0x30 / G+0x4A) unless `suppressY` is set.
  //   Tail: if flag363 (G+0x16B) == 0 AND flag97 (G+0x61) == 0, dispatch the stop/settle helper
  //   FUN_80054650(G, 0); otherwise clear flag95 (G+0x5F) bit 0x04.
  void velocityIntegrate(bool suppressY);

  // settleStep(mode) — guest FUN_80054650. The "stop/settle helper" `velocityIntegrate` tail-
  //   dispatches when Tomba isn't blocked (flag363 & flag97 clear). Sets DAT_1F800258 = 0, clears
  //   G+0x5F bit 0x04, and if G+0x16B (flag363) is 0 runs a two-way grid probe (FUN_8004954C —
  //   substrate) with a `sVar3 = probe_offset / 2` derived from G+0x62 or from the item hooked
  //   at G+0x10 (obj+0x86 - obj+0x84 - (G+0x32 - obj+0x32)). On probe hit stamps G+0x60=1 and
  //   G+0x5F = (flag from G+0x149 bit 0x4 → G+0x147 else G+0x149&1) + 4, returns 1. Sets DAT_
  //   1F800258 marks a "sink" fallback that flips G+0x5F to (5 - G+0x147). Returns 1 on hit else 0
  //   (via c->r[2]). Ghidra decomp scratch/decomp/footstep_hunt.c.
  uint32_t settleStep(int32_t mode);

  // postFrameWaterCheck() — guest FUN_8010E904 (final call in area_seaside_perframe).
  //   Water/sea gating: if seaside water mode 0x800BF816 is engaged, snap Tomba's Y (G+0x32) to
  //   the water surface (0x800BF812 minus G+0x62); otherwise, if not paused, run FUN_8010E408(G)
  //   (substrate). Also fires the area-exit trigger (0x1F800137 / 0x800BF80F / 0x800BF83A /
  //   0x800BF839 / 0x1F800236) when Tomba is off-map (G+0x32 < -0xE74 AND G+0x36 < 0x1451) in
  //   water-mode 2. NO SFX. Ghidra decomp scratch/decomp/tomba_postframe_10e904.c.
  void postFrameWaterCheck();

  // growthYSnap() — guest FUN_80022C78, the water-splash leaf postFrameWaterCheck fires. A leaf with
  //   no guest-stack frame: it resets Tomba's walk/collision latches and, on dry unfrozen land, re-snaps
  //   G+0x32 by the growth offset 0x46/0x8C that growthStep's Y compensation uses, so the grow/shrink
  //   transform settles onto his actual ground height. ActorInteraction::type8Interact reuses this
  //   exact tail for the "just left a growth transition" case.
  void growthYSnap();

  // Wire the item-kind leaves, growthYSnap and frameTick into the global
  // override registry — postInteractWalk's own typed runtime address dispatch(c, LEAF_TYPE_*) call sites are the
  // ONLY reachers of the 4 interactWalk addresses, and the native frameStartTickFaithful's
  // typed runtime address dispatch(c, 0x8005950C) is the only core-A reacher of frameTick (the sole direct
  // guest 0x8005950C caller is guest 0x80059D28 = the substrate frameStartTickFaithful, core-B only),
  // so a typed runtime address dispatch-only registration (no tomba::native::declareOverride setter) is sufficient for
  // all 5.
  static void registerOverrides(Game *game);

private:
  // EngineOverrideFn-shaped trampolines for registerOverrides (need class access to the private
  // sub-handlers below).
  static void ov_stepModeInteract(Core *c);
  static void ov_type8Interact(Core *c);
  static void ov_type7Interact(Core *c);
  static void ov_growthYSnap(Core *c);
  static void ov_frameTick(Core *c);
  static void ov_turnBiasCompute(Core *c);
  static void ov_outerTransitionGate(Core *c);
  static void ov_outerTransitionCommit(Core *c);
  static void ov_assetReady(Core *c);
  // tomba::native::declareOverride setter trampolines (verification-gated — see .cpp banner).
  static void gov_turnBiasCompute(Core *c);
  static void gov_outerTransitionGate(Core *c);
  static void gov_outerTransitionCommit(Core *c);
  static void gov_assetReady(Core *c);
  static void gov_mode0ActionGate(Core *c);

  ActorInteraction interaction_{*this}; // what touching an item means; the walks dispatch into it

public:
  // ----------------------------------------------------------------------------
  // Per-frame G-block driver (0x8005950C cascade) — RE'd 2026-07-08 wide-RE pass. See
  // docs/engine_re.md's "ActorTomba G-block" section for the full call-graph writeup of the
  // ~99-function 0x80052xxx-0x8005Fxxx region this drives, and docs/codemap.md for addresses.
  // ----------------------------------------------------------------------------
  // frameTick() — guest FUN_8005950C. Tomba's own per-frame G-block driver, called directly
  //   from the already-native Engine::frameStartTick/frameStartTickFaithful (game/core/
  //   engine.cpp, the `default: target = 0x8005950Cu` arm of its mode-keyed dispatch) whenever
  //   0x800BF870 isn't one of the 4 area-specific overlay modes (2/3/7/20). Dispatches on the
  //   OUTER state byte G+4 (0-7; >=8 is unreachable — the guest's own jump table has exactly 8
  //   entries and any value >=8 returns immediately with no-op):
  //     0 = INIT       -> enterOuterState0 (still-substrate FUN_80058648, mode=0)
  //     1 = ACTIVE     -> turnBiasCompute + FUN_80058918 (mode-N dispatch table A, still
  //                       substrate) + matrix-compose (FUN_800597AC, still substrate) +
  //                       outerTransitionCommit(mode=0)
  //     2 = COMMITTING -> FUN_80067CA4 (still substrate) + matrix-compose
  //     3 = (unused jump-table slot) -> no-op
  //     4 = ACTIVE_ALT -> same shape as case 1, but the "turn-suppress mask" pair
  //                       (0x800ECF54/0x800E7E68) is restored from 0x1F800166/0x1F800190
  //                       instead of being cleared to 0 when unpaused, dispatches
  //                       FUN_80058F5C (mode-N dispatch table B, the near-duplicate sibling of
  //                       table A) instead of table A, and just TICKS outerTransitionGate()
  //                       instead of committing a new target
  //     5,6 = SCRIPTED -> direct dispatch into already-substrate cutscene-ish leaves
  //                       0x8018BD30 / 0x8018BE40 (outside this RE region) + matrix-compose
  //     7 = LOAD-WAIT  -> a 3-state (G+5: 0/1/2) sub-machine that kicks a load (FUN_8001CF2C),
  //                       polls asset-readiness (assetReady, guest FUN_80045580), and on commit
  //                       resets to state 1 (ACTIVE) and stamps an anim-pointer's mode fields
  //                       (*0x1F800138 + 0x4C/0x4E)
  //   0x800ECF54/0x800E7E68 (the SAME "turn-suppress mask" pair beh_actor_tomba_proximity_
  //   combat's enemy-engage tables write) are save/restored around cases 1 and 4 so their
  //   per-frame masking is scoped to just the sub-dispatch each wraps. Faithful 1:1 port from
  //   guest 0x8005950C (authenticated executable/overlay evidence — ground truth; Ghidra's own decompile of
  //   this function matched it exactly, cross-checked line-by-line against the guest C).
  //   Guest frame: addiu sp,-32; spill s0(<-a0=G),s1,s2,ra. WIRED + SBS-VERIFIED 2026-07-09
  //   (override registry; frameStartTickFaithful's `default: typed runtime address dispatch(c, 0x8005950Cu)` now hits
  //   the native). frameTick's OWN logic is byte-exact: PSXPORT_SBS_MODE=full autonav ran 0
  //   sbs-div / 0 VIOLATION through f15600+ (was diverging at f158 before the register-faithfulness
  //   fix — see below). Of the 5 drafted sub-callees below, 4 (turnBiasCompute/outerTransitionGate/
  //   outerTransitionCommit/assetReady) are now ALSO wired+verified (2026-07-10 §9 promotion pass —
  //   see docs/findings/animation.md for the bugs that pass found: a MIPS branch-delay-slot misread
  //   in turnBiasCompute, a wrong gate constant in outerTransitionCommit, 7 missing r31 mirrors)
  //   and reached through their own tomba::native::declareOverride + tomba::native::declareOverride registrations —
  //   frameTick's `typed runtime address dispatch(c, addr)` call sites below are unchanged and pick them up
  //   automatically. resetLoadGate remains UNWIRED (dispatched to substrate, out of that pass's
  //   scope). Every dispatch sets the gen jal-site r31 constant first so substrate/native callees
  //   that spill ra byte-match core B.
  //   REGISTER-FAITHFULNESS (the f158 root cause): gen keeps r17/r18 live across the case-1/4/7
  //   callees (r17=ECF54_saved/r18=E7E68_saved in cases 1,4; r17=sub/r18=1 in case 7). The case
  //   callees guest 0x800597AC (spills r18), guest 0x80053FDC (spills r17), guest 0x80076D68 (spills
  //   r17+r18) read those register values via their own spill slots — so frameTick MUST write
  //   c->r[17]/c->r[18] at the same points gen does (using C++ locals alone leaves the stale
  //   caller values in the registers → the callee spills diverge). Same pattern as the
  //   NodeXform register-faithfulness fix (docs/findings/animation.md).
  void frameTick();

private:
  // ----------------------------------------------------------------------------
  // frameTick()'s immediate callees RE'd + drafted this pass (0x8005950C's direct sub-tree).
  // ----------------------------------------------------------------------------
  // turnBiasCompute(c, facing) — guest FUN_80055C9C. Frameless leaf (a0/G unused — only
  //   a1=facing heading + fixed globals DAT_800E806C/DAT_1F8000F2/DAT_800E805A). Computes a
  //   facing-vs-cached-view-heading delta, gated by a mode byte (DAT_800E806C==5 = a wide/menu
  //   variant with its own delta formula) and a "close" threshold (2048 / 2560 / 1536 depending
  //   on DAT_800E805A bit 0x800), then stamps a (turn-in, turn-out) bias-magnitude pair to
  //   0x1F80016C/0x1F80016E — the SAME turn-bias slots beh_actor_tomba_proximity_combat's
  //   enemy-engage tables also write (Tomba's own per-frame counterpart of that enemy nudge).
  //   Faithful port from guest 0x80055C9C (authenticated executable/overlay evidence, ground truth) — all 3
  //   goto-chains fully traced and consolidated into one boolean; safe to restructure (unlike a
  //   dense/DAG-shaped function) because every path was hand-verified against the guest C.
  static void turnBiasCompute(Core *c, int16_t facing);

  // outerTransitionGate() — guest FUN_80053E50(G). The gate outerTransitionCommit (and case 4)
  //   call first: bails (false) while G+0x16E (a pending-frame counter) is still positive.
  //   Otherwise clears 0x800BF81E, runs Engine::gStateMutate(G, 0xB) (already-native), and when
  //   G+0x164==1 (a specific interaction-slot state) OR the global "busy" latch 0x800BF80D is
  //   clear, resets the walk-state (G+0=3, G+0x16E/0x170/0x146 cleared) and commits outer-state
  //   2 (G+4=2, G+5=1, G+6=0, 0x800BF80D=1) plus spawns a stop-motion task (FUN_800312D4).
  //   Returns true once any path completes handling (frameTick's callers use it purely as a
  //   bail-early check). Faithful port from guest 0x80053E50 (authenticated executable/overlay evidence, ground
  //   truth). Guest frame: addiu sp,-32; spill s0,s1,s2,ra.
  bool outerTransitionGate();

  // outerTransitionCommit(mode) — guest FUN_80053FDC(G, mode). Calls outerTransitionGate() as
  //   its own first gate. If G+0x16E (pending counter) != G+0x170 (target), fires the
  //   transition cue + gStateMutate(0xB) + conditional walk-reset, then commits a NEW target
  //   (G+0x172=0x5A, G+0x170=G+0x16E) UNLESS the outer state is already 2 (mode!=1 branch only)
  //   or a "cutscene/dialogue" flag (G+0 & 0xC) is set (in which case it fires an SFX cue +
  //   spawns a task instead and returns without touching G+0). If they're already equal,
  //   decrements the settle counter G+0x172 and — on it hitting zero — either commits walk-
  //   state 1 (when G+0 != 2 AND (G+0 & 4) == 0) or re-arms the counter to 1 (G+0x172=1)
  //   otherwise. RE-CORRECTED 2026-07-10 (§9 promotion pass): the 2026-07-08 draft's "CORRECTED"
  //   note below was itself wrong — it claimed ground-truth guest 0x80053FDC compares g0 against
  //   0 (reverting what it called a Ghidra decompiler error showing `!= 2`), but a fresh line-by-
  //   line re-diff of authenticated executable/overlay evidence (`r3=u8(G+0); if(r3==2) goto rearm;` — a literal
  //   constant 2, unambiguous in the recorded binary evidence's C, not a Ghidra artifact) plus an SBS-
  //   confirmed fix (0-diff only after reverting to `!= 2`) proves gen genuinely gates on the
  //   literal 2, not 0: gen never special-cases g0==0 and DOES special-case g0==2. The pre-existing
  //   comment inverted this while "fixing" a Ghidra mismatch that wasn't actually wrong. See
  //   docs/findings/animation.md "turnBiasCompute/.../outerTransitionCommit ... §9 promotion"
  //   for the full writeup. Faithful port from ground truth. Guest frame: addiu sp,-32; spill
  //   s0,s1,ra (no s2 slot — a smaller frame than outerTransitionGate's).
  void outerTransitionCommit(int32_t mode);

  // assetReady(c, slot) — guest FUN_80045580. Frameless-except-ra leaf: looks up a per-slot
  //   4-byte record at (&DAT_800BE11C)[slot] and forwards it (plus 2 fixed table pointers
  //   0x8018A000/DAT_800A3EC8) to the substrate loader-status leaf FUN_80044CD4, returning
  //   whether it reported a positive (>0) status. Faithful port from guest 0x80045580
  //   (authenticated executable/overlay evidence, ground truth — matches Ghidra 1:1).
  static bool assetReady(Core *c, int32_t slot);

  // resetLoadGate(c) — guest FUN_80042310. Frameless-except-ra leaf: fires a niladic substrate
  //   cue (FUN_8001CF78), an SFX/cue trigger (FUN_80074590(0x7F,0,0)), clears the pause latch
  //   0x1F800137, then forwards the current area/mode byte (DAT_800BF870) into FUN_80074F24
  //   (an already-substrate "commit area mode" leaf). Faithful port from guest 0x80042310
  //   (authenticated executable/overlay evidence, ground truth — matches Ghidra 1:1).
  static void resetLoadGate(Core *c);

  void mode0ActionGate(); // FUN_8005A910 — selects the normal or swim guest handler.
};

} // namespace tomba::player
