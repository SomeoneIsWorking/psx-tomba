// game/core/libapi_intr.h — the kernel INTERRUPT-MASK primitives (SCEI libapi).
//
// Scope is deliberately narrow: the leaves that read or write the PSX interrupt-control registers
// through libapi's hardware-pointer table at 0x800ABDA8. That table is not a guess — a live read at
// f300 shows [0x800ABDA8] = 0x1F801074 (I_MASK) and [0x800ABDAC] = 0x1F8010F0 (DPCR), i.e. libapi's
// own array of hardware register addresses.
//
// This is NOT PlatformHle material. PlatformHle exists for primitives whose real bodies BUSY-SPIN on
// a hardware IRQ our no-IRQ runtime never raises, and which therefore must be replaced rather than
// reproduced. SetIntrMask does not spin: it is a plain load-store through that table, so the native
// body is a faithful mirror and goes through the ordinary override registry like any other leaf,
// oracle-gated so core B keeps running the guest body.
#pragma once
#include "guest_abi.h"

#include <cstdint>
struct Core;
class Game;

namespace tomba {

class LibapiIntr {
public:
  // Preserve Tomba! 2's measured libetc guest counter mirror at its native frame boundary.
  // The shared Timing owner advances the host count but does not know this title's RAM address.
  static void mirrorHostVblank(Core &core, std::uint32_t hostCount);

  // FUN_80085C9C — SetIntrMask(u16 mask) -> u16 previousMask. Reads the current 16-bit value at
  // *0x800ABDA8 (= I_MASK), stores the new one, returns the old, zero-extended.
  //
  // Callers use it as the standard save/disable/restore bracket around a critical section, which is
  // what identifies it. From the GPU-DMA enqueue path (Ghidra, FUN_80082D04):
  //     saved = SetIntrMask(0);   ... touch the DMA ring ...   SetIntrMask(saved);
  static void setIntrMask(Core *c);

  // FUN_0x80086230 — zeroes the 8-slot VSync callback table through the word-fill helper below,
  // then registers runVblankCallbacks() as the IRQ-0 (VBlank) handler.
  static void initVblankCallbacks(Core *c);
  // FUN_0x80086320 — the word-fill helper: writes N words of a constant.
  static void clearWords(Core *c);

  // FUN_0x80086288 — the VBlank handler body installed by initVblankCallbacks(): bump the libetc
  // VSync tick counter, then run every registered VSyncCallback in the 8-slot table. See the
  // banner in libapi_intr.cpp for how it was identified and why it is an ordinary port.
  static void runVblankCallbacks(Core *c);

  static void registerOverrides(Game *game);

private:
  inline static constexpr GuestFrameSpill kInitVblankSpills[1] = {{31u, 16u}};
  inline static constexpr GuestFrameSpill kRunVblankSpills[3] = {{17u, 20u}, {16u, 16u}, {31u, 24u}};
};

} // namespace tomba
