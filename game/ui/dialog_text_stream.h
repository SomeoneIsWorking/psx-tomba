// Dialog text render-mode owner. Original byte-stream advancement executes through the JIT.
//
// Struct: the dialog-box OBJECT (guest struct, fields below; only the ones this cluster touches):
//   +0x03 u8   subtype     — box "kind" (0-5 = pause/prompt variants that gate a render-mode timer
//                            via applyRenderMode; >=6 = plain message box, no timer gating)
//   +0x05 u8   state       — the box's OWN top-level state machine value (owned/written by
//                            FUB_8007D594, NOT this cluster) — advanceByte transitions it to
//                            2/3/4/5 on hitting the 0xFF terminator or an 0xF8/0xF9 mode-marker byte
//   +0x0D i8   ??          — read by callers (FUN_8007D208) as a "-1 = suppressed" gate; not
//                            written by this cluster
//   +0x10 u32  lineStart   — cursor snapshot at the start of the current line (set by the NOT-
//                            drafted FUN_8007D14C, read nowhere in this cluster)
//   +0x14 u32  cursor      — pointer into the dialog script byte stream (THE cursor advanceByte
//                            reads from and increments)
//   +0x18 i8   ??          — read by FUN_8007D208 (box-position variant selector), not touched here
//   +0x2A i8   col         — glyph column counter on the current line (incremented once per
//                            consumed byte, including control bytes — used by FUN_8007D208's
//                            box-height layout, not read here)
//   +0x40 u16  modeTimer   — render-mode timer set by applyRenderMode (FUN_8007D0D0) and by the
//                            0xFC "commit" control byte (both write 0xF/1, meaning: distinct scales
//                            depending on path — see applyRenderMode)
//   +0x42 u16  boxTimer    — secondary per-box timer; 0xFFFF = "off". Set by the 0xF8/0xF9 mode-
//                            marker dispatch (0/0xFFFF/0x3C per subtype) and by the 0xFF terminator
//                            (forced to 0xFFFF alongside state=2)
//
// advanceByte's `mode` parameter (a1): 1 = "committing" pass (the box is actually being laid out
// this frame — control bytes take effect, e.g. render-mode timers arm); 0 = a "peek/measure" pass
// (control bytes are walked over without side effects, used when e.g. FUN_8007D208 measures line
// width ahead of the commit pass). This mirrors Ghidra's `param_2 == 1` gate on every control-byte
// branch, cross-checked against authenticated executable/overlay evidence:guest 0x8007C0D0's raw MIPS.
#pragma once
struct Core;

class DialogTextStream {
public:
  // FUN_8007D0D0(obj a0) — sets modeTimer (obj+0x40) from subtype (obj+3) crossed with the global
  // text-speed/language byte DAT_800bf8a3. LEAF: guest 0x8007D0D0 has no `sp` descent at all.
  static void applyRenderMode(Core *c);
};
