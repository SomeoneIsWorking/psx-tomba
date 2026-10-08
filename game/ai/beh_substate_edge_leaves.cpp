// game/ai/beh_substate_edge_leaves.cpp — WIDE-RE DRAFT case-handler leaves of
// beh_substate_edge_orchestrator (game/ai/beh_substate_edge_orchestrator.cpp, guest 0x8012EB54).
//
// STATUS: UNWIRED / UNVERIFIED (wide-RE tier, docs/fleet-workflow.md §6). These are hand-
// transliterated 1:1 from authenticated executable/overlay evidence{0,1}.c ground truth (A00 overlay the cited guest
// instructions) — NOT mechanically diffed against it yet. Per §9, a wiring pass MUST re-diff every line against the
// generated C before registering + SBS-gating. Nothing here is called from anywhere (not installed
// in the override registry, no tomba::native::declareOverride) — dead code that only needs to COMPILE.
//
// ══ KNOWN DEFECT, THE THREE REMAINING DRAFTS: GUEST STACK SPILLS MISSING (found 2026-07-29) ══
//
// Every draft here descends sp (`c->r[29] -= N`) and then writes NOT ONE of its callee-saved spills
// into guest memory. There is not a single `mem_w32(c->r[29] + …)` in this file. They stash the
// registers in C locals instead and restore them from there. What the guest bodies actually do,
// per `binary ABI evidence <addr> --contract`:
//
//     0x8012E8A8  frame 48   8 spills  (r16-r22, ra @ +16..+44)
//     0x80130524  frame 24   2 spills  (r16 @ +16, ra @ +20)
//     0x8012ED84  frame 56  10 spills  (r16-r23, r30, ra @ +16..+52)
//
// That is 20 guest stack writes these drafts omit. (0x8012F494 was the fourth; it has since been
// REPLACED by a port_guest-visible behavior in game/ai/substate_edge_native.cpp and deleted from here — a second
// analysis found EIGHT defects in it, of which the missing spills were only one.) Wiring any of them as-is is a
// GUARANTEED SBS divergence: the native leg leaves those bytes stale while the substrate leg writes them, and the
// byte-compare covers the stack. This is the "MIRROR THE GUEST STACK" rule in CLAUDE.md — descending
// sp without reproducing the spills is the exact failure it names, and a C local is not a mirror.
//
// It is LATENT, not live: all four are unwired, so nothing is broken today. Fixing it is part of the
// wiring pass, not a separate job — and the cheap fix is to REGENERATE each body with
// `direct executable disassembly`, which emits the prologue verbatim and cannot omit a spill by construction.
// Hand-transliteration is precisely where this class of error is introduced.
//
// Drafted 2026-07-08: 0x8012E8A8 (162 gen-C ln), 0x8012F494 (64 ln), 0x80130524 (133 ln).
// Drafted 2026-07-10 (dedicated wide-RE pass, near-mechanical goto-preserving transliteration —
// same style as game/ai/beh_cull_substate_leaves.cpp's 0x80132A88/0x80132EDC, chosen specifically
// to minimize branch-polarity/operand-order risk on this wide-frame cluster per
// docs/fleet-workflow.md §9): 0x8012ED84 (401 gen-C ln, edge-orchestrator STATE 0 init).
//
// STILL MAPPED-ONLY, NOT drafted (0x8012F5B4 428 ln, 0x8012FD88 406 ln — see docs/engine_re.md for
// the call-graph/field-shape mapping notes; a prior 2026-07-10 pass judged these too large/uncertain
// for one confident pass and this session's budget went to a careful single draft of 0x8012ED84
// instead, per "correctness over coverage — draft ONE well rather than three badly").
//
// Field notes common to all three (from the orchestrator's own header + this session's RE):
//   obj[4]=state, obj[5]=substate, obj[8]=childCount(u8), obj[0xC0+4*i]=child-pointer table (SAME
//   table NodeXform::propagate walks) — obj[0x60] here is 0x1F800137 (fixed CD/controller-state
//   scratchpad byte guarded by case 1 of the orchestrator, unrelated to this file's obj fields).
#include "cfg.h"
#include "core.h"
#include "core/entry/game_ctx.h"
#include "core/overrides/native_override_catalog.h"
#include "math/gte_math.h" // Math::rotmat/matMul/applyMatlv/applyMatrixLV/rotY/rotZ (mathOf(c))
#include "math/mtx.h"      // Mtx::identity (mtxOf(c))
#include <stdint.h>

namespace {
constexpr uint32_t SCR_A = 0x1F800000u; // scratch matrix A (rotmat/identity dest)
constexpr uint32_t SCR_B = 0x1F800020u; // scratch matrix B (identity/rotZ/rotY compose dest)
} // namespace

// (removed 99 lines: the hand draft — REPLACED by a port_guest-visible behavior in game/ai/substate_edge_native.cpp.
// This file's
//  drafts omit their guest stack spills (see the banner above); do not resurrect it.)

// (removed 461 lines: the hand draft — REPLACED by a port_guest-visible behavior in game/ai/substate_edge_native.cpp.
// This file's
//  drafts omit their guest stack spills (see the banner above); do not resurrect it.)
