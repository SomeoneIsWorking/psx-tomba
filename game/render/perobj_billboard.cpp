// perobj_billboard.cpp — SUBSTRATE MIRROR for the per-object render-TYPE dispatch (FUN_8003CCA4) and
// 3 of its "special effect" billboard/particle-quad leaf renderers (FUN_8003C2D4, FUN_8003C464,
// FUN_8003C8F4). Same band (0x8003xxxx) as perobj_dispatch.cpp's cmdListDispatch/perModeDispatch;
// same ownership mechanism (tomba::native::declareOverride — these are reached as PLAIN intra-shard C calls from
// the still-substrate walk cluster guest 0x8003BF00/etc, never through typed runtime address dispatch).
//
// RE method: Ghidra headless decompile of a live free-roam RAM dump cross-checked against the ACTUAL
// guest body in authenticated executable/overlay evidence (C2D4), shard_1.c (C464), shard_4.c (C8F4), shard_5.c (CCA4),
// authenticated executable/overlay evidence (image-qualified runtime dispatcher slot wiring) — the recorded binary
// evidence's gte_write_ctrl/gte_write_data/ gte_op/gte_read_data calls are ground truth, not Ghidra's COP2 pseudo-C.
// All 4 addresses confirmed unowned via tools/codemap.py before porting.
//
// ==================================================================================================
// FUN_8003CCA4 (perObjRenderDispatch, a0=node r4): stores node into the "current render node" scratch
// (0x1F80028C), then selects one of 6 cases by
// `mem8(node+13) & 0xB` (bound-checked < 9) via a 9-slot table at 0x80014EC8. Every valid case runs
// cmdListDispatch() (already owned, FUN_8003CDD8) then, for 4 of the 6 cases, calls one of 5
// still-substrate "special effect" leaves (FUN_8003D584/F344/F3F4/F4C4/F594) with
// (node, poolPtrBeforeCmdListDispatch, poolPtrAfter) — i.e. the packet-pool span cmdListDispatch just
// emitted. None of these 5 leaves fire at seaside (perobj_dispatch.cpp's prior finding); they stay
// substrate, reached as plain guest-ABI calls so they still see the correct pool-pointer bracket.
//
// FUN_8003C2D4 / FUN_8003C464 (billboardCompose1/2, a0=node r4): each builds a "local" transform for a
// billboard-type node and composes it with a persistent camera MATRIX before handing off to
// billboardEmit. Both use a shared per-instance SCRATCHPAD region at 0x1F800000
// (BUF below) that holds ordinary libgte MATRIX structs (m[3][3] int16 row-major + 2 pad bytes + t[3]
// int32 — exactly what Mtx::identity's 8-word write pattern and Math::rotZ/matMul's byte
// reads agree on):
//   BUF+0x00 MAT_A     — C2D4: identity (Mtx::identity). C464: seeded by the still-substrate
//                         FUN_800517BC(node+122/124/126 as s16 x,y,z) instead of identity.
//   BUF+0x20 MAT_ROTZ  — identity, then Z-rotated in place by mem16(node+90) via Math::rotZ.
//   BUF+0x40 MAT_OUT   — Math::matMul(MAT_ROTZ, MAT_A, MAT_OUT) (= MAT_ROTZ for C2D4, since MAT_A is
//                         identity there); .t (=MAT_OUT+0x14) becomes the composed WORLD translation.
//   BUF+0xC0 WORLD_POS — object's world position triple (s16 x3, from node+46/50/54).
//   BUF+0xF8 CAM2      — the SCENE CAMERA view MATRIX at scratchpad 0x1F8000F8 — m rows @+0xF8..0x109,
//                         t @+0x10C..0x117. Read-only
//                         here. (#67 correction: an earlier note called this "main-RAM 0x800C0000";
//                         that was the same mis-base the f117 fix corrected for BUF itself.)
// Both then: load CAM2.m into CR0-4, MVMVA-transform WORLD_POS by it (same opcode as cmdListDispatch's
// world-translate), add CAM2.t into MAT_OUT.t, reload CR0-7 from MAT_OUT (rotation + composed
// translation), and call billboardEmit(node, mem8(node+71)&1).
//
// FUN_8003C8F4 (billboardEmit, a0=node r4, a1=flag r5): resolves the node's active particle SUB-LIST
// (node+56 -> {count:s16@0, byteOff:s16@2}[] indexed by *(s16*)(*(node+56)); node+60 = sub-list base),
// then for each particle (16-byte stride):
//   1. still-substrate FUN_8003B220(a0=guest scratch, a1=0, a2=particle) fills 4 quad-corner vectors
//      (V0..V2 packed for RTPT, V3 for a second RTPS) into REAL GUEST STACK memory at r29+16..+45 —
//      genuine stack addresses (not a host buffer) because a substrate callee writes them via
//      Core::mem_w16/32, and because SBS compares guest RAM including live stack frames (see
//      docs/findings — Animation::attach's guest-stack residual). A real GuestFrame(96) allocation
//      backs this (found the hard way: omitting it — and the callers' own frame allocations —
//      shifted this frame relative to the guest instruction path and produced a real, reproducible SBS diff).
//   2. RTPT (0x4A280030) projects V0-2 -> SXY0-2; on success stash them at BUF+8/16/24, AVSZ3
//      (0x4B400006) gives a first depth estimate; RTPS (0x4A180001) projects V3, and on success AVSZ4
//      (0x4B68002E) gives the final OTZ-style depth (else the particle is invalid, depth=-1).
//   3. off-screen cull: skip if all 4 corners' X>=320 (unsigned) or all 4 corners' Y>=240.
//   4. quantize the depth into an OT bucket (node+8 signed per-node depth-bias byte, >>10/<<9 rebucket,
//      clamped to the valid <2044 range else reset to -1 and skip).
//   5. still-substrate FUN_8003B054(BUF, particle, flag) fills the packet's color/UV fields; an
//      optional node+92 half-word override and a node+13-selected small case table (6 labels) patch a
//      couple of BUF bytes (sprite index / extra color word) before emission.
//   6. emit a 10-word packet (1 tag word [size=9 | old-OT-head] + 9 data words copied from
//      BUF+4..+36) at the packet-pool tail, prepended into the OT bucket
//      `depth` (OrderingTable::link) — the identical packet-chain mechanism perobj_dispatch.cpp's
//      cmdListDispatch/perModeDispatch already documents.
#include "core.h"
#include "core/entry/game_ctx.h"
#include "core/overrides/native_override_catalog.h"
#include "game.h"
#include "guest_call.h"
#include "guest_ordering_table.h"
#include "horizontal_visibility_cull.h"
#include "render.h"
// original guest-instruction fallbacks for the test-only substrate gate. The image-qualified runtime dispatcher is a
// single PROCESS-GLOBAL table shared by EVERY Core (SBS core A AND core B), so the trampolines below MUST defer to the
// real guest body on core B (the pure-substrate oracle) — otherwise the oracle runs this native mirror and SBS compares
// native-vs-native (a false 0-div) instead of native-vs-substrate. Same discipline as every other
// tomba::native::declareOverride cluster (gte_math/node_xform/cull/...). The oracle may carry ONLY async→sync
// conversions (sync_overrides.cpp) + HLE BIOS — nothing engine/game.

