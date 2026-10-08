// objlist_walk.cpp — SUBSTRATE MIRROR for the 4 still-substrate OBJECT-LIST WALKERS reached from the
// field draw dispatcher FUN_8003F9A8 (docs/findings/render.md "0x8003F9A8 474-prim attribution
// resolved"): FUN_8003BB50, FUN_8003BCF4, FUN_8003BF00,
// FUN_8003EEC0. Everything BELOW them (FUN_8003CCA4/C2D4/C464 = Render::perObjRenderDispatch/
// billboardCompose1/billboardCompose2) is already native — these 4 are the last unowned hop between
// the field dispatcher and that already-owned chain, so the 474 prims the otattr shadow stack was
// mis-crediting to FUN_8003F9A8 attribute correctly once these are owned.
//
// RE method: authenticated executable/overlay evidence (the recorded guest instruction listing) is ground truth, used
// DIRECTLY (Ghidra's pseudo-C reads FUN_8003BCF4's jump table as a set of functions; it is a set of labels
// inside one loop — see the FUN_8003BCF4 banner below). Cross-checked against scratch/decomp/otattr_subs.c
// (Ghidra headless dump) for the case-value semantics, which the recorded binary evidence's switch tables (read
// as REAL indirect jump-table data at each function's fixed table address) independently confirm.
// All 5 addresses confirmed unowned via tools/codemap.py before porting.
//
// COMMON SHAPE (BB50/BF00/EEC0 — self-contained single-function loops):
//   Each walks a fixed list of guest object pointers (an array for BB50/BCF4/BF00, a genuine
//   node+0x24-linked chain for EEC0), skipping dead entries (mem8(ptr+1)==0) and out-of-range TYPE
//   bytes (mem8(ptr+0xb)), and for every live in-range entry reads a REAL indirect jump table (fixed
//   guest address, static ROM data) to get a target address, then switches on that target: known
//   local labels run this function's OWN per-object work (mostly calling into the already-native
//   perObjRenderDispatch/billboardCompose1/billboardCompose2, or a still-substrate leaf via a plain
//   the cited guest address(c) call, exactly as gen does); an unrecognized target hits the recorded binary evidence's
//   defensive fallback (`typed runtime address dispatch(c, target); return;` — a full return bypassing the frame
//   epilogue, never hit by live game data since the live table only ever holds the enumerated case
//   values). BB50/BCF4 additionally maintain a per-field-frame "already refreshed this frame" cursor
//   pair in scratchpad (flag @0x1F800136, shared by all list walkers) that snapshots the list's
//   current head/count into a scratchpad working pair on the FIRST walker call each field frame and
//   leaves it alone on subsequent calls within the same frame — reproduced verbatim below (mem_r16/
//   mem_w16/mem_r32/mem_w32 at the literal scratchpad offsets; no magic constants, every offset is the
//   literal `authenticated executable/overlay evidence` operand).
//
// FUN_8003BCF4 — ONE function, ONE loop. Ghidra's pseudo-C and an earlier port both read the 33-entry
//   table at 0x80014CB0 as pointing at FUNCTIONS; it points at LABELS. Every target is inside this body
//   (0x8003BDAC..0x8003BEC8: one to five instructions, `move a0,s0` + one leaf call, then `j 0x8003bed8`)
//   or is the loop-continue label 0x8003BED8 itself (`bnez s1, loop; epilogue; jr ra`). The whole RAM
//   image holds exactly 11 jumps to 0x8003BED8, all `j` (never `jal`), all inside 0x8003BDAC..0x8003BEAC,
//   and no jump to the loop head 0x8003BD6C at all — so nothing outside this function ever enters the
//   tail, and the tail must NOT be a native override: an override "returns" to its entry r31, and for a
//   `j`-entered label that r31 is the stub's own `j` (set by the stub's preceding `jal`), so the tail was
//   re-entered AFTER its epilogue had popped the frame and walked the caller's garbage s1/s2 —
//   hut-entry-alt.pad f459, UNMAPPED read8 @0x01000001 with r17=0x1F7FFFF6, r18=0x29, ra=0x8003F9E8.
//   The loop state (r16=object, r17=remaining, r18=cursor, r19=0x800C0000, r20=table) lives in c->r[]
//   itself, never a C++ local: the leaves this loop reaches spill those callee-saved registers as their
//   caller state, so a stale value would reach guest RAM (perobj_dispatch.cpp's CmdListFrame banner).
#include "core.h"
#include "core/entry/game_ctx.h"
#include "core/overrides/native_override_catalog.h"
#include "game.h"
#include "guest_call.h"
#include "render.h"
#include <cstdint>
// original guest instructions fallbacks for the oracle-gated thunk — SBS core B (the pure oracle) must keep running the
// real guest body; see render_walk_dispatch.cpp's identical banner for the full rationale.

