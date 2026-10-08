// game/core/str.h — resident native string leaves with image-aware registration.
// The leaves are stateless; guest memory and registers remain owned by the supplied Core.
#pragma once
#include <stdint.h>
class Core;

namespace tomba {

class Str {
public:
  // Register the resident string leaves with the title's image-aware catalog.
  static void registerOverrides();

  // length(c, addr): FUN_80079528 — plain NUL-terminated C-string length. One of the two hottest
  //   unowned leaves in the game (~4235 dispatches / 600 frames of free-roam) — a generic strlen()
  //   called from all over the overlay set (menu/UI/text/asset-name code), not subsystem-specific.
  //   Disas 0x80079528..0x8007954C (tools/disas.py 0x80079528 --all 20): a plain byte scan, no
  //   sub-calls, no stack frame (leaf, sp untouched). Ghidra garbled this range (folded a second,
  //   UNREACHABLE function's bytes — 0x80079554 onward, no dispatch entry, no caller anywhere in
  //   authenticated executable/overlay evidence — into the same symbol; guest instruction path `guest 0x80079528` is
  //   instruction-exact ground truth and confirms only the strlen loop is reachable via the guest 0x80079528 entry
  //   point).
  static uint32_t length(Core *c, uint32_t addr);

  // copyBytes(c, dst, src, n): FUN_8009A3E0 — libc memcpy with a NULL-DESTINATION GUARD and a
  //   SIGNED length. Long known as "the memcpy-like out-of-band primitive" and left on the
  //   substrate (docs/engine_re.md, wide_re_gpu_putdrawenv.cpp); reading the 15-instruction body
  //   settles it. Returns dst, except dst == 0 which returns 0 without touching anything. n <= 0
  //   copies nothing and still returns dst — the guard is `> 0` on a SIGNED compare, so a negative
  //   length is a no-op rather than a 4-billion-byte copy.
  static uint32_t copyBytes(Core *c, uint32_t dst, uint32_t src, int32_t n);

  // FUN_0x8009A640 — byte compare, the sibling of copyBytes above. Guest-ABI entry point.
  static void compareBytes(Core *c);
};

} // namespace tomba
