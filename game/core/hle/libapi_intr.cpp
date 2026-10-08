// game/core/libapi_intr.cpp — see libapi_intr.h for scope and for why this is not PlatformHle.
//
// RE: Ghidra headless on the existing project (scratch/decomp/gpu_dma_85c9c.c, FUN_80085c9c together
// with its caller FUN_80082d04) cross-checked against authenticated executable/overlay evidence guest 0x80085C9C, which
// is four instructions:
//
//     r3 = mem_r32(0x800ABDA8)          // libapi's hardware-pointer table entry 0 = I_MASK
//     r2 = mem_r16(r3 + 0)              // previous mask, ZERO-extended (lhu) -> v0
//     mem_w16(r3 + 0, (u16)a0)          // store the new mask
//
// The identification rests on the CALLER, not on the shape: Ghidra shows FUN_80082d04 bracketing its
// DMA-ring manipulation with `saved = f(0); ... ; f(saved)`, the save/disable/restore idiom, and a
// live REPL read confirms the table entry is 0x1F801074. A four-instruction load-store swap could
// otherwise have been almost anything.
//
// Note the 16-bit read is UNSIGNED (lhu) while the pointer load is a full word — mem_r16 and
// mem_r32 respectively. Using mem_r16s here would sign-extend a mask with bit 15 set into a
// 0xFFFFxxxx return value.
//
// ---------------------------------------------------------------------------------------------
// FUN_80086288 = runVblankCallbacks() — libapi's VBLANK INTERRUPT HANDLER (added 2026-07-30).
//
// WHAT IT DOES, in game terms: once per vertical blank it bumps the libetc VSync tick counter the
// whole game paces off (0x800ABDE0 — the very counter libetc's VSync(mode) returns, and the one
// TombaFrameDriver mirrors after the host field tick) and then runs every VSyncCallback the game has installed, by
// walking libapi's 8-slot function-pointer table at 0x800ABDC0 and calling each non-null entry.
// It is the "chain" half of the VSyncCallback API: register a callback -> it lands in a table slot
// -> this handler is what actually calls it, every frame.
//
// HOW IT WAS IDENTIFIED — from the CALL SITES, not the shape (a counter bump plus a table walk
// could be almost anything):
//   1. Ghidra headless (scratch/decomp/vsync_cb_chain_86288.c, FUN_80086230 / FUN_80086288 /
//      FUN_800909c0 decompiled together) shows FUN_80086230 — the initVblankCallbacks() already
//      owned in this file — ending with `func_0x80085b50(0, 0x80086288)`. 0x80085B50 is libapi's
//      InterruptCallback(irq, handler); irq 0 is VBLANK. So this address is REGISTERED AS THE
//      VBLANK IRQ HANDLER by its own immediate neighbour, right after that same neighbour zeroes
//      the very table (0x800ABDC0, 8 words via clearWords) and the very counter (0x800ABDE0) this
//      body walks and increments. Producer and consumer are two functions apart.
//   2. Ghidra on FUN_80086288 itself: `iRam800abde0 = iRam800abde0 + 1;` then a do-while over
//      `puVar1 = (undefined4*)0x800abdc0`, `if (*puVar1 != 0) (*(code*)*puVar1)();`, 8 iterations.
//      That is the VSyncCallback chain, unambiguously.
//   3. It is REACHED, 12k+ times a run, and we know exactly from where: libsnd's SsSetTickMode
//      parks it in the "user callback" slot DAT_0x800AC430 (live REPL read, docs/journal.md
//      2026-06-15 "later 54"), and Sequencer::frameTick() (0x800909C0, game/audio/sequencer.cpp)
//      typed runtime address dispatch()es that slot once per native frame. Ghidra on FUN_800909c0 confirms the
//      trampoline: `if (pcRam800ac430) (*pcRam800ac430)(); (*pcRam800ac42c)();`.
//
// WHY THIS IS AN ORDINARY PORT AND NOT PlatformHle (the gate in the header banner above): the
// PlatformHle bar is a body that BUSY-SPINS on a hardware IRQ this no-IRQ runtime never raises and
// therefore cannot be reproduced, only replaced. This body does the opposite — it IS the handler,
// not a waiter. It has no spin, no completion-flag poll, no MMIO read, no call to the trapped
// libetc VSync 0x80085900 (its only call is the indirect dispatch through a table slot), and its
// loop is a bounded 8 iterations. The decisive evidence is empirical: the guest body already
// executes ~12k times per run through Sequencer::frameTick without ever reaching a trap, so the
// native body runs in exactly the same place with exactly the same reachability. Nothing here
// needs replacing; it needs porting.
//
// TRAP, worth stating: the counter at 0x800ABDE0 has TWO writers in this port. TombaFrameDriver
// STORES the host count there once per native frame, while this handler READ-MODIFY-WRITES it
// (+1). That interleaving is pre-existing substrate behaviour and the port reproduces it exactly —
// do not "fix" it here; a change would be a guest-state divergence, not a cleanup.
//
// LIVE-REGISTER LAW: the walk state lives in the GUEST registers, not in C++ locals. r16 is the
// table cursor and r17 the slot index, and both stay live across the callback dispatch — a
// callback that spills its caller's callee-saved registers must spill the cursor/index the
// substrate would have spilled, not whatever the previous native code parked there. Hence
// GuestReg<16>/<17> rather than a `for (int i...)`.
#include "hle/libapi_intr.h"
#include "core.h"
#include "game.h"
#include "guest_abi.h"
#include "overrides/guest_jal.h" // GuestFrame / GuestReg / guest_dispatch — the ABI vocabulary
#include "overrides/native_override_catalog.h"
// guest call boundaries so each keeps its own guest frame