namespace {
// Shared "already refreshed this field frame" flag (BB50/BCF4 both gate their cursor-refresh on it;
// some OTHER still-substrate walker not yet owned by this cluster clears it once per field frame).
constexpr uint32_t FRAME_FRESH_FLAG = 0x1F800136u;

// ---- FUN_8003BB50 -------------------------------------------------------------------------------
constexpr uint32_t W1_PTR_A = 0x1F80013Cu; // persistent cursor pointer (reset to LIST_HEAD each frame)
constexpr uint32_t W1_CNT_A = 0x1F800144u; // persistent cursor count  (reset to 0 each frame)
constexpr uint32_t W1_PTR_B = 0x1F800140u; // this-call working pointer (= old W1_PTR_A on refresh)
constexpr uint32_t W1_CNT_B = 0x1F800146u; // this-call working count   (= old W1_CNT_A on refresh)
constexpr uint32_t W1_LIST_HEAD = 0x800F2410u;
constexpr uint32_t W1_TABLE = 0x80014A70u; // 144-entry (idx<144) target-address table

// ---- FUN_8003BCF4 / FUN_8003BED8 ----------------------------------------------------------------
constexpr uint32_t W2_PTR_A = 0x1F800148u;
constexpr uint32_t W2_CNT_A = 0x1F800150u;
constexpr uint32_t W2_PTR_B = 0x1F80014Cu;
constexpr uint32_t W2_CNT_B = 0x1F800152u;
constexpr uint32_t W2_LIST_HEAD = 0x800F26C8u;
constexpr uint32_t W2_TABLE = 0x80014CB0u; // 33-entry (idx<33) target-address table

// ---- FUN_8003BF00 --------------------------------------------------------------------------------
constexpr uint32_t W3_PTR_A = 0x1F800154u;
constexpr uint32_t W3_CNT_A = 0x1F80015Cu;
constexpr uint32_t W3_PTR_B = 0x1F800158u;
constexpr uint32_t W3_CNT_B = 0x1F80015Eu;
constexpr uint32_t W3_LIST_HEAD = 0x800F2738u;
constexpr uint32_t W3_TABLE = 0x80014D38u;     // 32-entry (idx<32) target-address table
constexpr uint32_t W3_MODE_BYTE = 0x800BF870u; // render-mode-select byte (shared with perModeDispatch)

// ---- FUN_8003EEC0 --------------------------------------------------------------------------------
constexpr uint32_t W4_LIST_HEAD_VAR = 0x800F2738u; // *this = head-of-chain object pointer (SAME
                                                   // storage FUN_8003BF00 treats as an array base —
                                                   // here dereferenced ONCE for the chain head)
constexpr uint32_t W4_TABLE = 0x80015000u;         // 33-entry (idx<33) target-address table
} // namespace

