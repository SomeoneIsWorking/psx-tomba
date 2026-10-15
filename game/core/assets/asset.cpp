// game/core/asset.cpp — class Asset — PC-native ASSET LOADING subsystem owned by Engine.
// Owns the engine's loading domain (per CLAUDE.md THE BOUNDARY): the texture-group loader
// orchestration, the LZ image decompressor, the CPU->VRAM upload, and the stage-0/1 boot preload
// chain. The CD read + the task terminal-yield stay the retained platform/content mechanism (called
// via typed runtime address dispatch, not transcribed). See asset.h — instance-with-back-pointer subsystem, methods
// take typed args directly (no MIPS taxi c->r[4..7] marshal).
#include "assets/asset.h"
#include "cd/engine_cd_loader.h"
#include "core.h"
#include "entry/session.h"
#include "game.h" // c->game->hle.deliverEvent — Hle subsystem lives on Game
#include "gpu_ports.h"
#include "guest_call.h" // rc1-4 guest-call helpers (used by the preload chain below)
#include "overrides/native_override_catalog.h"
#include "sched/scheduler.h"
#include <lucent/log.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>

// PC-owned LZ image decompressor — replaces guest FUN_80044D8C (0x80044D8C). Rebuilds per-frame
// CLUTs (0x801FCDC0) and sprite/texture data from compressed area assets. It was the source of the
// gameplay 2D-sprite corruption: the SAME function gave correct output when guest but ZEROS
// when flat-interpreted by the coroutine interpreter (rec_coro_run) at runtime — a recorded binary evidence-vs-
// interpreter divergence. A pure decompressor belongs to the PC side, so we own it natively here.
//
// ABI (matches the MIPS at 0x80044D8C, verified by disassembly):
//   desc=descriptor, dst=dest, src0=src bytes, srclen=len. Returns bytes written.
//   Setup: build 8 back-ref offsets from the static table at 0x800153C8, scaled by the per-call
//   stride at desc+4:  offset[i] = base[i] + 2*(factor[i]*stride)  (2D image predictors: mode 1 =
//   previous byte, modes 2-7 = previous-row neighbours; row pitch = stride).
//   Stream of control bytes: len=ctrl>>3, mode=ctrl&7.  mode==0 -> literal copy `len` bytes from
//   src (ctrl byte 0 / len 0 terminates).  mode!=0 -> back-ref copy `len` bytes from dest+offset
//   [mode], BYTE-granular so overlapping copies replicate (RLE), exactly as the original loop.
#define LZ_OFFTAB_BASE 0x800153C8u
namespace tomba {

uint32_t Asset::lzDecompress(const LzDecompressArgs &args) {
  auto [desc, dst, src0, srclen] = args;
  Core *c = this->core;
  const uint32_t src_end = src0 + srclen;
  const int32_t stride = c->mem_r16s(desc + 4);
  // Frame discipline (faithful-execution model): the substrate body descends sp by 32 and builds
  // the 8-entry back-ref offset table IN ITS GUEST FRAME (sp+0..+28) — compared task-stack bytes
  // under strict SBS. Keep the guest-frame copy authoritative alongside the host mirror.
  c->r[29] -= 32;
  int32_t offtab[8];
  for (int i = 0; i < 8; i++) {
    const int32_t base = (int32_t)c->mem_r32(LZ_OFFTAB_BASE + i * 8 + 0);
    const int32_t factor = (int32_t)c->mem_r32(LZ_OFFTAB_BASE + i * 8 + 4);
    offtab[i] = base + 2 * (factor * stride);
    c->mem_w32(c->r[29] + (uint32_t)i * 4, (uint32_t)offtab[i]);
  }
  uint32_t src = src0, out = dst;
  while (src < src_end) {
    const uint8_t ctrl = c->mem_r8(src++);
    const uint32_t len = ctrl >> 3, mode = ctrl & 7u;
    if (mode != 0) { // back-reference into the output so far
      uint32_t bsrc = out + (uint32_t)offtab[mode];
      for (uint32_t k = 0; k < len; k++) {
        c->mem_w8(out++, c->mem_r8(bsrc++));
      }
    } else { // literal run from the source
      if (len == 0) {
        break; // terminator
      }
      for (uint32_t k = 0; k < len; k++) {
        c->mem_w8(out++, c->mem_r8(src++));
      }
    }
  }
  c->r[29] += 32;   // frame ascent
  return out - dst; // total bytes written
}

// PC-owned texture-group unpacker — replaces guest FUN_80044E84 (0x80044E84). Verified by
// disassembly: tablePtr = descriptor table base, anchorEnd = scratch-end anchor (0x1FD000). Layout:
// [count:4] then [pad:4] then `count` 12-byte entries each { stride:2(@+4 from entry head),
// field:2(@+6), srclen:4(@+8) }; source data starts 0x800 after the table base and advances by
// srclen per entry. For each entry: dst = anchorEnd - 2*stride*field (outputs stack ending at the
// anchor — transient scratch), decompress the entry's image there, then upload it (FUN_80081218)
// and run the post step (FUN_80080f6c). Non-gameplay (asset unpack) → PC-owned, calling the native
// decompressor directly; the two gfx-library sub-calls still route through the guest instruction path/dispatch.
void Asset::unpackGroup(const GroupRangeArgs &args) {
  auto [tablePtr, anchorEnd] = args;
  Core *c = this->core;
  const int32_t count = (int32_t)c->mem_r32(tablePtr);
  uint32_t entry = tablePtr + 4;   // first 12-byte descriptor entry
  uint32_t src = tablePtr + 0x800; // packed source data follows the table
  lucent::debug("unpack", "table=0x{:08X} count={} src0=0x{:08X} ra=0x{:08X}", tablePtr, count, src, c->r[31]);
  for (int32_t i = 0; i < count; i++) {
    const uint32_t desc = entry;
    const int32_t stride = c->mem_r16s(desc + 4);
    const int32_t field = c->mem_r16s(desc + 6);
    const uint32_t srclen = c->mem_r32(desc + 8);
    const uint32_t dst = anchorEnd - (uint32_t)(2 * stride * field);
    lucent::debug("unpack",
                  " e{} dst=({},{}) {}x{} src=0x{:08X} len={} srcbytes: {:02X} {:02X} {:02X} {:02X}",
                  i,
                  c->mem_r16s(desc),
                  c->mem_r16s(desc + 2),
                  stride,
                  field,
                  src,
                  srclen,
                  c->mem_r8(src),
                  c->mem_r8(src + 1),
                  c->mem_r8(src + 2),
                  c->mem_r8(src + 3));
    lzDecompress({.desc = desc, .dst = dst, .src0 = src, .srclen = srclen}); // native decompress into transient scratch
    src += srclen;
    entry += 12;
    uploadImage({.desc = desc, .src = dst}); // FUN_80081218(desc, dst): native VRAM upload
    //   (direct C call — the override table that routed 0x80081218 here is gone; top-down PC-driven)
    // Per-image post-step FUN_80080f6c(0) = libgs GPU DrawSync between uploads — meaningless for our
    // synchronous native upload (no async DMA to drain). Owned as a skip; see note above uploadImage.
  }
}

// FAITHFUL texture-group unpacker — FUN_80044E84 with full guest-stack discipline (faithful-
// execution model; used by the pc_faithful task-1 body loadTexgroup). Same algorithm as
// unpackGroup above, but: frame descent 48 with the RE'd live s-reg/ra spills at +16..+44, loop
// state maintained in the guest s-registers (nested callee prologues spill them into compared
// task-1 stack bytes), and the two libgs leaves — LoadImage FUN_80081218 and DrawSync
// FUN_80080F6C — dispatched at the live guest sp with their call-site ra constants, exactly as
// core B's substrate body runs them. Byte shape: guest instructions at 0x80044E84 (shard_5.c).
void Asset::unpackGroupFaithful(const GroupRangeArgs &args) {
  auto [tablePtr, anchorEnd] = args;
  Core *c = this->core;
  c->r[29] -= 48;
  const uint32_t sp = c->r[29];
  c->mem_w32(sp + 24, c->r[18]);
  c->r[18] = tablePtr;
  c->mem_w32(sp + 40, c->r[22]);
  c->r[22] = anchorEnd;
  c->mem_w32(sp + 36, c->r[21]);
  c->r[21] = tablePtr + 2048; // packed source follows the table
  c->mem_w32(sp + 44, c->r[31]);
  c->mem_w32(sp + 32, c->r[20]);
  c->mem_w32(sp + 28, c->r[19]);
  c->mem_w32(sp + 20, c->r[17]);
  c->mem_w32(sp + 16, c->r[16]);
  const int32_t count = (int32_t)c->mem_r32(c->r[18]);
  c->r[18] += 4;                    // first 12-byte descriptor entry
  c->r[20] = (uint32_t)(count - 1); // loop counter in s4 (branch-delay decrement)
  if (count > 0) {
    c->r[19] = c->r[18] - 6;
    for (;;) {
      c->r[19] += 12;
      const int32_t stride = c->mem_r16s(c->r[19] - 2); // desc+4
      const int32_t field = c->mem_r16s(c->r[19] + 0);  // desc+6
      c->r[17] = c->r[18];                              // desc (s1)
      c->r[18] += 8;
      const uint32_t srclen = c->mem_r32(c->r[18]); // desc+8
      c->r[18] += 4;                                // next descriptor
      c->r[6] = c->r[21];                           // src
      c->r[21] += srclen;
      c->r[16] = c->r[22] - (uint32_t)(2 * stride * field); // dst = transient scratch (s0)
      c->r[4] = c->r[17];
      c->r[5] = c->r[16];
      c->r[7] = srclen;
      c->r[31] = 0x80044F10u;
      lzDecompress(
          {.desc = c->r[17], .dst = c->r[16], .src0 = c->r[6], .srclen = srclen}); // FUN_80044D8C (guest-frame offtab)
      c->r[4] = c->r[17];
      c->r[5] = c->r[16];
      c->r[31] = 0x80044F1Cu;
      psx::cpu::dispatchGuestToReturn0(
          *c, 0x80081218u, psx::cpu::ExecutionBudget::currentTurn(*c), __func__); // libgs LoadImage at the live sp
      c->r[4] = 0;
      c->r[31] = 0x80044F24u;
      psx::cpu::dispatchGuestToReturn0(
          *c, 0x80080F6Cu, psx::cpu::ExecutionBudget::currentTurn(*c), __func__); // libgs DrawSync(0)
      const int32_t rem = (int32_t)c->r[20];
      c->r[20] = (uint32_t)(rem - 1);
      if (rem <= 0) {
        break;
      }
    }
  }
  c->r[31] = c->mem_r32(sp + 44);
  c->r[22] = c->mem_r32(sp + 40);
  c->r[21] = c->mem_r32(sp + 36);
  c->r[20] = c->mem_r32(sp + 32);
  c->r[19] = c->mem_r32(sp + 28);
  c->r[18] = c->mem_r32(sp + 24);
  c->r[17] = c->mem_r32(sp + 20);
  c->r[16] = c->mem_r32(sp + 16);
  c->r[29] += 48;
}

// PC-native TEXTURE-GROUP LOADER — owns the asset-load ORCHESTRATION FUN_80044F58 (0x80044F58): the
// per-group loader a level uses to stream a texture set into VRAM. RE (tools/disas.py + guest 0x80044F58):
// the current task selects a set via task[0x6D]=mode / task[0x6E]=set, then
//   1. CD-load a 2KB HEADER from sector (filebase0 = *0x800BE0F0) + set  [+ a 4/26 bias in mode 2] -> 0x800EF478
//   2. CD-load the compressed ARCHIVE from sector (filebase1 = *0x800BE0F8) + (hdr[0]>>11), len hdr[1]-hdr[0]
//      -> the fixed staging buffer 0x8018A000 (descriptor table in its first 0x800 bytes, packed data after)
//   3. UNPACK the archive (unpackGroup) -> decompress + upload each image to its VRAM (x,y) (owned)
//   4. copy a 42-word metadata table from hdr+0x100 (0x800EF578) -> 0x800FB170 (per-set sprite/CLUT meta the
//      game content reads back)
//   5. (mode==0 only) set _DAT_1f80019b=1 and run the task TERMINAL YIELD FUN_80051FB4 (suspend until next
//      frame — this is what streams the groups one-per-frame).
// ENGINE asset orchestration -> reimplemented PC-native; the CD read (via 0x8001DC40) and the terminal task
// yield (the scheduler) stay the retained platform/content mechanism (called, not transcribed). The
// mode/set inputs and the 0x800FB170 metadata are CONTENT-interface state, so this is gated on the main-
// RAM A/B diff (later-177). For mode==0 the terminal yield does not return (switch longjmps mid-game),
// exactly like eng_stage_transition's tail; there is no code after it.
// FAITHFUL guest-stack discipline (faithful-execution model): frame descent 24 with ra/s0 spills
// at +20/+16 (live values), header/archive CD reads dispatched with their call-site ra constants,
// unpackGroupFaithful for step 3 (its libgs leaves run at the live guest sp), and the metadata-
// copy cursor kept in the guest s0 (selfClose's prologue spills it). Byte shape: generated
// guest 0x80044F58. This method only runs on the pc_faithful task-1 path (runTask1PreloadStanza);
// the native_sync shortcut is preloadTexgroup below.
void Asset::loadTexgroup() {
  Core *c = this->core;
  c->r[29] -= 24;
  const uint32_t sp = c->r[29];
  c->mem_w32(sp + 20, c->r[31]);
  c->mem_w32(sp + 16, c->r[16]);
  const uint32_t task = c->mem_r32(0x1F800138u);
  const uint32_t mode = c->mem_r8(task + 0x6Du);
  const uint32_t set = c->mem_r8(task + 0x6Eu);
  uint32_t hdr_sector = c->mem_r32(0x800BE0F0u) + set; // filebase0 + set
  if (mode == 2) {                                     // mode-2 per-set 4/26-sector bias
    uint16_t mask = c->mem_r16(0x800BFE56u);
    hdr_sector += ((mask >> (set & 31)) & 1) ? 26u : 4u;
  }
  const uint32_t HDR = 0x800EF478u;
  c->r[4] = HDR;
  c->r[5] = hdr_sector;
  c->r[6] = 2048;
  c->r[31] = 0x80044FD8u;
  psx::cpu::dispatchGuestToReturn0(
      *c, 0x8001DC40u, psx::cpu::ExecutionBudget::currentTurn(*c), __func__); // 1. CD-load 2KB header (platform)
  c->r[16] = HDR;                                                             // s0 = header base (live for spills)
  uint32_t h0 = c->mem_r32(HDR + 0), h1 = c->mem_r32(HDR + 4);
  c->r[4] = 0x8018A000u;
  sessionOf(c).nativeOverrides.retireOverlay(*c, eng(c).activeAreaOverlay);
  c->r[5] = c->mem_r32(0x800BE0F8u) + (h0 >> 11);
  c->r[6] = h1 - h0;
  c->r[31] = 0x80045008u;
  psx::cpu::dispatchGuestToReturn0(*c,
                                   0x8001DC40u,
                                   psx::cpu::ExecutionBudget::currentTurn(*c),
                                   __func__); // 2. CD-load compressed archive -> staging
  c->r[4] = 0x8018A000u;
  c->r[5] = 0x001FD000u;
  c->r[31] = 0x8004501Cu;
  unpackGroupFaithful({.tablePtr = 0x8018A000u, .anchorEnd = 0x1FD000u}); // 3. unpack -> decompress + VRAM upload
  for (uint32_t i = 0; i < 42; i++) {                                     // 4. per-set metadata table (cursor in s0)
    c->mem_w32(0x800FB170u + i * 4, c->mem_r32(c->r[16] + 256u));
    c->r[16] += 4;
  }
  if (c->mem_r8(c->mem_r32(0x1F800138u) + 0x6Du) == 0) { // 5. terminal task end (mode 0)
    c->mem_w8(0x1F80019Bu, 1);
    c->r[31] = 0x80045070u;
    tomba::sched::taskScheduler(*c).selfClose(); // FUN_80051FB4 — does not return on a task
  }
  c->r[31] = c->mem_r32(c->r[29] + 20);
  c->r[16] = c->mem_r32(c->r[29] + 16);
  c->r[29] += 24;
}

// Per-image post-step FUN_80080f6c(0) = the game's libgs frame DrawSync: FUN_80083364(0) waits for the
// GPU's ordering-table DMA to drain and the GPU to go idle (polls GPUSTAT @0x800a5ab4 bit 0x01000000 /
// @0x800a5aa8 bit 0x04000000), having queued the (now-empty, since uploadImage bypasses it) ring.
// IMPORTANT: this same function is the MAIN-LOOP per-frame DrawSync, so it must NOT be globally overridden
// (a global no-op stalls all presentation). Inside the unpack loop, however, it is a between-uploads GPU
// sync that is meaningless for our SYNCHRONOUS native VRAM upload — there is no async DMA to wait on — so
// the unpack loop owns it as a skip. A/B-verified VRAM+RAM-identical across a full menu+seaside load
// (later-177): the unpack call site only ever passes a0=0 with an empty ring. DIAG: PSXPORT_DEBUG=unpacksync
// restores the per-image super-call to prove equivalence.

// DO NOT REGISTER 0x80081218 IN THE OVERRIDE REGISTRY. It surfaces near the top of every
// `PSXPORT_DEBUG=recdep` sweep (7,558 substrate dispatches per 6000 replay frames, 2026-07-29) and
// looks exactly like the free win that Mtx::identity was — a LIVE native whose address nobody
// installed. It is not. The body below is a REPLACEMENT, not a mirror: it uploads to the device directly
// and deliberately skips the guest's GsSortObject ring enqueue that the guest body performs.
// Wiring it would make core A omit guest writes core B still makes, i.e. an SBS divergence by
// construction. The dispatches are the faithful path doing its job, not a porting gap — leave them.
//
// FUN_80081218(desc {x, y, w, h: s16}, src): w*h 16-bit pixels, row-major. The guest queues a GP0 A0 in the
// GsSortObject ring at 0x800A5AC8; the upload goes to the device at once and the ring stays empty.
void Asset::uploadImage(const UploadImageArgs &args) {
  auto [desc, src] = args;
  Core *c = this->core;
  const int x = c->mem_r16s(desc + 0), y = c->mem_r16s(desc + 2);
  const int w = c->mem_r16s(desc + 4), h = c->mem_r16s(desc + 6);
  if (w <= 0 || h <= 0) {
    return;
  }
  std::vector<std::uint16_t> pixels(static_cast<std::size_t>(w) * static_cast<std::size_t>(h));
  for (std::size_t i = 0; i < pixels.size(); i++) {
    pixels[i] = c->mem_r16(src + static_cast<std::uint32_t>(i * 2u));
  }
  psx::gpu::loadImage(*c, {x, y, w, h}, pixels);
}

// ===== Stage-0/stage-1 area/asset PRELOAD — PC-native + SYNCHRONOUS (moved from native_boot.cpp,
// 2026-07 restructure). Reuses the existing leaf natives above (EngineCdLoader::loadFile for the sync CD
// reads, unpackGroup for decompress+VRAM upload) — same content-loading domain as the texgroup loader.

// FUN_80044F58 texture-group load, synchronous. (Mirrors loadTexgroup but driven by explicit
// (mode,set) — no task-1 spawn, no terminal yield.) Header sector -> archive -> unpack -> copy the
// 42-word per-set metadata table the still-guest content reads back.
void Asset::preloadTexgroup(const PreloadTexgroupArgs &args) {
  auto [mode, set] = args;
  Core *c = this->core;
  uint32_t hdr_sector = c->mem_r32(0x800BE0F0u) + set; // filebase0 + set
  if (mode == 2) {                                     // mode-2 per-set 4/26-sector bias
    uint16_t mask = (uint16_t)c->mem_r16(0x800BFE56u);
    hdr_sector += ((mask >> (set & 31)) & 1) ? 26u : 4u;
  }
  tomba::EngineCdLoader(*c).loadFile(0x800EF478u, hdr_sector, 2048); // 1. 2KB header
  uint32_t h0 = c->mem_r32(0x800EF478u), h1 = c->mem_r32(0x800EF47Cu);
  sessionOf(c).nativeOverrides.retireOverlay(*c, eng(c).activeAreaOverlay);
  tomba::EngineCdLoader(*c).loadFile(
      0x8018A000u, c->mem_r32(0x800BE0F8u) + (h0 >> 11), h1 - h0); // 2. compressed archive
  unpackGroup({.tablePtr = 0x8018A000u, .anchorEnd = 0x1FD000u});  // 3. decompress + VRAM upload (native)
  for (uint32_t i = 0; i < 42; i++) {                              // 4. per-set metadata table
    c->mem_w32(0x800FB170u + i * 4, c->mem_r32(0x800EF478u + 0x100u + i * 4));
  }
}

struct PreloadCelArgs {
  uint32_t out;
  uint32_t desc;
  uint32_t cbarg;
};
// FUN_800753D4 cel-load, SYNCHRONOUS. Original: FUN_80096480 (slot alloc + BAV cel load) -> store slot
// at `out` -> FUN_80096980 (kick the upload state machine) -> cross-frame poll FUN_80096a40 until the
// GPU-DMA upload completes. The alloc + kick carry no async wait (they leave the slot in state 1), so
// run them as the guest instruction path REFERENCE; our native GPU upload is synchronous, so we DROP the cross-frame
// poll (the "no async" directive) instead of yielding for a DMA that already happened.
static void preload_cel(Core *c, const PreloadCelArgs &args) {
  auto [out, desc, cbarg] = args;
  psx::cpu::callGuestNow(*c, __func__, 0x80096480u, desc, (uint32_t)-1, cbarg);
  int16_t slot = (int16_t)c->r[2];                                          // FUN_80096480(desc,-1,cbarg)
  c->mem_w16(out, (uint16_t)slot);                                          // *(u16*)out = allocated slot
  psx::cpu::callGuestNow(*c, __func__, 0x80096980u, cbarg, (uint32_t)slot); // FUN_80096980(cbarg, slot): kick upload
  // FUN_800753D4 writes `out` a SECOND time with FUN_80096980's RETURN (the real slot handle; -1 when
  // the SPU-DMA kick found no free channel). Dropping this write-back left a stale positive slot id in
  // the cel handle (0x800BED84/0x800BED82) on the failure branch — the sprite/cel registrations then
  // reference the wrong SPU sample bank: bug #29 "right instrument, wrong sample" on the skip path.
  // RE: scratch/decomp/vab_open.c (FUN_800753d4) + vab_kick.c (FUN_80096980). 2026-07-08.
  c->mem_w16(out, (uint16_t)c->r[2]);
  // Drain the BAV upload queue SYNCHRONOUSLY so this cel's VAB bank reaches SPU (and its slot frees so the
  // NEXT cel can allocate). The real FUN_800753d4 polls 0x80096a40, whose 0x800993a0 sync busy-waits on the
  // upload's DMA-complete IRQ event — which never fires in this no-IRQ preload, so the original code dropped
  // the poll and silently skipped a whole VAB bank (proved by the dual-core SPU-DMA diff: 1 transfer vs PSX's
  // 2). Deliver the sound-DMA-complete event first so 0x800993a0 returns immediately (no busy-wait, no yield),
  // then run the sync once. (later: in-game music VAB.)
  c->game->hle.deliverEvent(0xF0000009u, 0xFFFFFFFFu);  // sound/DMA-complete event
  psx::cpu::callGuestNow(*c, __func__, 0x80096a40u, 0); // FUN_80096a40(0): upload sync (now non-blocking)
}

// FUN_800754F4 cel/sprite VRAM build, synchronous. FUN_800753ac is itself an async CD read -> use the
// sync loadfile; the two FUN_800753d4 cel-loads go through preload_cel; the ten FUN_80075448
// sprite-cell registrations carry no CD/async wait, so run as guest instruction path. `base` = work base 0x80182000.
static uint32_t preload_build_vram(Core *c, uint32_t base) {
  uint32_t s0 = base + 0x51000u;                                               // descriptor table (filled by the read)
  tomba::EngineCdLoader(*c).loadFile(base, c->mem_r32(0x800BE108u), 0x51800u); // FUN_800753ac: read SND file (idx3)
  preload_cel(c, {.out = 0x800BED84u, .desc = base + c->mem_r32(s0 + 0x28), .cbarg = base + c->mem_r32(s0 + 0x30)});
  preload_cel(c, {.out = 0x800BED82u, .desc = base + c->mem_r32(s0 + 0x2c), .cbarg = base + c->mem_r32(s0 + 0x34)});
  constexpr struct {
    uint32_t off, sz;
  } cells[10] = {
      {0x0c, 14},
      {0x08, 14},
      {0x04, 14},
      {0x00, 14},
      {0x10, 8},
      {0x14, 8},
      {0x18, 8},
      {0x1c, 14},
      {0x20, 14},
      {0x24, 14},
  };
  uint32_t cell_h = (uint32_t)c->mem_r16s(0x800BED82u);
  for (int i = 0; i < 10; i++) {
    psx::cpu::callGuestNow(
        *c, __func__, 0x80075448u, (uint32_t)i, base + c->mem_r32(s0 + cells[i].off), cells[i].sz, cell_h);
  }
  return base + 26356; // v0 = base + 0x66f4
}

// FUN_8004514C — the stage-1 callback body. SWDATA + DAT load, shared texgroup sub-load,
// relocation table, cel/sprite VRAM build. Direct entry: no done_flag / task-end.
void Asset::preloadStage1() {
  Core *c = this->core;
  tomba::EngineCdLoader(*c).loadFile(0x80157000u, c->mem_r32(0x800BE110u), c->mem_r32(0x800BE114u)); // SWDATA.BIN
  preloadTexgroup({.mode = 1, .set = 1}); // shared texgroup sub-load
  uint32_t lo = c->mem_r32(0x800EF480u), hi = c->mem_r32(0x800EF484u);
  tomba::EngineCdLoader(*c).loadFile(0x80158000u, c->mem_r32(0x800BE100u) + (lo >> 11), hi - lo); // DAT payload
  uint32_t dat_end = (hi - lo) + 0x80158000u;
  c->mem_w32(0x1F800228u, dat_end);
  c->mem_w32(0x800ED014u, dat_end);
  int32_t n = (int32_t)c->mem_r32(0x800EF488u); // relocation table (blez -> skip)
  for (int32_t i = 0; i < n; i++) {
    uint32_t word = c->mem_r32(0x800EF48Cu + i * 4);
    c->mem_w32(0x800ECF58u + (word >> 24) * 4, (word & 0x00FFFFFFu) + 0x80158000u);
  }
  uint32_t v0 = preload_build_vram(c, 0x80182000u);
  c->mem_w32(0x1F80022Cu, v0);
}

// loadDescriptorChunk(descIdx, slot): FAITHFUL FUN_80045258 — a leaf indexed-chunk CD reader.
//   FUN_8001DC40((&DAT_800ECF58)[slot], DAT_800BE100 + ((&DAT_800FB170)[descIdx] >> 11),
//                (&DAT_800FB170)[descIdx+1] - (&DAT_800FB170)[descIdx]).
// Byte shape: ram_f1000_all.c:24254 (Ghidra, GAME project). Both tables are word-indexed
// (4 bytes/entry): 0x800FB170 = per-chunk byte-offset descriptor table (sector = offset>>11,
// size = next-offset - offset); 0x800ECF58 = the module/dest-pointer table this leaf reads FROM
// (already latched — e.g. slot 47/0x2f is the collision-grid buffer, slot 8 the area-8 extra
// texgroup buffer — NOT the table FUN_800452C0's reloc loop writes moments later on the
// general path).
void Asset::loadDescriptorChunk(const LoadDescriptorChunkArgs &args) { // FUN_80045258
  auto [descIdx, slot] = args;
  Core *c = this->core;
  uint32_t dest = c->mem_r32(0x800ECF58u + slot * 4u);
  uint32_t off0 = c->mem_r32(0x800FB170u + descIdx * 4u);
  uint32_t off1 = c->mem_r32(0x800FB170u + (descIdx + 1u) * 4u);
  tomba::EngineCdLoader(*c).dc40Sync(
      {.dest = dest, .lba = c->mem_r32(0x800BE100u) + (off0 >> 11), .size = off1 - off0});
}

// FUN_80045558(idx): load indexed file idx (0 = OPN, 1 = CRD) into the shared AREA slot. The one
// native owner also publishes the loaded code image, which the guest page code that calls this
// leaf (the pause-menu card page, then the CRD slot browser at 0x8018FA88) cannot do itself.
static void ov_loadAreaSlotFile(Core *c) {
  c->r[2] = sessionOf(c).nativeOverrides.loadAreaSlotFile(*c, eng(c).activeAreaOverlay, c->r[4] & 0xFFu);
}

void Asset::registerOverrides(native::NativeOverrideCatalog &natives) {
  natives.declare(0x80045558u, "Asset::loadAreaSlotFile", ov_loadAreaSlotFile);
}

} // namespace tomba