namespace {
// libapi's hardware-register pointer table. Entry 0 is I_MASK (0x1F801074); entry 1 is DPCR
// (0x1F8010F0). Only entry 0 is reached from here.
constexpr uint32_t kLibapiHwPtrTable = 0x800ABDA8u;

// The `lui at,0x800b` base every libapi VBlank datum below is built from (each offset the gen
// bodies add is NEGATIVE, so the base is one 64K page above the data itself).
constexpr uint32_t kLibapiDataBase = 0x800B0000u;

// libapi's VSyncCallback table: 8 function-pointer slots, zeroed by initVblankCallbacks() (which
// calls clearWords(table, 8)) and walked by runVblankCallbacks().
constexpr uint32_t kVsyncCallbackTable = kLibapiDataBase - 16960u; // 0x800ABDC0
constexpr uint32_t kVsyncCallbackSlots = 8;

// The libetc VSync tick counter: zeroed by initVblankCallbacks(), +1 per VBlank by
// runVblankCallbacks(). This is DAT_800abde0 — the value libetc's VSync(-1) query returns.
constexpr uint32_t kVblankTickCount = kLibapiDataBase - 16928u; // 0x800ABDE0

// A pointer slot immediately above the counter, in the same family as kLibapiHwPtrTable: a live
// headless read after 400 frames shows [0x800ABDE4] = 0x1F801114, the root-counter 1 (Timer 1)
// MODE register. initVblankCallbacks() writes 0x100 through it — bit 8 is Timer 1's clock-source
// select, i.e. count HBLANKs, which is the counter libetc's VSync(1) "hblank delta" query reads.
constexpr uint32_t kTimer1ModePtrSlot = kLibapiDataBase - 16924u; // 0x800ABDE4 -> 0x1F801114
constexpr uint32_t kTimer1ModeHblankSource = 0x100;

// Guest code addresses this file names.
constexpr uint32_t kVblankHandler = 0x80086288u;      // runVblankCallbacks — the handler installed...
constexpr uint32_t kIrqVblank = 0;                    // ...for IRQ 0 (VBLANK)...
constexpr uint32_t kInitVblankRetValue = 0x800862F4u; // ...and initVblankCallbacks' own v0.

// jal-site return addresses (the r31 constants the gen bodies load before each call).
constexpr uint32_t kRaClearWords = 0x80086260u;
constexpr uint32_t kRaInterruptCallback = 0x80086270u;
constexpr uint32_t kRaCallbackSlot = 0x800862D0u;
} // namespace

void tomba::LibapiIntr::mirrorHostVblank(Core &core, std::uint32_t hostCount) {
  core.mem_w32(kVblankTickCount, hostCount);
}

void tomba::LibapiIntr::setIntrMask(Core *c) {
  uint32_t imaskPtr = c->mem_r32(kLibapiHwPtrTable);
  uint32_t previous = c->mem_r16(imaskPtr); // lhu — zero-extended, see the banner
  c->mem_w16(imaskPtr, (uint16_t)c->r[4]);
  c->r[2] = previous;
}

