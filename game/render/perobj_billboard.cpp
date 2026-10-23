// game/render/perobj_billboard.cpp — the per-object render-type dispatch and three of its billboard leaves.
//
//   FUN_8003CCA4  perObjRenderDispatch: node into 0x1F80028C, one of 6 cases by mem8(node+13)&0xB (table
//                 0x80014EC8). Every case runs cmdListDispatch (FUN_8003CDD8); four then run one effect leaf
//                 (FUN_8003D584/F344/F3F4/F4C4/F594) over the packet span it emitted.
//   FUN_8003C2D4 / FUN_8003C464  compose a billboard node's local matrix with the scene camera, then
//                 billboardEmit. Per-instance scratchpad BUF at 0x1F800000:
//                 +0x00 MAT_A (identity; C464 seeds it from FUN_800517BC node+122/124/126), +0x20 MAT_ROTZ
//                 (Z-rotated by mem16(node+90)), +0x40 MAT_OUT (matMul, .t the composed world translation),
//                 +0xC0 WORLD_POS (node+46/50/54), +0xF8 CAM2 (the scene camera view matrix, read-only).
//   FUN_8003C8F4  billboardEmit(node, flag): the node's active particle sub-list (node+56), each 16-byte
//                 particle projected by RTPT/RTPS (corners from FUN_8003B220 into guest stack r29+16..+45),
//                 depth-culled and bucketed, its colour/UV filled by FUN_8003B054, then a 10-word packet
//                 (tag + BUF+4..+36) linked into the OT bucket.
#include "billboard_state.h"
#include "core.h"
#include "core/overrides/guest_jal.h"
#include "core/overrides/native_override_catalog.h"
#include "entry/session.h"
#include "game.h"
#include "guest_abi.h"
#include "guest_call.h"
#include "guest_ordering_table.h"
#include "horizontal_visibility_cull.h"
#include "render.h"

#include <lucent/log.h>

#include <cstdlib>
// original guest-instruction fallbacks for the test-only substrate gate. The image-qualified runtime dispatcher is a
// single PROCESS-GLOBAL table shared by EVERY Core (SBS core A AND core B), so the trampolines below MUST defer to the
// real guest body on core B (the pure-substrate oracle) — otherwise the oracle runs this native mirror and SBS compares
// native-vs-native (a false 0-div) instead of native-vs-substrate. Same discipline as every other
// tomba::native::declareOverride cluster (gte_math/node_xform/cull/...). The oracle may carry ONLY async→sync
// conversions (sync_overrides.cpp) + HLE BIOS — nothing engine/game.

// Still-substrate leaves called by these 4 (declared, called via plain guest-ABI intra-shard calls —
// exactly as the authenticated executable/overlay evidence reaches them; image-qualified runtime dispatcher still gates
// each, so if one is ever owned later these calls transparently pick that up).