// ===================================================================================================
// FUN_8003BB50 (Render::objListWalk1) — no args (guest ABI).
// ORACLE: guest 0x8003BB50
void Render::objListWalk1() {
  Core *c = mCore;
  // Real -40 guest frame (RE: guest 0x8003BB50 prologue) — spills r16/r17/r18/r19/ra.
  const uint32_t s16 = c->r[16], s17 = c->r[17], s18 = c->r[18], s19 = c->r[19], sra = c->r[31];
  c->r[29] -= 40;
  c->mem_w32(c->r[29] + 32, sra);
  c->mem_w32(c->r[29] + 28, s19);
  c->mem_w32(c->r[29] + 24, s18);
  c->mem_w32(c->r[29] + 20, s17);
  c->mem_w32(c->r[29] + 16, s16);

  if (c->mem_r8(FRAME_FRESH_FLAG) == 0) {
    const uint16_t oldCnt = c->mem_r16(W1_CNT_A);
    const uint32_t oldPtr = c->mem_r32(W1_PTR_A);
    c->mem_w16(W1_CNT_A, 0);
    c->mem_w32(W1_PTR_A, W1_LIST_HEAD);
    c->mem_w16(W1_CNT_B, oldCnt);
    c->mem_w32(W1_PTR_B, oldPtr);
  }
  // Live loop state lives in c->r[] itself (register-faithfulness — see CLAUDE.md "MIRROR THE GUEST
  // STACK"): r17=remaining count, r18=list cursor pointer, r16=current object ptr, r19=table base
  // (loop-invariant, set once). gen keeps all four LIVE in the real callee-saved registers across every
  // nested dispatch; the still-substrate leaves this loop reaches (guest 0x8002AE0C, and the typed runtime address
  // dispatch vtable targets) SPILL them to their own guest-stack frames as "caller state". Keeping them only in C++
  // locals was a real, reproducible bug: gen's r19=0x80014A70 (the table base) was being spilled by a downstream leaf
  // as native's stale 0 — the exact SBS diff at 0x801FE8C4/E4 (A=0 B=80014A70), f119..f156, healed once r19 is set
  // here. (Found via bisected SBS-full; baseline forced-gen = 0-diff.)
  c->r[17] = (uint32_t)(int16_t)c->mem_r16(W1_CNT_B);
  c->r[18] = c->mem_r32(W1_PTR_B);
  if (c->r[17] == 0u) {
    goto epilogue;
  }
  c->r[19] = W1_TABLE;

  while (c->r[17] != 0u) {
    c->r[16] = c->mem_r32(c->r[18]);
    c->r[18] += 4;
    c->r[17]--;
    const uint32_t cmd = c->r[16];
    if (c->mem_r8(cmd + 1) == 0) {
      continue;
    }
    const uint32_t type = c->mem_r8(cmd + 0xB);
    if (type >= 144u) {
      continue;
    }
    const uint32_t target = c->mem_r32(c->r[19] + type * 4u);
    c->r[4] = cmd;
    switch (target) {
    case 0x8003BC00u: {
      c->r[31] = 0x8003BC08u;
      perObjRenderDispatch();
      const uint32_t attr = c->mem_r8(cmd + 0xB);
      if ((attr & 0x40u) == 0u) {
        if ((attr & 0x80u) == 0u) {
          break;
        }
        c->r[4] = cmd;
        c->r[5] = (uint32_t)c->mem_r16s(cmd + 0x80u);
      } else {
        c->r[4] = cmd;
        c->r[5] = 0x50u;
      }
      c->r[31] = 0x8003BC64u;
      c->r[6] = 0u;
      psx::cpu::dispatchGuestToReturn0(*c, 0x8002AE0Cu, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
      break;
    }
    case 0x8003BC24u: {
      c->r[31] = 0x8003BC2Cu;
      psx::cpu::dispatchGuestToReturn0(*c, 0x80122974u, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
      const uint32_t attr = c->mem_r8(cmd + 0xB);
      if ((attr & 0x40u) == 0u) {
        if ((attr & 0x80u) == 0u) {
          break;
        }
        c->r[4] = cmd;
        c->r[5] = (uint32_t)c->mem_r16s(cmd + 0x80u);
      } else {
        c->r[4] = cmd;
        c->r[5] = 0x50u;
      }
      c->r[31] = 0x8003BC64u;
      c->r[6] = 0u;
      psx::cpu::dispatchGuestToReturn0(*c, 0x8002AE0Cu, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
      break;
    }
    case 0x8003BC6Cu:
      c->r[31] = 0x8003BC74u;
      billboardCompose1();
      break;
    case 0x8003BC7Cu:
      c->r[31] = 0x8003BC84u;
      billboardCompose2();
      break;
    case 0x8003BC8Cu:
      c->r[31] = 0x8003BC94u;
      psx::cpu::dispatchGuestToReturn0(*c, 0x8003C5F8u, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
      break;
    case 0x8003BC9Cu:
      c->r[31] = 0x8003BCA4u;
      psx::cpu::dispatchGuestToReturn0(*c, 0x8003C788u, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
      break;
    case 0x8003BCACu:
      c->r[31] = 0x8003BCB4u;
      billboardCompose1();
      [[fallthrough]]; // gen: L_8003BCAC falls straight into L_8003BCB4 (no separate advance)
    case 0x8003BCB4u: {
      // L_8003BCB4 is ALSO a directly-reachable switch target (Ghidra's case 0x16, vtable+0x7C
      // WITHOUT the preceding billboardCompose1 call BCAC/case-0x15 makes) — a genuinely separate
      // table entry, not merely BCAC's fallthrough tail. Read cmd+124 either way.
      const uint32_t vt = c->mem_r32(cmd + 124u);
      c->r[31] = 0x8003BCD0u;
      c->r[4] = cmd;
      psx::cpu::dispatchGuestToReturn0(*c, vt, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
      break;
    }
    case 0x8003BCC0u: {
      const uint32_t vt = c->mem_r32(cmd + 24u);
      c->r[31] = 0x8003BCD0u;
      c->r[4] = cmd;
      psx::cpu::dispatchGuestToReturn0(*c, vt, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
      break;
    }
    case 0x8003BCD0u:
      break; // no-op table entry: skip (matches gen's dedicated loop-continue case value)
    default:
      // Defensive mirror of the recorded binary evidence's indirect-jump fallback (authenticated executable/overlay
      // evidence's `default: typed runtime address dispatch(c, c->r[2]); return;`) — a full RETURN bypassing the frame
      // epilogue. Never hit by live game data: the live 144-slot table only ever holds the case values above.
      psx::cpu::dispatchGuestToReturn0(*c, target, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
      return;
    }
  }
epilogue:
  c->r[31] = c->mem_r32(c->r[29] + 32);
  c->r[19] = c->mem_r32(c->r[29] + 28);
  c->r[18] = c->mem_r32(c->r[29] + 24);
  c->r[17] = c->mem_r32(c->r[29] + 20);
  c->r[16] = c->mem_r32(c->r[29] + 16);
  c->r[29] += 40;
}

// ===================================================================================================
// FUN_8003BCF4 (Render::objListWalk2) — no args (guest ABI). See the file banner: the table targets are
// labels in this body, reproduced as the switch arms below with the exact r31 each stub's `jal` leaves.
// ORACLE: guest 0x8003BCF4
void Render::objListWalk2() {
  Core *c = mCore;
  // Real -40 guest frame (RE: guest 0x8003BCF4 prologue) — spills r16/r17/r18/r19/r20/ra.
  const uint32_t s16 = c->r[16], s17 = c->r[17], s18 = c->r[18], s19 = c->r[19], s20 = c->r[20], sra = c->r[31];
  c->r[29] -= 40;
  c->mem_w32(c->r[29] + 36, sra);
  c->mem_w32(c->r[29] + 32, s20);
  c->mem_w32(c->r[29] + 28, s19);
  c->mem_w32(c->r[29] + 24, s18);
  c->mem_w32(c->r[29] + 20, s17);
  c->mem_w32(c->r[29] + 16, s16);

  if (c->mem_r8(FRAME_FRESH_FLAG) == 0) {
    const uint16_t oldCnt = c->mem_r16(W2_CNT_A);
    const uint32_t oldPtr = c->mem_r32(W2_PTR_A);
    c->mem_w16(W2_CNT_A, 0);
    c->mem_w32(W2_PTR_A, W2_LIST_HEAD);
    c->mem_w16(W2_CNT_B, oldCnt);
    c->mem_w32(W2_PTR_B, oldPtr);
  }
  c->r[17] = (uint32_t)(int16_t)c->mem_r16(W2_CNT_B);
  c->r[18] = c->mem_r32(W2_PTR_B);
  if (c->r[17] == 0u) {
    goto epilogue;
  }
  c->r[19] = 0x800C0000u; // s3: the area-overlay arms read the mode byte as `lbu v1,-0x790(s3)` (0x800BF870)
  c->r[20] = W2_TABLE;

  while (c->r[17] != 0u) {
    c->r[16] = c->mem_r32(c->r[18]);
    c->r[18] += 4;
    c->r[17]--;
    const uint32_t cmd = c->r[16];
    if (c->mem_r8(cmd + 1) == 0u) {
      continue;
    }
    const uint32_t type = c->mem_r8(cmd + 0xB);
    if (type >= 33u) {
      continue;
    }
    const uint32_t target = c->mem_r32(c->r[20] + type * 4u);
    switch (target) {
    case 0x8003BDACu: // the mesh flush
      c->r[31] = 0x8003BDB4u;
      c->r[4] = cmd;
      perObjRenderDispatch();
      break;
    case 0x8003BDBCu: { // area-overlay renderer chosen by the mode byte
      const uint32_t mode = c->mem_r8(c->r[19] - 0x790u);
      if (mode == 0u) {
        c->r[31] = 0x8003BDD4u;
        c->r[4] = cmd;
        psx::cpu::dispatchGuestToReturn0(*c, 0x801341E8u, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
      } else if (mode == 6u) {
        c->r[31] = 0x8003BDECu;
        c->r[4] = cmd;
        psx::cpu::dispatchGuestToReturn0(*c, 0x80123C14u, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
      }
      break;
    }
    case 0x8003BDF4u: { // area-overlay renderer chosen by the mode byte
      const uint32_t mode = c->mem_r8(c->r[19] - 0x790u);
      uint32_t leaf = 0u, ra = 0u;
      switch (mode) {
      case 1u:
        leaf = 0x80129114u;
        ra = 0x8003BE0Cu;
        break;
      case 6u:
        leaf = 0x80120D2Cu;
        ra = 0x8003BE24u;
        break;
      case 7u:
        leaf = 0x8011AD44u;
        ra = 0x8003BE3Cu;
        break;
      case 0xAu:
        leaf = 0x80115338u;
        ra = 0x8003BE54u;
        break;
      case 0xFu:
        leaf = 0x80117984u;
        ra = 0x8003BE6Cu;
        break;
      default:
        break;
      }
      if (leaf != 0u) {
        c->r[31] = ra;
        c->r[4] = cmd;
        psx::cpu::dispatchGuestToReturn0(*c, leaf, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
      }
      break;
    }
    case 0x8003BE74u:
      c->r[31] = 0x8003BE7Cu;
      c->r[4] = cmd;
      psx::cpu::dispatchGuestToReturn0(*c, 0x80136748u, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
      break;
    case 0x8003BE84u:
      c->r[31] = 0x8003BE8Cu;
      c->r[4] = cmd;
      billboardCompose1();
      break;
    case 0x8003BE94u: { // vtable +0x7C, then falls into the billboardCompose2 arm
      const uint32_t vt = c->mem_r32(cmd + 0x7Cu);
      c->r[31] = 0x8003BEA4u;
      c->r[4] = cmd;
      psx::cpu::dispatchGuestToReturn0(*c, vt, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
    }
      [[fallthrough]];
    case 0x8003BEA4u:
      c->r[31] = 0x8003BEACu;
      c->r[4] = cmd;
      billboardCompose2();
      break;
    case 0x8003BEB4u: // billboardCompose1, then falls into the vtable +0x7C arm
      c->r[31] = 0x8003BEBCu;
      c->r[4] = cmd;
      billboardCompose1();
      [[fallthrough]];
    case 0x8003BEBCu: {
      const uint32_t vt = c->mem_r32(cmd + 0x7Cu);
      c->r[31] = 0x8003BED8u;
      c->r[4] = cmd;
      psx::cpu::dispatchGuestToReturn0(*c, vt, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
      break;
    }
    case 0x8003BEC8u: { // vtable +0x18 (the type-0x20 custom-render fn)
      const uint32_t vt = c->mem_r32(cmd + 0x18u);
      c->r[31] = 0x8003BED8u;
      c->r[4] = cmd;
      psx::cpu::dispatchGuestToReturn0(*c, vt, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
      break;
    }
    case 0x8003BED8u:
      break; // loop-continue table entry: nothing is drawn
    default:
      // Defensive mirror of the guest's `jr v0` to a target outside this body — a full RETURN bypassing
      // the frame epilogue. Never hit by live game data: the live 33-slot table only ever holds the
      // labels above.
      psx::cpu::dispatchGuestToReturn0(*c, target, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
      return;
    }
  }
epilogue:
  c->r[31] = c->mem_r32(c->r[29] + 36);
  c->r[20] = c->mem_r32(c->r[29] + 32);
  c->r[19] = c->mem_r32(c->r[29] + 28);
  c->r[18] = c->mem_r32(c->r[29] + 24);
  c->r[17] = c->mem_r32(c->r[29] + 20);
  c->r[16] = c->mem_r32(c->r[29] + 16);
  c->r[29] += 40;
}

// ===================================================================================================
// FUN_8003BF00 (Render::objListWalk3) — no args (guest ABI).
// ORACLE: guest 0x8003BF00
void Render::objListWalk3() {
  Core *c = mCore;
  // Real -32 guest frame (RE: guest 0x8003BF00 prologue) — spills r16/r17/r18/ra.
  const uint32_t s16 = c->r[16], s17 = c->r[17], s18 = c->r[18], sra = c->r[31];
  c->r[29] -= 32;
  c->mem_w32(c->r[29] + 28, sra);
  c->mem_w32(c->r[29] + 24, s18);
  c->mem_w32(c->r[29] + 20, s17);
  c->mem_w32(c->r[29] + 16, s16);

  if (c->mem_r8(FRAME_FRESH_FLAG) == 0) {
    const uint16_t oldCnt = c->mem_r16(W3_CNT_A);
    const uint32_t oldPtr = c->mem_r32(W3_PTR_A);
    c->mem_w16(W3_CNT_A, 0);
    c->mem_w32(W3_PTR_A, W3_LIST_HEAD);
    c->mem_w16(W3_CNT_B, oldCnt);
    c->mem_w32(W3_PTR_B, oldPtr);
  }
  // Live loop state in c->r[] (register-faithfulness, same rationale as objListWalk1): gen keeps
  // r16=remaining count, r17=list cursor pointer, r18=table base (loop-invariant) LIVE across every
  // nested dispatch (authenticated executable/overlay evidence/5063/5065) — downstream substrate leaves spill them.
  c->r[16] = (uint32_t)(int16_t)c->mem_r16(W3_CNT_B);
  c->r[17] = c->mem_r32(W3_PTR_B);
  if (c->r[16] == 0u) {
    goto epilogue;
  }
  c->r[18] = W3_TABLE;

  while (c->r[16] != 0u) {
    const uint32_t cmd = c->mem_r32(c->r[17]);
    c->r[17] += 4;
    c->r[16]--;
    if (c->mem_r8(cmd + 1) == 0) {
      continue;
    }
    const uint32_t type = c->mem_r8(cmd + 0xB);
    if (type >= 32u) {
      continue;
    }
    const uint32_t target = c->mem_r32(c->r[18] + type * 4u);
    switch (target) {
    case 0x8003BFACu:
      c->r[31] = 0x8003BFB4u;
      c->r[4] = cmd;
      perObjRenderDispatch();
      break;
    case 0x8003BFBCu:
      c->r[31] = 0x8003BFC4u;
      c->r[4] = cmd;
      billboardCompose1();
      break;
    case 0x8003BFCCu:
      c->r[31] = 0x8003BFD4u;
      c->r[4] = cmd;
      billboardCompose2();
      break;
    case 0x8003BFDCu:
      c->r[31] = 0x8003BFE4u;
      c->r[4] = cmd;
      psx::cpu::dispatchGuestToReturn0(*c, 0x8003C5F8u, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
      break;
    case 0x8003BFECu:
      c->r[31] = 0x8003BFF4u;
      c->r[4] = cmd;
      psx::cpu::dispatchGuestToReturn0(*c, 0x8003C788u, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
      break;
    case 0x8003BFFCu: {
      if (c->mem_r8(W3_MODE_BYTE) == 0x14u) {
        c->r[31] = 0x8003C018u;
        c->r[4] = cmd;
        psx::cpu::dispatchGuestToReturn0(*c, 0x8010FC70u, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
      } else {
        c->r[31] = 0x8003C028u;
        c->r[4] = cmd;
        psx::cpu::dispatchGuestToReturn0(*c, 0x8004CC88u, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
      }
      break;
    }
    case 0x8003C028u:
      break; // no-op table entry: skip (matches gen's dedicated loop-continue case value)
    default:
      // Defensive mirror of the recorded binary evidence's indirect-jump fallback (authenticated executable/overlay
      // evidence's `default: typed runtime address dispatch(c, c->r[2]); return;`) — a full RETURN bypassing the frame
      // epilogue.
      c->r[4] = cmd;
      psx::cpu::dispatchGuestToReturn0(*c, target, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
      return;
    }
  }
epilogue:
  c->r[31] = c->mem_r32(c->r[29] + 28);
  c->r[18] = c->mem_r32(c->r[29] + 24);
  c->r[17] = c->mem_r32(c->r[29] + 20);
  c->r[16] = c->mem_r32(c->r[29] + 16);
  c->r[29] += 32;
}

// ===================================================================================================
// FUN_8003EEC0 (Render::objListWalk4) — no args (guest ABI). Walks a genuine singly-linked chain
// (node+0x24 "next"), NOT a positional array — no scratchpad cursor, no per-frame refresh flag; the
// chain head is re-read fresh from W4_LIST_HEAD_VAR every call.
// ORACLE: guest 0x8003EEC0
void Render::objListWalk4() {
  Core *c = mCore;
  // Real -32 guest frame (RE: guest 0x8003EEC0 prologue) — spills r16/r17/r18/ra.
  const uint32_t s16 = c->r[16], s17 = c->r[17], s18 = c->r[18], sra = c->r[31];
  c->r[29] -= 32;
  c->mem_w32(c->r[29] + 28, sra);
  c->mem_w32(c->r[29] + 24, s18);
  c->mem_w32(c->r[29] + 20, s17);
  c->mem_w32(c->r[29] + 16, s16);

  // Live loop state in c->r[] (register-faithfulness, same rationale as objListWalk1): gen keeps
  // r16=current node, r17=next node, r18=table base LIVE across every nested dispatch
  // (authenticated executable/overlay evidence/10996/10999) — downstream substrate leaves (guest 0x8003B704, the
  // typed runtime address dispatch vtable targets) spill them.
  c->r[16] = c->mem_r32(W4_LIST_HEAD_VAR);
  if (c->r[16] != 0u) {
    c->r[18] = W4_TABLE;
    while (c->r[16] != 0u) {
      const uint32_t node = c->r[16];
      const uint32_t active = c->mem_r8(node + 1u);
      c->r[17] = c->mem_r32(node + 0x24u); // next: ALWAYS loaded, regardless of active/type
      if (active == 0u) {
        c->r[16] = c->r[17];
        continue;
      }
      const uint32_t type = c->mem_r8(node + 0xB);
      if (type >= 33u) {
        c->r[16] = c->r[17];
        continue;
      }
      const uint32_t target = c->mem_r32(c->r[18] + type * 4u);
      switch (target) {
      case 0x8003EF20u:
        c->r[31] = 0x8003EF28u;
        c->r[4] = node;
        perObjRenderDispatch();
        c->r[16] = c->r[17];
        continue;
      case 0x8003EF30u:
        c->r[31] = 0x8003EF38u;
        c->r[4] = node;
        perObjRenderDispatch();
        c->r[31] = 0x8003EF60u;
        c->r[4] = node;
        psx::cpu::dispatchGuestToReturn0(*c, 0x8003B704u, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
        c->r[16] = c->r[17];
        continue;
      case 0x8003EF40u: {
        c->r[31] = 0x8003EF48u;
        c->r[4] = node;
        billboardCompose1();
        if (c->mem_r8(node + 2u) != 1u) {
          c->r[16] = c->r[17];
          continue;
        }
        c->r[31] = 0x8003EF60u;
        c->r[4] = node;
        psx::cpu::dispatchGuestToReturn0(*c, 0x8003B704u, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
        c->r[16] = c->r[17];
        continue;
      }
      case 0x8003EF68u: {
        const uint32_t vt = c->mem_r32(node + 24u);
        c->r[31] = 0x8003EF78u;
        c->r[4] = node;
        psx::cpu::dispatchGuestToReturn0(*c, vt, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
        c->r[16] = c->r[17];
        continue;
      }
      case 0x8003EF78u:
        c->r[16] = c->r[17];
        continue; // no-op table entry: skip (matches gen's dedicated loop-continue case value)
      default:
        // Defensive mirror of the recorded binary evidence's indirect-jump fallback (authenticated executable/overlay
        // evidence's `default: typed runtime address dispatch(c, c->r[2]); return;`) — a full RETURN bypassing the
        // frame epilogue.
        c->r[4] = node;
        psx::cpu::dispatchGuestToReturn0(*c, target, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
        return;
      }
    }
  }
  c->r[31] = c->mem_r32(c->r[29] + 28);
  c->r[18] = c->mem_r32(c->r[29] + 24);
  c->r[17] = c->mem_r32(c->r[29] + 20);
  c->r[16] = c->mem_r32(c->r[29] + 16);
  c->r[29] += 32;
}

// ===================================================================================================
namespace {
void ov_objListWalk1(Core *c) {
  rend(c)->objListWalk1();
}
void ov_objListWalk2(Core *c) {
  rend(c)->objListWalk2();
}
void ov_objListWalk3(Core *c) {
  rend(c)->objListWalk3();
}
void ov_objListWalk4(Core *c) {
  rend(c)->objListWalk4();
}
} // namespace

// ORACLE-PURITY: installed via tomba::native::declareOverride (never the raw tomba::native::declareOverride), so SBS
// core B (the pure original guest instructions oracle) always runs the real guest body while core A / standalone
// runs these native methods — see perobj_dispatch.cpp's identical banner for the full rationale.
void objlist_walk_install() {
  static bool done = false;
  if (done) {
    return;
  }
  done = true;
  tomba::native::declareOverride(0x8003BB50u, "ov_objListWalk1", ov_objListWalk1);
  tomba::native::declareOverride(0x8003BCF4u, "ov_objListWalk2", ov_objListWalk2);
  tomba::native::declareOverride(0x8003BF00u, "ov_objListWalk3", ov_objListWalk3);
  tomba::native::declareOverride(0x8003EEC0u, "ov_objListWalk4", ov_objListWalk4);
}