// FUN_0x80086230 — VBlank-callback subsystem init: clear the 8-slot VSyncCallback table and its
// tick counter, then install runVblankCallbacks() below as the IRQ-0 (VBLANK) handler.
// ORACLE: guest 0x80086230
void tomba::LibapiIntr::initVblankCallbacks(Core *c) {
  GuestFrame<24, 1> frame(c, kInitVblankSpills); // -24, abi_extract-verified

  c->r[4] = kVsyncCallbackTable; // a0 for the clearWords() call below — set early, kept live
  c->mem_w32(c->mem_r32(kTimer1ModePtrSlot), kTimer1ModeHblankSource);
  c->r[1] = kLibapiDataBase; // $at, see kLibapiDataBase
  c->mem_w32(kVblankTickCount, 0);

  c->r[5] = kVsyncCallbackSlots;
  tomba::guest::dispatchJalToReturn(*c, 0x80086320u, kRaClearWords); // clearWords(table, 8)

  c->r[5] = kVblankHandler;
  c->r[4] = kIrqVblank;
  tomba::guest::dispatchJalToReturn(*c, 0x80085B50u, kRaInterruptCallback); // InterruptCallback(IRQ_VBLANK, handler)

  c->r[2] = kInitVblankRetValue;
}

// FUN_0x80086288 — the VBlank handler itself: bump the tick counter, then call every VSyncCallback
// registered in the 8-slot table. See this file's second banner for the identification, the
// PlatformHle determination and the LIVE-REGISTER note.
// ORACLE: guest 0x80086288
void tomba::LibapiIntr::runVblankCallbacks(Core *c) {
  // Read before the frame descends sp, exactly as the guest-visible behavior does (lw into v0, then addiu sp).
  uint32_t ticks = c->mem_r32(kVblankTickCount);

  GuestFrame<32, 3> frame(c, kRunVblankSpills); // -32, abi_extract-verified (program order preserved)

  GuestReg<17> slotIndex(c); // s1 — 0..7, live across every callback dispatch
  GuestReg<16> slotAddr(c);  // s0 — &table[slotIndex], live across every callback dispatch
  GuestReg<2> v0(c);         // v0 — the loaded handler, then the loop condition (0 on return)

  slotIndex = 0;
  slotAddr = kVsyncCallbackTable;
  c->r[1] = kLibapiDataBase; // $at, see kLibapiDataBase
  c->mem_w32(kVblankTickCount, ticks + 1u);

  // do-while: all 8 slots are visited unconditionally; the null test is per-slot, inside the loop.
  do {
    v0 = c->mem_r32(slotAddr);
    if (v0 != 0u) {
      tomba::guest::dispatchJalToReturn(*c, v0, kRaCallbackSlot);
    }
    slotIndex += 1u;
    v0 = (uint32_t)((int32_t)(uint32_t)slotIndex < (int32_t)kVsyncCallbackSlots);
    slotAddr += 4u; // branch-delay slot — advanced on every iteration, taken or not
  } while (v0 != 0u);
}

// FUN_0x80086320 — the word-fill helper: writes N words of a constant.
// ORACLE: guest 0x80086320
void tomba::LibapiIntr::clearWords(Core *c) {
  {
    int _t = (c->r[5] == c->r[0]);
    c->r[2] = c->r[5] + (uint32_t)-1;
    if (_t) {
      goto L_8008633C;
    }
  }
  c->r[3] = c->r[0] + (uint32_t)-1;
L_8008632C:;
  c->mem_w32((c->r[4] + (uint32_t)0), c->r[0]);
  c->r[2] = c->r[2] + (uint32_t)-1;
  {
    int _t = (c->r[2] != c->r[3]);
    c->r[4] = c->r[4] + (uint32_t)4;
    if (_t) {
      goto L_8008632C;
    }
  }
L_8008633C:;
  return;
}

void tomba::LibapiIntr::registerOverrides(Game *) {
  tomba::native::declareOverride(0x80085C9Cu, "tomba::LibapiIntr::setIntrMask", &tomba::LibapiIntr::setIntrMask);
  tomba::native::declareOverride(0x80086320u, "tomba::LibapiIntr::clearWords", &tomba::LibapiIntr::clearWords);
  tomba::native::declareOverride(
      0x80086230u, "tomba::LibapiIntr::initVblankCallbacks", &tomba::LibapiIntr::initVblankCallbacks);
  // Named install so PSXPORT_DEBUG=ovhit reports this one in game terms rather than as a bare
  // address — it is the hottest thing in this file (~2 hits per frame, forever).
  tomba::native::declareOverride(
      kVblankHandler, "tomba::LibapiIntr::runVblankCallbacks", &tomba::LibapiIntr::runVblankCallbacks);
}