namespace tomba {

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

// A case of FUN_8003CCA4's table (0x80014EC8): the effect leaf it runs after cmdListDispatch, and the jal-site ra
// of each call (0 where it makes none).
struct NodeCase {
  render::ObjectEffect effect;
  uint32_t dispatchReturn = 0;
  uint32_t effectReturn = 0;
};

constexpr uint32_t kNoDrawCase = 0x8003CDC0u; // falls straight to the epilogue

// FUN_8004CC88's node table and frame.
constexpr uint32_t kInstanceClock = 0x1F80017Cu; // u16 frame clock; its value mod 20 scrolls kinds 4..10
constexpr uint64_t kDivideBy20 = 0xCCCCCCCDu;    // the guest's multu by 1/20
constexpr uint32_t kInstanceFields = 0x60u;      // held in s3
constexpr uint32_t kInstanceCount = 0x6Au;       // s16
constexpr uint32_t kInstanceTable = 0x6Cu;       // 16-byte entries: kind at +3, s16 x, y, z at +4
constexpr uint32_t kInstancePosition = 8u;       // the entry's z, which s1 points at
constexpr uint32_t kInstanceStride = 0x10u;
constexpr uint32_t kInstanceShown = 0x70u;  // bit per entry
constexpr uint32_t kInstanceHidden = 0x74u; // bit per entry
constexpr uint32_t kInstancePlace = 0x8004A828u;
constexpr uint32_t kInstanceTurn = 0x8004B374u;
constexpr GuestFrameSpill kInstanceSpills[6] = {{16, 16}, {18, 24}, {19, 28}, {31 /*ra*/, 36}, {20, 32}, {17, 20}};

NodeCase nodeCaseOf(Core &c, uint32_t node, uint32_t target) {
  switch (target) {
  case 0x8003CD00u:
    return {{}, 0x8003CD08u, 0};
  case 0x8003CD10u:
    return {render::ObjectEffect::colorAdd(c, node), 0x8003CD20u, 0x8003CD30u}; // FUN_8003D584
  case 0x8003CD38u:
    return {render::ObjectEffect::clutSwap(c, node), 0x8003CD48u, 0x8003CD58u}; // FUN_8003F344
  case 0x8003CD60u:
    // node+27 == 0 runs FUN_8003F4C4 (off), otherwise FUN_8003F3F4 (on).
    if (c.mem_r8(node + 27) == 0) {
      return {render::ObjectEffect::semi(false), 0x8003CD70u, 0x8003CD98u};
    }
    return {render::ObjectEffect::semi(true), 0x8003CD70u, 0x8003CD88u};
  case 0x8003CDA0u:
    return {render::ObjectEffect::flatTint(c, node), 0x8003CDB0u, 0x8003CDC0u}; // FUN_8003F594
  default:
    return {};
  }
}

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
  // Each case publishes its own jal-site ra before cmdListDispatch and before its effect leaf.
  const NodeCase nodeCase = nodeCaseOf(*c, node, target);
  if (nodeCase.dispatchReturn == 0) {
    if (target != kNoDrawCase) {
      psx::cpu::callGuestNow(*c, __func__, target);
    }
    return;
  }
  const uint32_t pre = tomba::render::PacketPool(*c).cursor();
  c->r[4] = node;
  c->r[5] = flag;
  c->r[31] = nodeCase.dispatchReturn;
  {
    const render::ObjectEffectScope underEffect(objectEffect, nodeCase.effect);
    cmdListDispatch();
  }
  if (nodeCase.effectReturn == 0) {
    return;
  }
  const uint32_t post = tomba::render::PacketPool(*c).cursor();
  // The leaf reads the node after the emission; the effect the emitters saved was read before it.
  if (nodeCaseOf(*c, node, target).effect != nodeCase.effect) {
    lucent::error("render", "node 0x{:08X}: its effect changed while cmdListDispatch emitted it", node);
    std::abort();
  }
  c->r[4] = node;
  c->r[5] = pre;
  c->r[6] = post;
  c->r[31] = nodeCase.effectReturn;
  nodeCase.effect.apply(psx::present::EmitMemory(*c), pre, post);
}

