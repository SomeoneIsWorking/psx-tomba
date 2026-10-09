// game/render/wide_re_libgpu_leaves.cpp — WIDE-RE DRAFT leaves of the libgpu "GPU sys" jump table
// documented in docs/engine_re.md ("Graphics pipeline — the REAL draw path (libgpu)"). The table
// lives at guest 0x800A5998 (32778<<16 + 22936); the per-frame loop FUN_80050b08 already names two
// of this file's addresses from that doc. Wide-RE tier (docs/fleet-workflow.md §6): UNWIRED /
// UNVERIFIED, hand-transliterated 1:1 from authenticated executable/overlay evidence the cited guest instructions
// (ground truth — NOT mechanically diffed yet). Nothing here is called from anywhere (no tomba::native::declareOverride
// registration, no tomba::native::declareOverride) — dead code that only needs to COMPILE. A wiring pass MUST re-diff
// every line against the generated C before registering + SBS-gating (per §9).
//
// Struct map confirmed this session (base = 0x800A0000, i.e. `(32778u<<16)`):
//   +22936 (0x5998)  GPU_SYS: fn-ptr jump table (per docs/engine_re.md: +0x08 DMA-send, +0x14
//                     DrawOTagEnv, +0x18 DrawOTag, +0x2C ClearOTagR, +0x3C DrawSync)
//   +22940 (0x599C)  GPU_SYS_INIT: a SECOND fn-ptr, called by DrawSync/ClearOTagR/PutDrawEnv only
//                     while the flag byte @+22946 is >= 2 (POLARITY CORRECTED 2026-07-10 — the
//                     original "< 2 / one-time init" reading was inverted; it's a steady-state
//                     "GPU sys is up" hook), not itself in the jump table.
//   +22944 (0x59A0)..+22950 (0x59A6): small ints/shorts read by guest 0x80081FB0 (PutDrawEnv helper,
//                     NOT drafted this session — see MAP note below); +22946 is the same boot/reset
//                     flag byte DrawSync/ClearOTagR both gate on.
//   +23208 (0x5AA8)  GPU_DMA_FLAGS:  status/flags word pointer (indirect: the struct itself holds a
//                     POINTER, dereferenced before use) — bit 0x00000002 (1024u<<16 | 2, see
//                     gpuDmaQueueReset below) and bit 0x10000000 are tested by the queue cluster
//                     (0x80082D04/0x80082FB4/0x80083364/0x80082424 — NOT drafted, see MAP note).
//   +23212 (0x5AAC)  GPU_DMA_ARG0:   pointer, stores the caller's r4 arg (gpuDmaQueueReset).
//   +23216 (0x5AB0)  GPU_DMA_ARG1:   pointer, zeroed at reset.
//   +23220 (0x5AB4)  GPU_DMA_STATE:  pointer, flags word — reset writes (256u<<16 | 1025) = 0x01000401.
//   +23260 (0x5ADC), +23264 (0x5AE0): the SAME two fields runtime/psx/sync_overrides.cpp's
//                     `gpu_timeout_arm`/`gpu_timeout_chk` already own as the libgpu GPU-DMA-completion
//                     TIMEOUT (arm/check) — CONFIRMS this whole struct is the libgpu OT-DMA-send
//                     status block, and the 0x80082D04 queue cluster is its interrupt/completion-
//                     callback ring buffer (64 slots, stride 0x60, base 0x80100000+0xC30). UPDATE
//                     (2026-07-10, dedicated deep-RE pass): DRAFTED — see
//                     game/render/wide_re_gpu_dma_queue.cpp and docs/engine_re.md.
//
// MAP-only this session (identified, NOT drafted — too large / too deep a callee chain):
//   0x800815D0 = PutDrawEnv (CONFIRMED identity, already named in docs/engine_re.md). Calls
//     guest 0x80081FB0 (40-line struct-pack helper) which itself calls 5 more unowned leaves
//     (0x80082240, 0x800822D8, 0x80082370, 0x80082220, 0x8008238C) — a proper port needs those RE'd
//     first. Left for a dedicated frontier pass; this file only covers the two CONFIRMED single-leaf
//     table entries (DrawSync, ClearOTagR) plus the queue-reset helper.
//   0x80082D04, 0x80082FB4, 0x80083364, 0x80082424 — the GPU-DMA completion callback queue. UPDATE
//     (2026-07-10, dedicated deep-RE pass): DRAFTED in game/render/wide_re_gpu_dma_queue.cpp — see
//     that file's header for the corrected field map (this comment's head/tail addresses,
//     0x800A5A88/0x800A5A8C, were a wrong guess; ground truth is 0x800A5AC8/0x800A5ACC) and call
//     graph. 0x80082734 turned out NOT to be part of this cluster (a separate, larger LoadImage-style
//     FIFO streamer) — still MAPPED only, see the new file's header.
#include "core.h"
#include "core/entry/game_ctx.h"
#include "core/overrides/native_override_catalog.h" // tomba::native::declareOverride — declared, not locally extern'd
#include "guest_call.h"
#include "libgpu_draw_mode.h"
#include "render.h"
#include <stdint.h>