// Still-substrate leaves called by these 4 (declared, called via plain guest-ABI intra-shard calls —
// exactly as the authenticated executable/overlay evidence reaches them; image-qualified runtime dispatcher still gates
// each, so if one is ever owned later these calls transparently pick that up).

namespace {

constexpr uint32_t CUR_NODE_SCR = 0x1F80028Cu; // "current render node" scratch
constexpr uint32_t BUF = 0x1F800000u;          // SCRATCHPAD MATRIX-compose buffer (C2D4/C464/C8F4) —
                                               // guest 0x8003C2D4/8003C8F4 base r16/r17 = 8064<<16
                                               // = 0x1F800000, NOT main RAM. (Was wrongly 0x800C0000;
                                               // the mis-base made every emitted packet's data differ
                                               // from the substrate — the f117 divergence, masked by the
                                               // false 0-div until the oracle-gate fix surfaced it.)
constexpr uint32_t MAT_A = BUF + 0x00u;
constexpr uint32_t MAT_ROTZ = BUF + 0x20u;
constexpr uint32_t MAT_OUT = BUF + 0x40u;
constexpr uint32_t WORLD_POS = BUF + 0xC0u;
constexpr uint32_t CAM2 = BUF + 0xF8u;        // persistent camera MATRIX mirror (read-only here)
constexpr uint32_t MVMVA_TRANS = 0x4A486012u; // same opcode cmdListDispatch uses for the world-translate

// RAII guest-stack frame: real guest instruction paths allocate their own stack frame (r29 -= size) before
// running, and callees compute their OWN frame relative to the CALLER'S post-allocation r29 — so a
// callee reached from here (billboardEmit reads its scratch as c->r[29]-96+off) needs r29 to reflect
// the SAME depth the guest instruction path would have at that call, even though nothing in THIS function's own
// body reads/writes through r29 itself. Symmetric allocate/restore, net-zero like the guest instruction path's own
// push/pop (found empirically: 8003C2D4/8003C464 omitting this shifted 8003C8F4's frame by their own
// size and produced a real, reproducible SBS diff at f118 in the task-0 stack region).
struct GuestFrame {
  Core *c;
  uint32_t size;
  GuestFrame(Core *c_, uint32_t size_) : c(c_), size(size_) {
    c->r[29] -= size;
  }
  ~GuestFrame() {
    c->r[29] += size;
  }
};

// FUN_8003CCA4's REAL prologue (register-faithfulness, f118 root cause, 2026-07-09): unlike the
// call sites above where GuestFrame's bare sp-adjust is enough (their bodies spill live-injected
// values inline themselves), guest 0x8003CCA4 (authenticated executable/overlay evidence) actually SPILLS its caller's
// live r16/r17/r18/r31 to guest memory at entry (mem_w32 sp+16/20/24/28) and restores them at every
// exit (L_8003CDC0) — a plain MIPS callee-save prologue/epilogue, not a value injection. The bare
// GuestFrame(c,32) this call site used only adjusted c->r[29] and never wrote those 4 words, leaving
// WHATEVER STALE bytes were already sitting in that guest-stack region (leftover from an unrelated
// earlier writer) instead of the caller's real r16/r17/r18/r31 — the exact SBS diff at
// 0x801FE8B8../0x801FE8E8.. (task-0 stack, several frames into this call chain) that unmasked once
// the f62 register-faithfulness gap (cmdListDispatch's r16=loop-index/r17=SCR, see
// perobj_dispatch.cpp) was fixed and the SBS gate advanced past it. r18 is reassigned to `node`
// immediately after the spill (matching gen's `r18 = r4` right after `mem_w32(sp+24,r18)`); r16/r17
// are pure save/restore (this function's own body never sets them — case 0x8003CD00, the only case
// seaside objects hit, doesn't either, per gen).
struct CCA4Frame {
  Core *c;
  uint32_t s16, s17, s18, sra;
  explicit CCA4Frame(Core *c_) : c(c_), s16(c_->r[16]), s17(c_->r[17]), s18(c_->r[18]), sra(c_->r[31]) {
    c->r[29] -= 32;
    c->mem_w32(c->r[29] + 24, s18);
    c->mem_w32(c->r[29] + 28, sra);
    c->mem_w32(c->r[29] + 20, s17);
    c->mem_w32(c->r[29] + 16, s16);
  }
  ~CCA4Frame() {
    c->r[31] = c->mem_r32(c->r[29] + 28);
    c->r[18] = c->mem_r32(c->r[29] + 24);
    c->r[17] = c->mem_r32(c->r[29] + 20);
    c->r[16] = c->mem_r32(c->r[29] + 16);
    c->r[29] += 32;
  }
};

} // namespace