struct BillboardComposeTailArgs {
  uint32_t node;
  uint32_t flag;
  uint32_t outMat = MAT_OUT;
};
// ==================================================================================================
// Shared tail: compose CAM2 (camera rotation+translation) onto WORLD_POS, add it into outMat.t,
// reload CR0-7 from outMat, then hand off to billboardEmit(node, flag).
// outMat = the BUF slot holding the composed matrix. C2D4/C464 land it in MAT_OUT (BUF+0x40); C788
// (billboardCompose3) composes in place in MAT_ROTZ (BUF+0x20) — its gen loads CR0-7 from BUF+0x20,
// not BUF+0x40 — so the tail is parameterized by that slot instead of hardcoding MAT_OUT.
static void billboardComposeTail(Core *c, const BillboardComposeTailArgs &args) {
  auto [node, flag, outMat] = args;
  c->mem_w16(WORLD_POS + 0, c->mem_r16(node + 46));
  c->mem_w16(WORLD_POS + 2, c->mem_r16(node + 50));
  c->mem_w16(WORLD_POS + 4, c->mem_r16(node + 54));
  c->gte().writeControl(0, c->mem_r32(CAM2 + 0));
  c->gte().writeControl(1, c->mem_r32(CAM2 + 4));
  c->gte().writeControl(2, c->mem_r32(CAM2 + 8));
  c->gte().writeControl(3, c->mem_r32(CAM2 + 12));
  c->gte().writeControl(4, c->mem_r32(CAM2 + 16));
  c->gte().writeData(0, c->mem_r32(WORLD_POS + 0));
  c->gte().writeData(1, c->mem_r32(WORLD_POS + 4));
  c->gte().execute(MVMVA_TRANS);
  c->mem_w32(outMat + 0x14, c->gte().readData(25));
  c->mem_w32(outMat + 0x18, c->gte().readData(26));
  c->mem_w32(outMat + 0x1C, c->gte().readData(27));
  c->mem_w32(outMat + 0x14, c->mem_r32(outMat + 0x14) + c->mem_r32(CAM2 + 0x14));
  c->mem_w32(outMat + 0x18, c->mem_r32(outMat + 0x18) + c->mem_r32(CAM2 + 0x18));
  c->mem_w32(outMat + 0x1C, c->mem_r32(outMat + 0x1C) + c->mem_r32(CAM2 + 0x1C));
  c->gte().writeControl(0, c->mem_r32(outMat + 0));
  c->gte().writeControl(1, c->mem_r32(outMat + 4));
  c->gte().writeControl(2, c->mem_r32(outMat + 8));
  c->gte().writeControl(3, c->mem_r32(outMat + 12));
  c->gte().writeControl(4, c->mem_r32(outMat + 16));
  c->gte().writeControl(5, c->mem_r32(outMat + 0x14));
  c->gte().writeControl(6, c->mem_r32(outMat + 0x18));
  c->gte().writeControl(7, c->mem_r32(outMat + 0x1C));
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
  mathOf(c).matMul({.rPtr = MAT_ROTZ, .mPtr = MAT_A, .outPtr = MAT_OUT});
  // gen's live callee-saved state at the guest 0x8003C8F4 call site (L4593-4595): billboardEmit
  // spills these as its "caller" registers, so they must hold gen's values here.
  c->r[16] = MAT_OUT;
  c->r[17] = MAT_A;
  c->r[18] = flag;
  c->r[19] = node;
  c->r[31] = 0x8003C448u;
  billboardComposeTail(c, {.node = node, .flag = flag});
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
  psx::cpu::callGuestNow(*c, __func__, 0x800517BCu);
  mtxOf(c).identity(MAT_ROTZ);
  mathOf(c).rotZ((int16_t)c->mem_r16(node + 90), MAT_ROTZ);
  const uint32_t flag = c->mem_r8(node + 71) & 1u;
  mathOf(c).matMul({.rPtr = MAT_ROTZ, .mPtr = MAT_A, .outPtr = MAT_OUT});
  // gen's live callee-saved state at the guest 0x8003C8F4 call site (L5993-5995) — NOTE C464 differs
  // from C2D4: r17=flag (not MAT_A) and r18=node (gen reassigns r17 to flag at L5931 and keeps
  // r18=node from the prologue). billboardEmit spills these, so match gen exactly.
  c->r[16] = MAT_OUT;
  c->r[17] = flag;
  c->r[18] = node;
  c->r[31] = 0x8003C5E0u;
  billboardComposeTail(c, {.node = node, .flag = flag});
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
  mathOf(c).matMul({.rPtr = node + 152, .mPtr = MAT_A, .outPtr = MAT_ROTZ}); // MAT_ROTZ = (node+152 matrix) x identity
  // gen's live callee-saved state at the billboardEmit call site (abi_extract call [2]): r16=MAT_ROTZ,
  // r17=flag, r18=node. billboardEmit spills these as its caller regs, so match gen exactly.
  c->r[16] = MAT_ROTZ;
  c->r[17] = flag;
  c->r[18] = node;
  c->r[31] = 0x8003C8DCu;
  billboardComposeTail(
      c, {.node = node, .flag = flag, .outMat = MAT_ROTZ}); // same GTE world-translate tail as C2D4/C464, on MAT_ROTZ
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
  mathOf(c).rotMatSoft({.anglesPtr = node + 84, .out = MAT_ROTZ}); // MAT_ROTZ = software RotMatrix(SVECTOR @ node+84)
  const uint32_t flag = c->mem_r8(node + 71) & 1u;
  mathOf(c).matMul({.rPtr = MAT_ROTZ, .mPtr = MAT_A, .outPtr = MAT_OUT});
  // gen's live callee-saved state at the billboardEmit (guest 0x8003C8F4) call site: r16=MAT_OUT,
  // r17=MAT_A, r18=flag, r19=node — identical to C2D4. billboardEmit spills these as its caller regs.
  c->r[16] = MAT_OUT;
  c->r[17] = MAT_A;
  c->r[18] = flag;
  c->r[19] = node;
  c->r[31] = 0x8003C76Cu;
  billboardComposeTail(c, {.node = node, .flag = flag});
  // Epilogue restore (guest 0x8003C5F8 L_8003C76C): read the caller's r16..r19/ra back from the
  // spill slots — same anti-leak discipline as C2D4.
  c->r[16] = c->mem_r32(sp + 16);
  c->r[17] = c->mem_r32(sp + 20);
  c->r[18] = c->mem_r32(sp + 24);
  c->r[19] = c->mem_r32(sp + 28);
  c->r[31] = c->mem_r32(sp + 32);
}