namespace {
constexpr uint32_t GPU_SYS_BASE = (32778u << 16);            // 0x800A0000
constexpr uint32_t GPU_SYS_TABLE = GPU_SYS_BASE + 22936;     // 0x800A5998 — libgpu fn-ptr jump table
constexpr uint32_t GPU_SYS_INIT_FN = GPU_SYS_BASE + 22940;   // 0x800A599C — one-time init hook fn-ptr
constexpr uint32_t GPU_BOOT_FLAG = GPU_SYS_BASE + 22946;     // 0x800A59A2 — boot/reset flag byte
constexpr uint32_t GPU_DMA_FLAGS_PTR = GPU_SYS_BASE + 23208; // 0x800A5AA8
constexpr uint32_t GPU_DMA_ARG0_PTR = GPU_SYS_BASE + 23212;  // 0x800A5AAC
constexpr uint32_t GPU_DMA_ARG1_PTR = GPU_SYS_BASE + 23216;  // 0x800A5AB0
constexpr uint32_t GPU_DMA_STATE_PTR = GPU_SYS_BASE + 23220; // 0x800A5AB4
} // namespace

// guest 0x80080F6C (0x80080F6C) — DrawSync(mode). VERIFIED & WIRED 2026-07-10 (was DRAFT). RE'd from authenticated
// executable/overlay evidence guest 0x80080F6C (25 gen-C ln). CONFIRMED identity via docs/engine_re.md's per-frame-loop
// RE ("FUN_80080f6c(0) = DrawSync(0)
// // WAIT for previous frame's draw to finish"). Guest ABI: a0=mode (arg not read by this leaf body
// itself — passed straight through to the callee as the 2nd dispatch's a1).
//
// When the boot/reset flag byte @GPU_BOOT_FLAG is >= 2: call the hook GPU_SYS_INIT_FN with (a0 = a
// fixed BIOS-window constant 0x8001BEDC, a1 = mode). [POLARITY CORRECTED 2026-07-10 by the dedicated
// PutDrawEnv/streamer pass: the original draft had `< 2` — inverted. The raw gen-C
// (authenticated executable/overlay evidence guest 0x80080F6C) is `_t = (bootFlag < 2); if (_t) goto L_80080FA8;`, i.e.
// bootFlag<2 SKIPS the hook call; the call happens on fallthrough, when bootFlag>=2. ClearOTagR
// below and PutDrawEnv (wide_re_gpu_putdrawenv.cpp) have the same shape and polarity — all three
// now agree. The hook is therefore NOT a "first frames after reset" init but a "GPU sys is up"
// steady-state hook; naming updated accordingly.] Unconditionally after that: call
// GPU_SYS_TABLE[+60] (table+0x3C = DrawSync's OWN table slot per the doc) with (a0 = mode).
//
// RE-VERIFY CORRECTION (2026-07-10, wiring pass): the prior draft's "no stack frame, leaf, sp
// untouched" claim was WRONG — guest 0x80080F6C DOES push a -24 guest frame and spill s0/r16 +
// ra (authenticated executable/overlay evidence: `sp-=24; mem_w32(sp+16,r16); r16=a0; ...
// mem_w32(sp+20,ra)`), restored at both exits. Both call targets here (GPU_SYS_INIT_FN and the
// table+0x3C entry) are ARBITRARY dispatch targets that may push their own frames — if this leaf
// doesn't mirror gen's sp/ra, every downstream guest-stack write from those callees lands 24 bytes
// off from gen's, which is an immediate SBS-fatal divergence. Mirrored below (frame + both r31
// return-address literals, per CLAUDE.md "mirror the guest stack, never omit it").
void Render::drawSync() {
  Core *c = mCore;
  const uint32_t mode = c->r[4];
  c->r[29] -= 24;
  c->mem_w32(c->r[29] + 16, c->r[16]);
  c->r[16] = mode;
  c->mem_w32(c->r[29] + 20, c->r[31]);

  auto epilogue = [&]() {
    c->r[31] = c->mem_r32(c->r[29] + 20);
    c->r[16] = c->mem_r32(c->r[29] + 16);
    c->r[29] += 24;
  };

  uint8_t bootFlag = c->mem_r8(GPU_BOOT_FLAG);
  if (bootFlag >= 2) {
    c->r[4] = (32770u << 16) + (uint32_t)(int32_t)(-16676); // 0x8001BEDC — fixed BIOS-window arg
    uint32_t initFn = c->mem_r32(GPU_SYS_INIT_FN);
    c->r[5] = c->r[16];
    c->r[31] = 0x80080FA8u;
    psx::cpu::dispatchGuestToReturn0(*c, initFn, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
  }
  // BUG FIX (2026-07-10, wiring re-verify): GPU_SYS_TABLE (0x800A5998) is a POINTER FIELD holding
  // the address of the real jump table — the guest-visible behavior dereferences it TWICE
  // (authenticated executable/overlay evidence lines 16-18: `r2=mem_r32(base+22936); r2=mem_r32(r2+60)`), not once.
  // The prior draft read `mem_r32(GPU_SYS_TABLE + 60)` directly (single deref), which — before the
  // table is ever relocated/reallocated to match the raw base+22936 address — reads garbage
  // (observed: 0xFFFFFFFF / stale scratch like 0x0101000A) and dispatches into nowhere, corrupting
  // the whole downstream OT chain. GPU_SYS_INIT_FN below is NOT a pointer-to-pointer (single deref
  // is correct there — confirmed against gen, no second indirection on that field).
  uint32_t tableBase = c->mem_r32(GPU_SYS_TABLE);
  uint32_t tableSlot60 = c->mem_r32(tableBase + 60); // table+0x3C, the DrawSync entry itself
  c->r[4] = c->r[16];
  c->r[31] = 0x80080FC4u;
  psx::cpu::dispatchGuestToReturn0(*c, tableSlot60, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
  epilogue();
}

// guest 0x80081458 (0x80081458) — ClearOTagR(OT, entries). VERIFIED & WIRED 2026-07-10 (was DRAFT). RE'd from
// authenticated executable/overlay evidence guest 0x80081458 (64 gen-C ln). CONFIRMED identity via docs/engine_re.md
// ("FUN_80081458=ClearOTagR (table+0x2c)"; per-frame loop calls it as `FUN_80081458(ctx, 0x800)` = 2048 OT entries).
//
// NOTE: the guest C emission for this address contains a SECOND, unreachable-from-here prologue/
// epilogue pair after this function's `return` (a binary-boundary artifact — the shard groups adjacent
// guest code without a clean symbol boundary). That trailing block is a DIFFERENT, un-RE'd MIPS
// function reachable only via its own call sites (not via a call to 0x80081458) — NOT ported here.
//
// Guest ABI: a0=OT pointer, a1=entry count. Same boot-flag-gated hook pattern as DrawSync
// (hook gets a0=fixed const 0x8001BF68, a1=OT, a2=entryCount) — and the SAME polarity correction
// applies (2026-07-10, dedicated PutDrawEnv/streamer pass): the gen-C
// (authenticated executable/overlay evidence guest 0x80081458) is `_t = (bootFlag < 2); if (_t) goto L_800814A0;`, i.e.
// bootFlag<2 SKIPS the hook; the call happens when bootFlag>=2 (the original draft had this
// inverted). Then calls GPU_SYS_TABLE[+44] (table+0x2C, ClearOTagR's own slot) with
// (a0=OT, a1=entryCount) — presumably the real hardware-facing OT-clear loop, opaque to this leaf.
// AFTER that call, ClearOTagR additionally links *OT to a small shared "dummy tail packet"
// (classic libgpu ClearOTagR internal: every table build shares one small terminator/padding
// structure) — CONSTANTS CORRECTED 2026-07-10: the gen-C decimals are +23136/+23116 from base
// 0x800A0000, i.e. tail packet at **0x800A5A60** and tag content **0x800A5A4C** (the original
// draft's 0x800A5B20/0x800A5B0C was a decimal→hex conversion slip, off by 0xC0). Writes a tag word
// (0x04000000 | (0x800A5A4C & 0x00FFFFFF)) to 0x800A5A60, then sets *OT = (0x800A5A60 &
// 0x00FFFFFF). Transcribed as literal constant-folded values (the guest-visible behavior computes on raw
// addresses-as-integers, not memory reads through pointers, for this whole tail — no dereference).
// Frame -32, spills ra/s17/s16 at +24/+20/+16 (s16=OT ptr kept live across the hook call,
// s17=entryCount).
void Render::clearOTagR() {
  Core *c = mCore;
  c->r[29] -= 32;
  c->mem_w32(c->r[29] + 16, c->r[16]);
  c->r[16] = c->r[4]; // s16 = OT
  c->mem_w32(c->r[29] + 20, c->r[17]);
  c->r[17] = c->r[5]; // s17 = entryCount
  uint8_t bootFlag = c->mem_r8(GPU_BOOT_FLAG);
  c->mem_w32(c->r[29] + 24, c->r[31]); // ra spill happens unconditionally (branch-delay-slot write)
  if (bootFlag >= 2) {
    c->r[4] = (32770u << 16) + (uint32_t)(int32_t)(-16536); // 0x8001BF68 — fixed BIOS-window arg
    c->r[5] = c->r[16];
    uint32_t initFn = c->mem_r32(GPU_SYS_INIT_FN);
    c->r[6] = c->r[17];
    c->r[31] = 0x800814A0u;
    psx::cpu::dispatchGuestToReturn0(*c, initFn, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
  }
  // Same missing-indirection bug as DrawSync above (fixed 2026-07-10): GPU_SYS_TABLE must be
  // dereferenced once to get the table's real base, THEN +44 dereferenced again (authenticated executable/overlay
  // evidence shard_7.c:12284 lines 19-22: `r2=mem_r32(base+22936); r2=mem_r32(r2+44)`).
  uint32_t tableBase = c->mem_r32(GPU_SYS_TABLE);
  uint32_t tableSlot44 = c->mem_r32(tableBase + 44); // table+0x2C, ClearOTagR's own entry
  c->r[4] = c->r[16];
  c->r[5] = c->r[17];
  c->r[31] = 0x800814BCu;
  psx::cpu::dispatchGuestToReturn0(*c, tableSlot44, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);

  const uint32_t mask24 = (255u << 16) | 65535u;              // 0x00FFFFFF
  constexpr uint32_t kDummyTagAddr = 0x800A0000u + 23136u;    // 0x800A5A60 (gen-C decimal, CORRECTED — see header)
  constexpr uint32_t kDummyTagContent = 0x800A0000u + 23116u; // 0x800A5A4C (gen-C decimal, CORRECTED)
  uint32_t tag = (kDummyTagContent & mask24) | (1024u << 16); // 0x04000000 | low24
  c->mem_w32(kDummyTagAddr, tag);
  c->mem_w32(c->r[16], kDummyTagAddr & mask24); // *OT = low24(0x800A5A60)
  // gen publishes v0 (r2) = s16 = the OT pointer (arg r4) — its final r2 after `r2 = r16+0` right
  // before the *OT store. The prior draft left r2 stale (MIRROR_VERIFY: native=0x800
  // substrate=<OT ptr e.g. 0x800E80A8>) because it wrote *OT via c->r[16] without also publishing r2.
  // gen also publishes v1 (r3) = the dummy-tag word itself (r3 = low24(content)|0x04000000) — its
  // final r3 after `r3 = r3|r4` (MIRROR_VERIFY: substrate=0x040A5A4C). Native left it stale.
  c->r[2] = c->r[16];
  c->r[3] = tag;

  c->r[31] = c->mem_r32(c->r[29] + 24);
  c->r[17] = c->mem_r32(c->r[29] + 20);
  c->r[16] = c->mem_r32(c->r[29] + 16);
  c->r[29] += 32;
}

// ==================================================================================================
// Wiring (2026-07-10): promoted DrawSync/ClearOTagR from wide-RE draft to verified ownership per
// docs/fleet-workflow.md §9. Re-verify found TWO real bugs, both fixed above:
//   (1) DrawSync had a MISSING STACK FRAME — the draft claimed "no stack frame, leaf, sp untouched"
//       but guest 0x80080F6C actually pushes a -24 frame and spills s0/r16+ra; every downstream
//       guest-stack write from its 2 dispatch targets would have landed 24 bytes off from gen.
//   (2) BOTH DrawSync and ClearOTagR read `GPU_SYS_TABLE + offset` with only ONE dereference —
//       GPU_SYS_TABLE (0x800A5998) is actually a POINTER FIELD to the real jump table, and gen
//       dereferences it TWICE (`r2=mem_r32(base+22936); r2=mem_r32(r2+60)`). The single-deref
//       version read garbage (0xFFFFFFFF / stale scratch) and dispatched into nowhere, corrupting
//       the entire downstream OT chain (frame-0 SBS: 6109+ RAM bytes diverged, then a
//       render-queue-overflow crash within a few frames). Root-caused via PSXPORT_THUNK_FORCE_GEN
//       bisection + a temporary debug print, NOT by staring at the RE.
// Both are substrate-called leaves (plain intra-shard C calls, not typed runtime address dispatch) — wired via the
// oracle-gated tomba::native::declareOverride thunk so SBS core B keeps running the pure original guest instructions
// body. OVHIT (PSXPORT_DEBUG=ovhit, 5-frame REPL run): both FIRE with MATCHING native/oracle counts — `0x80080F6C
// native=1045 oracle=1045`, `0x80081458 native=1020 oracle=1020` — real gate coverage, not a "0-diff because never
// called" false positive. libgpuSetDrawMode (0x80083DE0, SetDrawMode) joined them 2026-07-29, after a line-by-line
// re-verify against its gen body corrected a wrong-argument defect the draft carried (see its banner below), and
// libgpuDmaStatusReset joined on the same day after a re-verify found its draft already faithful.
// vertexHeaderRepack remains an unwired, un-re-verified wide-RE draft.
namespace {
void ov_drawSync(Core *c) {
  rend(c)->drawSync();
}
void ov_clearOTagR(Core *c) {
  rend(c)->clearOTagR();
}
} // namespace

static void libgpuSetDrawMode(Core *c);    // SetDrawMode — defined below, wired here
static void libgpuDmaStatusReset(Core *c); // GPU-DMA status-block reset — ditto

void gpu_libgpu_leaves_install() {
  static bool done = false;
  if (done) {
    return;
  }
  done = true;
  tomba::native::declareOverride(0x80080F6Cu, "ov_drawSync", ov_drawSync);
  tomba::native::declareOverride(0x80081458u, "ov_clearOTagR", ov_clearOTagR);
  tomba::native::declareOverride(0x80083DE0u, "libgpuSetDrawMode", libgpuSetDrawMode);
  tomba::native::declareOverride(0x80082C68u, "libgpuDmaStatusReset", libgpuDmaStatusReset);
}

// libgpuDmaStatusReset (0x80082C68) — GPU-DMA status-block RESET. RE-VERIFIED + WIRED 2026-07-29
// (line-by-line against authenticated executable/overlay evidence guest 0x80082C68:11051-11069: all four target
// addresses 0x800A5AA8/AAC/AB0/AB4 and all four stored values matched, in store order, with only v0 missing). Unlike
// libgpuSetDrawMode below, this draft was FAITHFUL — which is why the bank has to be checked rather than assumed either
// way. RE'd from authenticated executable/overlay evidence guest 0x80082C68 (19 gen-C ln, no branches, no calls — fully
// self-contained). Not itself a GPU_SYS_TABLE entry (no table dereference); writes the same status-block fields the
// 0x80082D04 completion-queue cluster (MAPPED, not drafted — see file header) tests every call. Guest ABI: a0 = an
// opaque pointer the caller owns (stashed verbatim into GPU_DMA_ARG0_PTR's target — this leaf never reads through it,
// just stores it for the queue cluster to consume later).
//
// Writes: *GPU_DMA_FLAGS_PTR = (1024u<<16) | 2  (0x04000002); *GPU_DMA_ARG0_PTR = a0;
// *GPU_DMA_ARG1_PTR = 0; *GPU_DMA_STATE_PTR = (256u<<16) | 1025  (0x01000401).
static void libgpuDmaStatusReset(Core *c) {
  uint32_t a0 = c->r[4];
  uint32_t flagsPtr = c->mem_r32(GPU_DMA_FLAGS_PTR);
  c->mem_w32(flagsPtr, (1024u << 16) | 2u);
  uint32_t arg0Ptr = c->mem_r32(GPU_DMA_ARG0_PTR);
  c->mem_w32(arg0Ptr, a0);
  uint32_t arg1Ptr = c->mem_r32(GPU_DMA_ARG1_PTR);
  c->mem_w32(arg1Ptr, 0);
  uint32_t statePtr = c->mem_r32(GPU_DMA_STATE_PTR);
  c->mem_w32(statePtr, (256u << 16) | 1025u);
  c->r[2] = statePtr; // v0: the guest-visible behavior's last pointer load is left in r2 at return
}

// libgpuSetDrawMode (0x80083DE0) — libgpu **SetDrawMode(DR_MODE* p, int dfe, int dtd, int tpage,
// RECT* tw)**. Was a DRAFT carrying a real defect; re-verified line-by-line against
// authenticated executable/overlay evidence guest 0x80083DE0 and WIRED 2026-07-29.
//
// THE ARGUMENTS ARE NOW PINNED, and they identify the function. The draft described a1 as
// "rgbBitsSrc" whose own low bits land in the mode word — but gen line 12643 masks **r7**, not r5:
//
//     { int _t = (c->r[5] == c->r[0]); c->r[2] = c->r[7] & 2559u; if (_t) goto L_80083E08; }
//
// i.e. it BRANCHES on a1 and MASKS a3. The draft used a1 for both, and its banner asserted "a3(r7) =
// UNUSED by this leaf (register alias only, verified: the guest-visible behavior never reads r7)" — which that one
// line disproves. Wiring it as drafted would have written a texture-page field built from the wrong
// argument into every DR_TPAGE header the guest emits. This is exactly the re-verify that the
// wide-RE bank requires before a draft is wired, doing its job.
//
// Once a3 is the tpage, the whole signature is stock Sony libgpu SetDrawMode:
//   mode = 0xE1000000 | (tpage & 0x9FF) | (dtd ? 0x200 : 0) | (dfe ? 0x400 : 0)
// which is the documented DR_TPAGE encoding, with dfe = "draw to display area" and dtd = dither.
// The 5th argument arrives on the stack per the o32 outgoing-arg convention; this leaf never adjusts
// sp (abi_extract: frame_size 0), so c->r[29]+16 is the correct slot. tw == 0 means "no texture
// window" and writes a zero word rather than a DR_TWIN.
//
// v0 IS REPRODUCED even though it is junk on both paths (the mode word when tw == 0, the low bits of
// the negated X offset otherwise). It is junk that a caller could still read, and it costs one
// assignment to be exact instead of leaving whatever the previous call left in r2.
static void libgpuSetDrawMode(Core *c) {
  // 5th arg: o32 outgoing-arg stack slot
  c->r[2] = tomba2::render::setDrawMode(
      psx::present::EmitMemory(*c), c->r[4], c->r[5], c->r[6], c->r[7], c->mem_r32(c->r[29] + 16));
}

// vertexHeaderRepack (0x800847B0) — 20-byte SoA->AoS vertex-header REPACK. DRAFT. RE'd from authenticated
// executable/overlay evidence guest 0x800847B0 (18 gen-C ln, fully self-contained — no calls, no branches). LOW
// confidence on semantic name: the shape is a fixed 5-word struct copy from a0 to a1 with fields 0/1 swapped and three
// of the five words additionally overwritten in their LOW 16 bits by a value taken from a DIFFERENT source word — i.e.
// it repacks a {u32,u32,u32,u32,s16} source into a differently-ordered destination where three fields are (high16 old,
// low16 new). This is the same "pack two logical halfwords into one word" idiom used throughout the GT3/GT4 packet
// builders (game/render/ overlay_gt3gt4.cpp, overlay_ground_gt3gt4.cpp) — plausibly a shared vertex/UV-pair repacker
// for that family, but NOT confirmed against a caller this session (no direct caller found in authenticated
// executable/overlay evidence; reached only via typed runtime address dispatch, consistent with the free-roam dispatch
// count). Guest ABI: a0=src (20 B), a1=dst (20 B); no return value read by any caller pattern seen.
static void vertexHeaderRepack(Core *c) {
  uint32_t src = c->r[4];
  uint32_t dst = c->r[5];

  uint32_t w0 = c->mem_r32(src + 0);
  uint32_t w1 = c->mem_r32(src + 4);
  c->mem_w32(dst + 4, w0);
  c->mem_w32(dst + 0, w1);
  c->mem_w16(dst + 0, (uint16_t)w0);

  uint32_t w2 = c->mem_r32(src + 8);
  uint32_t w3 = c->mem_r32(src + 12);
  c->mem_w32(dst + 12, w2);
  c->mem_w32(dst + 8, w3);
  c->mem_w16(dst + 12, (uint16_t)w1);
  c->mem_w16(dst + 8, (uint16_t)w2);

  uint32_t h4 = (uint32_t)(int32_t)c->mem_r16s(src + 16);
  c->mem_w16(dst + 4, (uint16_t)w3);
  c->mem_w16(dst + 16, (uint16_t)h4);
}