// ==================================================================================================
// FUN_8003CCA4
void Render::perObjRenderDispatch() {
  Core *c = mCore;
  CCA4Frame frame(c);
  const uint32_t node = c->r[4];
  // Register-faithfulness (2026-07-10, the f118 residual root cause — one level deeper than the
  // FUN_8003C048 ownership fix): guest 0x8003CCA4's REAL prologue (authenticated executable/overlay evidence)
  // reassigns r18 = r4 (node) IMMEDIATELY after its own spill, and computes r5 = ((mem8(node+13) ^
  // 15) < 1) ONCE, before the case switch — both values stay LIVE (plain MIPS register lifetime,
  // never re-set per case) all the way to whichever case's `guest 0x8003CDD8(c)` call. This function's
  // own C++ body only ever needed the local `node`, so a prior draft never wrote c->r[18]/c->r[5] —
  // meaning cmdListDispatch's CmdListFrame (which spills "caller r18" as part of its own real
  // prologue) span stale bytes instead of gen's real node/flag, and cmdListDispatch's `flag` param
  // (c->r[5]) silently held garbage instead of gen's real per-node flag. Confirmed via
  // PSXPORT_SBS_PREWATCH=0x801FE8B8: core B's write came from guest 0x8003CDD8+0x18 (its own r18
  // spill) with the caller (guest 0x8003CCA4, reached via FUN_8003C048) holding r18=node, while
  // core A held whatever renderWalk's own r18 (CASE188_SCR, an unrelated constant) still was.
  c->r[18] = node;
  // gen: r3=mem8(node+11); r3^=15; r5=(r3<1) — the FLAG field is node+11 (NOT node+13, which is the
  // separate `sel` case-table index below). A prior draft of this fix used node+13 for both,
  // routing cmdListDispatch's flag&1 test the wrong way and making perModeDispatch pick the
  // per-mode table (native) instead of gen's real generic-fallback path (guest 0x800803DC) for nodes
  // whose real flag has bit0 set — confirmed via PSXPORT_SBS_PREWATCH: core B's chain ended in
  // guest 0x800803DC while core A's ended in the per-mode target overlay guest 0x80146478.
  const uint32_t flag = ((c->mem_r8(node + 11) ^ 15u) < 1u) ? 1u : 0u;
  c->mem_w32(CUR_NODE_SCR, node);
  const uint32_t sel = c->mem_r8(node + 13) & 11u;
  if (sel >= 9u) {
    return;
  }
  constexpr uint32_t TABLE = 0x80014EC8u;
  const uint32_t target = c->mem_r32(TABLE + sel * 4u);
  // RE'd return-address constants gen sets in r31 immediately before each nested call (see
  // authenticated executable/overlay evidence guest 0x8003CCA4). Register-faithfulness (2026-07-09, the f118 residual
  // root cause): a prior draft called cmdListDispatch()/the special-effect leaves without ever
  // setting c->r[31], leaving whatever stale value the OUTER caller (FUN_8003C048) left there
  // instead — a real, reproducible SBS diff at FUN_80146478's own ra spill slot (0x801FE8D0..),
  // several frames deep in this call chain. Mirrored per CLAUDE.md ("MIRROR THE GUEST STACK...
  // register-faithfulness"), same discipline as billboardCompose1/2's own fix (commit bef7769).
  switch (target) {
  case 0x8003CD00u: {
    c->r[4] = node;
    c->r[5] = flag;
    c->r[31] = 0x8003CD08u;
    rend(c)->cmdListDispatch();
    break;
  }
  case 0x8003CD10u: {
    const uint32_t pre = tomba2::render::PacketPool(*c).cursor();
    c->r[4] = node;
    c->r[5] = flag;
    c->r[31] = 0x8003CD20u;
    rend(c)->cmdListDispatch();
    const uint32_t post = tomba2::render::PacketPool(*c).cursor();
    c->r[4] = node;
    c->r[5] = pre;
    c->r[6] = post;
    c->r[31] = 0x8003CD30u;
    rend(c)->effectColorAdd(node, pre, post);
    break;
  }
  case 0x8003CD38u: {
    const uint32_t pre = tomba2::render::PacketPool(*c).cursor();
    c->r[4] = node;
    c->r[5] = flag;
    c->r[31] = 0x8003CD48u;
    rend(c)->cmdListDispatch();
    const uint32_t post = tomba2::render::PacketPool(*c).cursor();
    c->r[4] = node;
    c->r[5] = pre;
    c->r[6] = post;
    c->r[31] = 0x8003CD58u;
    rend(c)->effectClutSwap(node, pre, post);
    break;
  }
  case 0x8003CD60u: {
    const uint32_t pre = tomba2::render::PacketPool(*c).cursor();
    c->r[4] = node;
    c->r[5] = flag;
    c->r[31] = 0x8003CD70u;
    rend(c)->cmdListDispatch();
    const uint32_t post = tomba2::render::PacketPool(*c).cursor();
    c->r[4] = node;
    c->r[5] = pre;
    c->r[6] = post;
    // Branch polarity (2026-07-09, found during the same audit): guest 0x8003CCA4 L_8003CD60
    // tests node+27==0 -> guest 0x8003F4C4 (the L_8003CD90 target), node+27!=0 -> guest 0x8003F3F4 —
    // a prior draft had this INVERTED. Neither leaf fires at seaside (this file's own banner),
    // so the flip was never caught by the autonav gate; fixed here to match gen exactly.
    if (c->mem_r8(node + 27) == 0) {
      c->r[31] = 0x8003CD98u;
      rend(c)->effectSemiOff(node, pre, post);
    } else {
      c->r[31] = 0x8003CD88u;
      rend(c)->effectSemiOn(node, pre, post);
    }
    break;
  }
  case 0x8003CDA0u: {
    const uint32_t pre = tomba2::render::PacketPool(*c).cursor();
    c->r[4] = node;
    c->r[5] = flag;
    c->r[31] = 0x8003CDB0u;
    rend(c)->cmdListDispatch();
    const uint32_t post = tomba2::render::PacketPool(*c).cursor();
    c->r[4] = node;
    c->r[5] = pre;
    c->r[6] = post;
    c->r[31] = 0x8003CDC0u;
    rend(c)->effectFlatTint(node, pre, post);
    break;
  }
  case 0x8003CDC0u:
    break; // no-op case: the guest instruction path falls straight to the epilogue
  default:
    // Defensive mirror of the guest instruction path's raw `jr` fallback for an unrecognized table entry — never
    // hit by live game data (only the 6 cases above ever appear in the live table).
    psx::cpu::dispatchGuestToReturn0(*c, target, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
    return;
  }
}

// ==================================================================================================
// Shared tail: compose CAM2 (camera rotation+translation) onto WORLD_POS, add it into outMat.t,
// reload CR0-7 from outMat, then hand off to billboardEmit(node, flag).
// outMat = the BUF slot holding the composed matrix. C2D4/C464 land it in MAT_OUT (BUF+0x40); C788
// (billboardCompose3) composes in place in MAT_ROTZ (BUF+0x20) — its gen loads CR0-7 from BUF+0x20,
// not BUF+0x40 — so the tail is parameterized by that slot instead of hardcoding MAT_OUT.
static void billboardComposeTail(Core *c, uint32_t node, uint32_t flag, uint32_t outMat = MAT_OUT) {
  c->mem_w16(WORLD_POS + 0, c->mem_r16(node + 46));
  c->mem_w16(WORLD_POS + 2, c->mem_r16(node + 50));
  c->mem_w16(WORLD_POS + 4, c->mem_r16(node + 54));
  gte_write_ctrl(0, c->mem_r32(CAM2 + 0));
  gte_write_ctrl(1, c->mem_r32(CAM2 + 4));
  gte_write_ctrl(2, c->mem_r32(CAM2 + 8));
  gte_write_ctrl(3, c->mem_r32(CAM2 + 12));
  gte_write_ctrl(4, c->mem_r32(CAM2 + 16));
  gte_write_data(0, c->mem_r32(WORLD_POS + 0));
  gte_write_data(1, c->mem_r32(WORLD_POS + 4));
  gte_op(c, MVMVA_TRANS);
  c->mem_w32(outMat + 0x14, gte_read_data(25));
  c->mem_w32(outMat + 0x18, gte_read_data(26));
  c->mem_w32(outMat + 0x1C, gte_read_data(27));
  c->mem_w32(outMat + 0x14, c->mem_r32(outMat + 0x14) + c->mem_r32(CAM2 + 0x14));
  c->mem_w32(outMat + 0x18, c->mem_r32(outMat + 0x18) + c->mem_r32(CAM2 + 0x18));
  c->mem_w32(outMat + 0x1C, c->mem_r32(outMat + 0x1C) + c->mem_r32(CAM2 + 0x1C));
  gte_write_ctrl(0, c->mem_r32(outMat + 0));
  gte_write_ctrl(1, c->mem_r32(outMat + 4));
  gte_write_ctrl(2, c->mem_r32(outMat + 8));
  gte_write_ctrl(3, c->mem_r32(outMat + 12));
  gte_write_ctrl(4, c->mem_r32(outMat + 16));
  gte_write_ctrl(5, c->mem_r32(outMat + 0x14));
  gte_write_ctrl(6, c->mem_r32(outMat + 0x18));
  gte_write_ctrl(7, c->mem_r32(outMat + 0x1C));
  c->r[4] = node;
  c->r[5] = flag;
  rend(c)->billboardEmit();
}

// FUN_8003C2D4
void Render::billboardCompose1() {
  Core *c = mCore;
  const uint32_t node = c->r[4];
  if (c->mem_r32(node + 56) == 0) {
    return;
  }
  GuestFrame frame(c, 40);
  // Register-faithfulness (guest 0x8003C2D4 prologue, L4509-4514): spill the caller's
  // r16..r19/ra at sp+16..+32. The GuestFrame only allocates the frame; the spill BYTES are
  // what SBS compares (gen writes them; the bare RAII left stale bytes there).
  const uint32_t sp = c->r[29];
  c->mem_w32(sp + 16, c->r[16]);
  c->mem_w32(sp + 20, c->r[17]);
  c->mem_w32(sp + 24, c->r[18]);
  c->mem_w32(sp + 28, c->r[19]);
  c->mem_w32(sp + 32, c->r[31]);
  mtxOf(c).identity(MAT_A);
  mtxOf(c).identity(MAT_ROTZ);
  mathOf(c).rotZ((int16_t)c->mem_r16(node + 90), MAT_ROTZ);
  const uint32_t flag = c->mem_r8(node + 71) & 1u;
  mathOf(c).matMul(MAT_ROTZ, MAT_A, MAT_OUT);
  // gen's live callee-saved state at the guest 0x8003C8F4 call site (L4593-4595): billboardEmit
  // spills these as its "caller" registers, so they must hold gen's values here.
  c->r[16] = MAT_OUT;
  c->r[17] = MAT_A;
  c->r[18] = flag;
  c->r[19] = node;
  c->r[31] = 0x8003C448u;
  billboardComposeTail(c, node, flag);
  // Epilogue restore (guest 0x8003C2D4 L4597-4601): read the caller's values back from the spill
  // slots. MUST restore — the reassignments above (esp. r31=0x8003C448) would otherwise leak to the
  // substrate render-walk caller and corrupt its control flow (registers aren't SBS-compared, but
  // the substrate reads them).
  c->r[16] = c->mem_r32(sp + 16);
  c->r[17] = c->mem_r32(sp + 20);
  c->r[18] = c->mem_r32(sp + 24);
  c->r[19] = c->mem_r32(sp + 28);
  c->r[31] = c->mem_r32(sp + 32);
}

// FUN_8003C464
void Render::billboardCompose2() {
  Core *c = mCore;
  const uint32_t node = c->r[4];
  if (c->mem_r32(node + 56) == 0) {
    return;
  }
  GuestFrame frame(c, 32);
  // Register-faithfulness (guest 0x8003C464 prologue, L5907-5911): spill caller's
  // r16/r17/r18/ra at sp+16/+20/+24/+28. (C464's prologue does NOT spill r19 — it passes through.)
  const uint32_t sp = c->r[29];
  c->mem_w32(sp + 16, c->r[16]);
  c->mem_w32(sp + 20, c->r[17]);
  c->mem_w32(sp + 24, c->r[18]);
  c->mem_w32(sp + 28, c->r[31]);
  c->r[4] = MAT_A;
  c->r[5] = (uint32_t)c->mem_r16s(node + 122);
  c->r[6] = (uint32_t)c->mem_r16s(node + 124);
  c->r[7] = (uint32_t)c->mem_r16s(node + 126);
  psx::cpu::dispatchGuestToReturn0(*c, 0x800517BCu, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
  mtxOf(c).identity(MAT_ROTZ);
  mathOf(c).rotZ((int16_t)c->mem_r16(node + 90), MAT_ROTZ);
  const uint32_t flag = c->mem_r8(node + 71) & 1u;
  mathOf(c).matMul(MAT_ROTZ, MAT_A, MAT_OUT);
  // gen's live callee-saved state at the guest 0x8003C8F4 call site (L5993-5995) — NOTE C464 differs
  // from C2D4: r17=flag (not MAT_A) and r18=node (gen reassigns r17 to flag at L5931 and keeps
  // r18=node from the prologue). billboardEmit spills these, so match gen exactly.
  c->r[16] = MAT_OUT;
  c->r[17] = flag;
  c->r[18] = node;
  c->r[31] = 0x8003C5E0u;
  billboardComposeTail(c, node, flag);
  // Epilogue restore (guest 0x8003C464): read the caller's values back from the spill slots
  // (r16/r17/r18/ra — C464 does not save r19). Same anti-leak discipline as billboardCompose1.
  c->r[16] = c->mem_r32(sp + 16);
  c->r[17] = c->mem_r32(sp + 20);
  c->r[18] = c->mem_r32(sp + 24);
  c->r[31] = c->mem_r32(sp + 28);
}

// ==================================================================================================
// FUN_8003C788 — billboardCompose3. Third compose sibling of C2D4/C464. Unlike them (which build a
// Z-rotation LOCAL matrix), C788 seeds MAT_A = identity and folds the node's OWN stored 3x3+t matrix
// (node+152) through it — matMul(node+152, MAT_A, MAT_ROTZ) leaves MAT_ROTZ = the node matrix — then
// runs the SAME CAM2 world-translate tail as its siblings, but composing in place in MAT_ROTZ (BUF+0x20)
// rather than MAT_OUT. All callees owned: Mtx::identity (0x80051794), Math::matMul (0x80084110),
// billboardEmit (0x8003C8F4). Frame 32, spills r16/r17/r18/ra like C464 (no r19). See abi_extract
// 0x8003C788 --contract + authenticated executable/overlay evidence
void Render::billboardCompose3() {
  Core *c = mCore;
  const uint32_t node = c->r[4];
  if (c->mem_r32(node + 56) == 0) {
    return;
  }
  GuestFrame frame(c, 32);
  // Register-faithfulness (guest 0x8003C788 prologue): spill caller's r16/r17/r18/ra at
  // sp+16/+20/+24/+28 — same 32-byte / 4-spill shape as C464.
  const uint32_t sp = c->r[29];
  c->mem_w32(sp + 16, c->r[16]);
  c->mem_w32(sp + 20, c->r[17]);
  c->mem_w32(sp + 24, c->r[18]);
  c->mem_w32(sp + 28, c->r[31]);
  mtxOf(c).identity(MAT_A);
  const uint32_t flag = c->mem_r8(node + 71) & 1u;
  mathOf(c).matMul(node + 152, MAT_A, MAT_ROTZ); // MAT_ROTZ = (node+152 matrix) x identity
  // gen's live callee-saved state at the billboardEmit call site (abi_extract call [2]): r16=MAT_ROTZ,
  // r17=flag, r18=node. billboardEmit spills these as its caller regs, so match gen exactly.
  c->r[16] = MAT_ROTZ;
  c->r[17] = flag;
  c->r[18] = node;
  c->r[31] = 0x8003C8DCu;
  billboardComposeTail(c, node, flag, MAT_ROTZ); // same GTE world-translate tail as C2D4/C464, on MAT_ROTZ
  // Epilogue restore (guest 0x8003C788 L_8003C8DC): read the caller's r16/r17/r18/ra back.
  c->r[16] = c->mem_r32(sp + 16);
  c->r[17] = c->mem_r32(sp + 20);
  c->r[18] = c->mem_r32(sp + 24);
  c->r[31] = c->mem_r32(sp + 28);
}

// ==================================================================================================
// FUN_8003C5F8 — billboardComposeC5F8. Fourth compose sibling. STRUCTURALLY IDENTICAL to C2D4
// (billboardCompose1): identity(MAT_A), identity(MAT_ROTZ), build a local rotation into MAT_ROTZ,
// then matMul(MAT_ROTZ, MAT_A, MAT_OUT) and the shared CAM2 world-translate tail → billboardEmit.
// The ONLY difference from C2D4: instead of a single-angle Z rotation (Math::rotZ on mem16(node+90)),
// C5F8 builds a FULL 3-Euler-angle rotation from the SVECTOR at node+84 via Math::rotMatSoft
// (FUN_800847F0, the non-GTE software RotMatrix owned this session). Frame 40, spills r16/r17/r18/
// r19/ra like C2D4. All callees owned: Mtx::identity (0x80051794), Math::rotMatSoft (0x800847F0),
// Math::matMul (0x80084110), billboardEmit (0x8003C8F4). See authenticated executable/overlay evidence guest
// 0x8003C5F8.
void Render::billboardComposeC5F8() {
  Core *c = mCore;
  const uint32_t node = c->r[4];
  if (c->mem_r32(node + 56) == 0) {
    return;
  }
  GuestFrame frame(c, 40);
  // Register-faithfulness (guest 0x8003C5F8 prologue): spill caller's r16..r19/ra at sp+16..+32 —
  // same 40-byte / 5-spill shape as C2D4.
  const uint32_t sp = c->r[29];
  c->mem_w32(sp + 16, c->r[16]);
  c->mem_w32(sp + 20, c->r[17]);
  c->mem_w32(sp + 24, c->r[18]);
  c->mem_w32(sp + 28, c->r[19]);
  c->mem_w32(sp + 32, c->r[31]);
  mtxOf(c).identity(MAT_A);
  mtxOf(c).identity(MAT_ROTZ);
  mathOf(c).rotMatSoft(node + 84, MAT_ROTZ); // MAT_ROTZ = software RotMatrix(SVECTOR @ node+84)
  const uint32_t flag = c->mem_r8(node + 71) & 1u;
  mathOf(c).matMul(MAT_ROTZ, MAT_A, MAT_OUT);
  // gen's live callee-saved state at the billboardEmit (guest 0x8003C8F4) call site: r16=MAT_OUT,
  // r17=MAT_A, r18=flag, r19=node — identical to C2D4. billboardEmit spills these as its caller regs.
  c->r[16] = MAT_OUT;
  c->r[17] = MAT_A;
  c->r[18] = flag;
  c->r[19] = node;
  c->r[31] = 0x8003C76Cu;
  billboardComposeTail(c, node, flag);
  // Epilogue restore (guest 0x8003C5F8 L_8003C76C): read the caller's r16..r19/ra back from the
  // spill slots — same anti-leak discipline as C2D4.
  c->r[16] = c->mem_r32(sp + 16);
  c->r[17] = c->mem_r32(sp + 20);
  c->r[18] = c->mem_r32(sp + 24);
  c->r[19] = c->mem_r32(sp + 28);
  c->r[31] = c->mem_r32(sp + 32);
}

// ==================================================================================================
// FUN_8003C8F4
void Render::billboardEmit() {
  Core *c = mCore;
  const uint32_t node = c->r[4];
  const uint32_t flag = c->r[5];
  if (c->mem_r32(node + 56) == 0) {
    return;
  }
  GuestFrame frame(c, 96); // real guest stack frame: guest 0x8003B220 writes through this as a real
                           // guest address, and callers' own frames must already be allocated
                           // (see GuestFrame's comment) for this base to land on the same bytes
                           // the guest instruction path uses.
  // Register-faithfulness (guest 0x8003C8F4 prologue, L4367-4376): spill the caller's
  // r16..r22/ra at sp+64..+92. The spilled values are the caller's (billboardCompose1/2) live
  // callee-saved registers — which this port now sets correctly before the call (see above).
  const uint32_t sp = c->r[29];
  c->mem_w32(sp + 64, c->r[16]);
  c->mem_w32(sp + 68, c->r[17]);
  c->mem_w32(sp + 72, c->r[18]);
  c->mem_w32(sp + 76, c->r[19]);
  c->mem_w32(sp + 80, c->r[20]);
  c->mem_w32(sp + 84, c->r[21]);
  c->mem_w32(sp + 88, c->r[22]);
  c->mem_w32(sp + 92, c->r[31]);
  auto FR = [c](uint32_t off) {
    return c->r[29] + off;
  };
  constexpr int32_t DEFAULT_DEPTH = -1;

  // Resolve the active particle sub-list.
  const uint32_t tbl = c->mem_r32(node + 56);
  const int idx = (int16_t)c->mem_r16(tbl + 0);
  const uint32_t listBase = c->mem_r32(node + 60);
  const uint32_t entry = listBase + (uint32_t)(idx << 2);
  const int16_t byteOff = (int16_t)c->mem_r16(entry + 2);
  int count = (int16_t)c->mem_r16(entry + 0);
  uint32_t particle = listBase + (uint32_t)(int32_t)byteOff;
  int bbIt = 0; // particle index within THIS billboardEmit call (bbord diag: same-call grouping)
  for (; count != 0; count--, particle += 16u, bbIt++) {
    // 1) Build the quad's 4 corner vectors (still-substrate; writes real guest stack memory).
    c->r[4] = FR(16);
    c->r[5] = 0;
    c->r[6] = particle;
    psx::cpu::dispatchGuestToReturn0(*c, 0x8003B220u, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);

    gte_write_data(0, c->mem_r32(FR(16) + 0));
    gte_write_data(1, c->mem_r32(FR(16) + 4));
    gte_write_data(2, c->mem_r32(FR(16) + 8));
    gte_write_data(3, c->mem_r32(FR(16) + 12));
    gte_write_data(4, c->mem_r32(FR(16) + 16));
    gte_write_data(5, c->mem_r32(FR(16) + 20));
    gte_op(c, 0x4A280030u); // RTPT: project V0-2 -> SXY0-2
    int32_t ctrl31 = (int32_t)gte_read_ctrl(31);
    c->mem_w32(FR(48), (uint32_t)ctrl31);
    int32_t depth;
    if (ctrl31 < 0) {
      depth = DEFAULT_DEPTH;
    } else {
      c->mem_w32(BUF + 8, gte_read_data(12));
      c->mem_w32(BUF + 16, gte_read_data(13));
      c->mem_w32(BUF + 24, gte_read_data(14));
      gte_op(c, 0x4B400006u); // AVSZ3
      c->mem_w32(FR(48), gte_read_data(24));
      gte_write_data(0, c->mem_r32(FR(40) + 0));
      gte_write_data(1, c->mem_r32(FR(40) + 4));
      gte_op(c, 0x4A180001u); // RTPS: project V3
      ctrl31 = (int32_t)gte_read_ctrl(31);
      c->mem_w32(FR(48), (uint32_t)ctrl31);
      if (ctrl31 >= 0) {
        c->mem_w32(BUF + 32, gte_read_data(14));
        gte_op(c, 0x4B68002Eu); // AVSZ4 -> OTZ
        c->mem_w32(FR(52), gte_read_data(7));
        depth = (int32_t)c->mem_r32(FR(52));
      } else {
        depth = DEFAULT_DEPTH;
      }
    }
    c->mem_w32(FR(56), (uint32_t)depth);

    // 2) Off-screen cull: X over the draw window, Y under 240, unsigned 16-bit as the guest.
    const std::uint32_t xs[4] = {c->mem_r16(BUF + 8), c->mem_r16(BUF + 16), c->mem_r16(BUF + 24), c->mem_r16(BUF + 32)};
    bool onX = tomba2::horizontal_cull::forDrawWindow(c).keepsX(xs, 4, tomba2::horizontal_cull::Domain::Packed16);
    if (!onX) {
      continue;
    }
    bool onY = (uint32_t)c->mem_r16(BUF + 10) < 240u || (uint32_t)c->mem_r16(BUF + 18) < 240u ||
               (uint32_t)c->mem_r16(BUF + 26) < 240u || (uint32_t)c->mem_r16(BUF + 34) < 240u;
    if (!onY) {
      continue;
    }

    // 3) Quantize into an OT bucket: node's signed per-node depth-bias byte, >>10/<<9 rebucket into
    // the valid range, else reset to invalid (-1).
    {
      int32_t a = (int32_t)c->mem_r32(FR(56)) + (int8_t)c->mem_r8(node + 8);
      int32_t d = tomba2::render::OrderingTable::compressDepth(a);
      c->mem_w32(FR(56), (uint32_t)d);
      if (!tomba2::render::OrderingTable::inDepthRange(d)) {
        c->mem_w32(FR(56), (uint32_t)DEFAULT_DEPTH);
      }
    }
    if ((int32_t)c->mem_r32(FR(56)) < 0) {
      continue;
    }

    // 4) Fill color/UV (still-substrate), then optional overrides.
    c->r[4] = BUF;
    c->r[5] = particle;
    c->r[6] = flag;
    psx::cpu::dispatchGuestToReturn0(*c, 0x8003B054u, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
    if (c->mem_r16(node + 92) != 0) {
      c->mem_w16(BUF + 14, c->mem_r16(node + 92));
    }

    const uint32_t caseSel = c->mem_r8(node + 13);
    if (caseSel < 33u) {
      constexpr uint32_t CASE_TABLE = 0x80014E40u;
      const uint32_t caseTarget = c->mem_r32(CASE_TABLE + caseSel * 4u);
      switch (caseTarget) {
      case 0x8003CB60u:
        c->mem_w8(BUF + 7, 45);
        break;
      case 0x8003CB6Cu:
        c->mem_w8(BUF + 7, 47);
        break;
      case 0x8003CB78u:
        c->mem_w32(BUF + 4, c->mem_r32(node + 24));
        c->mem_w8(BUF + 7, 44);
        break;
      case 0x8003CB90u:
        c->mem_w32(BUF + 4, c->mem_r32(node + 24));
        c->mem_w8(BUF + 7, 46);
        break;
      case 0x8003CBA8u:
        c->mem_w8(BUF + 7, 45);
        c->mem_w16(BUF + 14, c->mem_r8(node + 24) != 0 ? 16507u : 16443u);
        break;
      case 0x8003CBC8u:
        break; // explicit no-op case: falls through to packet emission
      default:
        // Defensive mirror of the guest instruction path's raw `jr` fallback for an unrecognized table entry: the
        // guest instruction path does `typed runtime address dispatch(c, caseTarget); return` here — a FULL early
        // return, NOT a fallthrough to packet emission. Never hit by live game data (this 33-entry table's slots all
        // resolve to one of the 5 cases above or the CBC8 no-op).
        psx::cpu::dispatchGuestToReturn0(*c, caseTarget, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
        return;
      }
    }

    // 5) Emit the 10-word packet (tag + 9 data words) at the pool tail, prepended into the OT bucket.
    const tomba2::render::PacketPool packets(*c);
    uint32_t tail = packets.cursor();
    tomba2::render::OrderingTable::active(*c).link(tail, 9u, c->mem_r32(FR(56)));
    tail += 4;
    for (uint32_t off = 4; off <= 36; off += 4) {
      c->mem_w32(tail, c->mem_r32(BUF + off));
      tail += 4;
    }
    packets.setCursor(tail);
  }
}

// ==================================================================================================
namespace {
// Engine/game natives installed into the per-Core image-qualified runtime dispatcher table. These are NOT gated
// here — the gate lives in ONE place (the override registry, runtime/psx/override_registry.h)
// so it can't be forgotten cluster-by-cluster. tomba::native::declareOverride() installs into that
// registry, which runs the original guest body on the test-only substrate leg and the native
// everywhere else.
void ov_perObjRenderDispatch(Core *c) {
  rend(c)->perObjRenderDispatch();
}
void ov_billboardCompose1(Core *c) {
  rend(c)->billboardCompose1();
}
void ov_billboardCompose2(Core *c) {
  rend(c)->billboardCompose2();
}
void ov_billboardCompose3(Core *c) {
  rend(c)->billboardCompose3();
}
void ov_billboardComposeC5F8(Core *c) {
  rend(c)->billboardComposeC5F8();
}
void ov_billboardEmit(Core *c) {
  rend(c)->billboardEmit();
}
} // namespace

void perobj_billboard_install() {
  static bool done = false;
  if (done) {
    return;
  }
  done = true;
  // tomba::native::declareOverride (runtime/psx/override_registry.h) installs into the ONE
  // process-global override registry, which runs original guest instructions on the oracle leg (core B) and the
  // native handler everywhere else — NOT a raw tomba::native::declareOverride, since these are engine/game
  // natives and the oracle must run the pure guest body for them.
  tomba::native::declareOverride(0x8003CCA4u, "ov_perObjRenderDispatch", ov_perObjRenderDispatch);
  tomba::native::declareOverride(0x8003C2D4u, "ov_billboardCompose1", ov_billboardCompose1);
  tomba::native::declareOverride(0x8003C464u, "ov_billboardCompose2", ov_billboardCompose2);
  tomba::native::declareOverride(0x8003C788u, "ov_billboardCompose3", ov_billboardCompose3);
  tomba::native::declareOverride(0x8003C5F8u, "ov_billboardComposeC5F8", ov_billboardComposeC5F8);
  tomba::native::declareOverride(0x8003C8F4u, "ov_billboardEmit", ov_billboardEmit);
}