// FUN_8004CC88 (a0=node): each shown instance of the node's table at node+0x6C (count at +0x6A, shown where
// +0x70 has the bit and +0x74 does not) moves the node to the instance and composes it. The instances are one
// object draw, numbered by table index.
void Render::billboardInstances() {
  Core *c = mCore;
  ::GuestFrame<40, 6> frame(c, kInstanceSpills);
  GuestReg<16> nodeReg(c);
  nodeReg = c->r[4];
  const uint32_t node = c->r[16];
  const uint64_t clockProduct = (uint64_t)c->mem_r16(kInstanceClock) * kDivideBy20;
  c->hi = (uint32_t)(clockProduct >> 32);
  c->lo = (uint32_t)clockProduct;
  GuestReg<18> indexReg(c);
  indexReg = 0;
  GuestReg<19> fieldsReg(c);
  fieldsReg = node + kInstanceFields;
  GuestReg<20> scrollReg(c);
  scrollReg = (c->mem_r16(kInstanceClock) % 20u) & 0xFFFEu;
  if ((int16_t)c->mem_r16(node + kInstanceCount) <= 0) {
    return;
  }
  const tomba::render::ObjectDrawScope draw(
      *c, tomba::render::BillboardState::kInstances, eng(c).graphicsBind.records.object(node));
  GuestReg<17> entryReg(c);
  entryReg = c->mem_r32(node + kInstanceTable) + kInstancePosition;
  for (uint32_t index = 0; (int32_t)index < (int16_t)c->mem_r16(node + kInstanceCount); index++) {
    indexReg = index;
    const uint32_t entry = c->r[17];
    const uint32_t bit = 1u << index;
    if ((c->mem_r32(node + kInstanceShown) & bit) != 0 && (c->mem_r32(node + kInstanceHidden) & bit) == 0) {
      c->mem_w8(node + 3, c->mem_r8(entry - 5));
      c->mem_w32(node + 0x2C, (uint32_t)(int32_t)(int16_t)c->mem_r16(entry - 4) << 16);
      c->mem_w32(node + 0x30, (uint32_t)(int32_t)(int16_t)c->mem_r16(entry - 2) << 16);
      c->mem_w32(node + 0x34, (uint32_t)(int32_t)(int16_t)c->mem_r16(entry) << 16);
      c->r[4] = node;
      tomba::guest::dispatchJalToReturn(*c, {.target = kInstancePlace, .returnAddress = 0x8004CD5Cu});
      sessionOf(c).objectDraw.numberNext(index);
      const uint32_t kind = c->mem_r8(node + 3);
      if (kind < 2u || (uint8_t)(kind - 4u) < 7u) {
        if (kind >= 2u) {
          c->mem_w32(node + 0x38, c->mem_r32(node + 0x38) + c->r[20] * 2u);
        }
        c->mem_w8(node + 0xD, c->mem_r8(node + 0xD) & 0xDFu);
        c->r[4] = node;
        tomba::guest::dispatchJalToReturn(
            *c, {.target = 0x8003C2D4u, .returnAddress = kind < 2u ? 0x8004CD84u : 0x8004CDC0u});
      } else {
        c->r[4] = node;
        c->r[5] = 0;
        tomba::guest::dispatchJalToReturn(*c, {.target = kInstanceTurn, .returnAddress = 0x8004CDD4u});
        c->r[4] = node;
        tomba::guest::dispatchJalToReturn(*c, {.target = 0x8003C464u, .returnAddress = 0x8004CDDCu});
      }
    }
    entryReg = entry + kInstanceStride;
  }
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
  // The node's particles are one object: its render record's current life.
  const tomba::render::EmitterObject object(
      *c, tomba::render::BillboardState::kEmitter, tomba::render::OwnObject{eng(c).graphicsBind.records.object(node)});
  tomba::render::BillboardState drawn(c->gte());
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
    psx::cpu::callGuestNow(*c, __func__, 0x8003B220u);

    c->gte().writeData(0, c->mem_r32(FR(16) + 0));
    c->gte().writeData(1, c->mem_r32(FR(16) + 4));
    c->gte().writeData(2, c->mem_r32(FR(16) + 8));
    c->gte().writeData(3, c->mem_r32(FR(16) + 12));
    c->gte().writeData(4, c->mem_r32(FR(16) + 16));
    c->gte().writeData(5, c->mem_r32(FR(16) + 20));
    c->gte().execute(0x4A280030u); // RTPT: project V0-2 -> SXY0-2
    int32_t ctrl31 = (int32_t)c->gte().readControl(31);
    c->mem_w32(FR(48), (uint32_t)ctrl31);
    int32_t depth;
    if (ctrl31 < 0) {
      depth = DEFAULT_DEPTH;
    } else {
      c->mem_w32(BUF + 8, c->gte().projected(0).word());
      c->mem_w32(BUF + 16, c->gte().projected(1).word());
      c->mem_w32(BUF + 24, c->gte().projected(2).word());
      c->gte().execute(0x4B400006u); // AVSZ3
      c->mem_w32(FR(48), c->gte().readData(24));
      c->gte().writeData(0, c->mem_r32(FR(40) + 0));
      c->gte().writeData(1, c->mem_r32(FR(40) + 4));
      c->gte().execute(0x4A180001u); // RTPS: project V3
      ctrl31 = (int32_t)c->gte().readControl(31);
      c->mem_w32(FR(48), (uint32_t)ctrl31);
      if (ctrl31 >= 0) {
        c->mem_w32(BUF + 32, c->gte().projected(2).word());
        c->gte().execute(0x4B68002Eu); // AVSZ4 -> OTZ
        c->mem_w32(FR(52), c->gte().readData(7));
        depth = (int32_t)c->mem_r32(FR(52));
      } else {
        depth = DEFAULT_DEPTH;
      }
    }
    c->mem_w32(FR(56), (uint32_t)depth);

    // 2) Off-screen cull: X over the draw window, Y under 240, unsigned 16-bit as the guest.
    const std::uint32_t xs[4] = {c->mem_r16(BUF + 8), c->mem_r16(BUF + 16), c->mem_r16(BUF + 24), c->mem_r16(BUF + 32)};
    bool onX = tomba::horizontal_cull::forDrawWindow(c).keepsX(xs, 4, tomba::horizontal_cull::Domain::Packed16);
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
      int32_t d = tomba::render::OrderingTable::compressDepth(a);
      c->mem_w32(FR(56), (uint32_t)d);
      if (!tomba::render::OrderingTable::inDepthRange(d)) {
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
    psx::cpu::callGuestNow(*c, __func__, 0x8003B054u);
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
        psx::cpu::callGuestNow(*c, __func__, caseTarget);
        return;
      }
    }

    // 5) Emit the 10-word packet (tag + 9 data words) at the pool tail, prepended into the OT bucket.
    const tomba::render::PacketPool packets(*c);
    uint32_t tail = packets.cursor();
    const uint32_t bucket = c->mem_r32(FR(56));
    tomba::render::OrderingTable::active(*c).link({.packet = tail, .words = 9u, .bucket = bucket});
    tail += 4;
    std::array<uint32_t, tomba::render::BillboardState::kPacketWords> command{};
    for (uint32_t word = 0; word < command.size(); ++word) {
      command[word] = c->mem_r32(BUF + 4u + word * 4u);
      c->mem_w32(tail, command[word]);
      tail += 4;
    }
    packets.setCursor(tail);
    std::array<uint32_t, tomba::render::BillboardState::kCornerWords> corners{};
    for (uint32_t word = 0; word < corners.size(); ++word) {
      corners[word] = c->mem_r32(FR(16) + word * 4u);
    }
    drawn.add(corners, bucket, command);
  }
  drawn.save(*c);
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
void ov_billboardInstances(Core *c) {
  rend(c)->billboardInstances();
}
} // namespace

void perobj_billboard_install(native::NativeOverrideCatalog &natives) {
  // tomba::native::declareOverride (runtime/psx/override_registry.h) installs into the ONE
  // process-global override registry, which runs original guest instructions on the oracle leg (core B) and the
  // native handler everywhere else — NOT a raw tomba::native::declareOverride, since these are engine/game
  // natives and the oracle must run the pure guest body for them.
  natives.declare(0x8003CCA4u, "ov_perObjRenderDispatch", ov_perObjRenderDispatch);
  natives.declare(0x8003C2D4u, "ov_billboardCompose1", ov_billboardCompose1);
  natives.declare(0x8003C464u, "ov_billboardCompose2", ov_billboardCompose2);
  natives.declare(0x8003C788u, "ov_billboardCompose3", ov_billboardCompose3);
  natives.declare(0x8003C5F8u, "ov_billboardComposeC5F8", ov_billboardComposeC5F8);
  natives.declare(tomba::render::BillboardState::kEmitter, "ov_billboardEmit", ov_billboardEmit);
  natives.declare(tomba::render::BillboardState::kInstances, "ov_billboardInstances", ov_billboardInstances);
}

} // namespace tomba
