// game/ui/dialog_text_stream.cpp — WIDE-RE DRAFT (2026-07-08, worktree agent-a53f252288693983d).
// UNWIRED, UNVERIFIED — compiles, no override registration, no SBS run. See dialog_text_stream.h
// for the struct-field map.
//
// RE SOURCE: Ghidra headless decompile (the Ghidra evidence workflow, project scratch/ghidra/main_ram,
// scratch/decomp/ui_8007.c) cross-checked line-for-line against authenticated executable/overlay evidence's
// guest 0x8007C0D0 and authenticated executable/overlay evidence's guest 0x8007D0D0 (the raw guest MIPS — the
// CLAUDE.md-mandated ground truth). Every branch below was traced against both.
//
// FINDING (worth keeping — avoids re-deriving this): guest 0x8007C0D0's 0xF8/0xF9 "mode-marker"
// byte handling reads a 6-entry pointer table at 0x80016EBC (indexed by dialog subtype, obj+3) and
// hands the result to `typed runtime address dispatch(c, addr); return;` UNCONDITIONALLY, with NO frame unwind before
// the call. That looks like a real indirect call to a separate function, but it is NOT: the sibling
// function guest 0x8007D0D0 has the EXACT same pattern (table at 0x80017384-ish, 6 entries) and
// there the recorded binary evidence DID statically resolve 2 of its 6 entries to local `goto`s within
// guest 0x8007D0D0 itself (`case 0x8007D100u: goto L_8007D100;` / `case 0x8007D13Cu: goto
// L_8007D13C;`), proving the table holds LOCAL CASE LABELS inside the same function, not distinct
// callable functions — the recorded binary evidence's static table-recognition just didn't cover every entry in
// FUN_8007C0D0's variant. Ghidra's high-level decompile independently shows the exact same 6
// per-subtype effects as an ordinary switch with NO call at all, which corroborates this. Root
// cause: MIPS jump-table-compiled switch statements look identical to a genuine computed call at
// the raw-instruction level our recorded binary evidence works from; it can only prove "local" when it happens to
// also see the label addresses appear elsewhere as branch targets. CONSEQUENCE for future wiring:
// do NOT `typed runtime address dispatch` this table read (there is no real registered function at those raw
// addresses) — reproduce the per-subtype effect directly, as done below.
#include "dialog_text_stream.h"
#include "core.h"
#include <cstdint>

namespace {
constexpr uint32_t OFF_SUBTYPE = 0x03;         // dialog box subtype (0-5 gated, >=6 plain)
constexpr uint32_t OFF_STATE = 0x05;           // box top-level state (owned by FUN_8007D594)
constexpr uint32_t OFF_CURSOR = 0x14;          // script byte-stream cursor
constexpr uint32_t OFF_COL = 0x2A;             // glyph column counter on current line
constexpr uint32_t OFF_MODE_TIMER = 0x40;      // render-mode timer (applyRenderMode + 0xFC control byte)
constexpr uint32_t OFF_BOX_TIMER = 0x42;       // secondary per-box timer (0xF8/0xF9 mode-marker, 0xFF term)
constexpr uint32_t G_TEXT_SPEED = 0x800BF8A3u; // DAT_800bf8a3 -- text-speed/language mode byte
enum { R_A0 = 4, R_A1 = 5, R_V0 = 2 };
} // namespace

// FUN_8007D0D0(obj a0) -- LEAF (guest 0x8007D0D0 has no `sp` descent). Cross-checked verbatim
// against authenticated executable/overlay evidence:guest 0x8007D0D0.
// ORACLE: guest 0x8007D0D0
void DialogTextStream::applyRenderMode(Core *c) {
  const uint32_t obj = c->r[R_A0];
  uint8_t subtype = c->mem_r8(obj + OFF_SUBTYPE);
  if (subtype == 0 || subtype == 1) {
    // fall through to the text-speed/language check below
  } else if (subtype >= 2 && subtype <= 5) {
    c->mem_w16(obj + OFF_MODE_TIMER, 1);
    return;
  } else {
    return; // subtype >= 6: no-op
  }
  uint8_t speed = c->mem_r8(G_TEXT_SPEED);
  if (speed == 0) {
    c->mem_w16(obj + OFF_MODE_TIMER, 3);
  } else if (speed == 1) {
    c->mem_w16(obj + OFF_MODE_TIMER, 2);
  } else {
    c->mem_w16(obj + OFF_MODE_TIMER, 1);
  }
}
