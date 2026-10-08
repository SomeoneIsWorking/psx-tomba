// PC-native engine FONT / TEXT system init — reimplementing FUN_80075130 (called from game_main's
// init prefix). Per the boundary (CLAUDE.md): the front-end UI / text system is ENGINE → reimplement
// PC-native. FUN_80075130 sets a few engine-state fields directly and orchestrates 14 callees.
//
// SCOPE (deferred-libgpu carve-out, per docs/port-progress.md §A): own the ORCHESTRATION + the direct
// memsets/field writes + the 3 ENGINE-STATE callees (FUN_800963a0 font-bank select, FUN_80096370
// font-bank2, FUN_800752b4 glyph-class table fill). KEEP the 8 libgpu/libgs/sound callees as
// `typed runtime address dispatch` IN-CONTEXT, exactly where the guest instruction path calls them — they do indirect
// draw-env / FntLoad/FntOpen setup and carry the later-182b nested-dispatch divergence risk, so we do NOT own them.
//
// CRITICAL stack detail: two of the KEPT (dispatched) libgpu callees (0x80098330 / 0x80098d30) read a
// struct that FUN_80075130 builds on ITS OWN STACK FRAME at sp+16. So this native orchestrator allocates
// the same `sp -= 48` frame, populates sp+16..sp+26, and passes `a0 = sp+16` to them — otherwise the
// dispatched FntOpen reads garbage. The frame is torn down (sp/ra restored) at the end (mirror epilogue).
//
// RE source: tools/disas.py 0x80075130 / 0x800963a0 / 0x80096370 / 0x800752b4 (+ --mem). Full RE table +
// per-callee semantics in docs/engine_re.md "FUN_80075130 font / text init". Store widths are exact
// (sb/sh/sw) — they are the engine-interface state the rest of the engine + retained PSX content read.
#include "ui/font.h"
#include "core.h"
#include "core/overrides/guest_jal.h" // GuestFrame/guest_fn — ABI vocabulary (2026-07-15 readability pass)
#include "core/overrides/native_override_catalog.h"
#include "game.h"
#include "guest_abi.h"
#include "guest_call.h"
#include "render/ordering_table.h"
#include <format>
#include <lucent/log.h>
#include <stdint.h>
#include <string>

namespace {
// Font-bank engine-state bytes (own leaves FUN_800963a0/FUN_80096370).
constexpr uint32_t kFontBankAddr = 0x80105CECu;
constexpr uint32_t kFontBank2Addr = 0x80105D28u;

// Glyph-class table (FUN_800752b4): 24 entries, stride 12, class byte at entry+8.
constexpr uint32_t kGlyphClassTableBase = 0x800BE238u;
constexpr uint32_t kGlyphClassStride = 12u;
constexpr uint32_t kGlyphClassFieldOffset = 8u;
constexpr int32_t kGlyphClassCount = 24;

// Direct engine-state fields FUN_80075130 seeds around the libgpu FntOpen block.
constexpr uint32_t kTextCursorFlagAddr = 0x800BED78u; // sw 0                     [800751a0/9c]
constexpr uint32_t kTextUnusedFlagAddr = 0x800BED80u; // sh -1 (jal #9 delay slot)
constexpr uint32_t kLineTableHeadAddr = 0x800BE358u;  // sw 0 (once)
constexpr uint32_t kLineTableRowsAddr = 0x800BE3D6u;  // 14x sh 0, stepping -8
constexpr uint32_t kLineTableRowCount = 14u;
constexpr uint32_t kLineTableRowStride = 8u;
constexpr uint32_t kTextStateByteA = 0x800BE22Au; // sb 0
constexpr uint32_t kTextStateByteB = 0x800BE22Bu; // sb 0

// FntOpenParams — typed lens over the local FntOpen-call struct FUN_80075130 builds on its OWN
// stack frame at sp+16 (12 bytes: count u32@0, flags u32@4, size u16@8 == u16@10). The two
// dispatched libgpu callees (0x80098330/0x80098d30) read this struct by address (a0 = sp+16), so
// the frame must be real guest-stack bytes, not a native local.
struct FntOpenParams {
  Core *c;
  uint32_t base; // == fsp + 16
  void setCount(int32_t v) {
    c->mem_w32(base + 0u, (uint32_t)v);
  }
  void setFlags(int32_t v) {
    c->mem_w32(base + 4u, (uint32_t)v);
  }
  // Both halves store the SAME value — sp+26 is NOT a computed return, the original `sh v0,26(sp)`
  // runs in the #10 call's delay slot with v0 still holding the size assigned just above.
  void setSize(uint16_t v) {
    c->mem_w16(base + 8u, v);
    c->mem_w16(base + 10u, v);
  }
};

// Font::init's own guest-stack frame (sp-=48; sw ra,40(sp)) — confirmed via
// `tools/binary ABI evidence 0x80075130 --contract`: the ONLY prologue spill is ra at sp+40.
constexpr GuestFrameSpill kInitSpills[] = {{31, 40}};
} // namespace

// FUN_800963a0 — font-bank selector. If ((bank-1)&0xff) < 24, store the bank byte at
// kFontBankAddr and return the sign-extended low byte; otherwise return -1. (At the init call
// bank=24 → (24-1)&0xff = 23 < 24 → store 24, return 24.) Leaf, no sub-calls (frame_size=0).
void Font::bankSelect(uint32_t bank) {
  Core *c = this->core;
  uint32_t v = (bank - 1) & 0xff;
  if (v < 24) {
    c->mem_w8(kFontBankAddr, (uint8_t)bank);
    c->r[2] = (uint32_t)(int32_t)(int8_t)(uint8_t)bank; // (bank<<24)>>24 : sign-extend low byte
  } else {
    c->r[2] = (uint32_t)-1;
  }
}

// FUN_80096370 — font-bank2 store. `*kFontBank2Addr(sb) = bank; jr ra`. Leaf; does NOT set v0
// (guest instruction path left v0 untouched — the caller ignores it). At the init call bank=0.
void Font::bank2Store(uint32_t bank) {
  this->core->mem_w8(kFontBank2Addr, (uint8_t)bank);
}

// FUN_800752b4 — glyph-class table fill. Iterates i = 0..23 over the 24-entry table. Thresholds
// from cls: t1=24-cls, t0=16-cls, a3=12-cls, a4=8-cls. The slt/bne tests branch AWAY when (i<thr)
// is true, so the fall-through (i>=thr) assigns:
//   i>=t1 ->4 ; i>=t0 ->1 ; i>=a3 ->3 ; i>=a4 ->2 ; else ->0   (exclusive cascade, first match wins).
// Returns the count in v0 but the caller IGNORES it.
void Font::glyphClassFill(int32_t cls) {
  Core *c = this->core;
  int32_t t1 = 24 - cls, t0 = 16 - cls, a3 = 12 - cls, a4 = 8 - cls;
  for (int i = 0; i < kGlyphClassCount; i++) {
    uint8_t val;
    if (i >= t1) {
      val = 4;
    } else if (i >= t0) {
      val = 1;
    } else if (i >= a3) {
      val = 3;
    } else if (i >= a4) {
      val = 2;
    } else {
      val = 0;
    }
    c->mem_w8(kGlyphClassTableBase + (uint32_t)i * kGlyphClassStride + kGlyphClassFieldOffset, val);
  }
  c->r[2] = (uint32_t)kGlyphClassCount; // loop-exit count (caller ignores)
}

// FUN_80075130 — font / text system init orchestrator. No args, no return. Mirrors the guest instruction path frame
// (sp -= 48; sw ra,40(sp)) because dispatched callees #11/#13 read a struct at sp+16. Owns the direct
// writes + the 3 engine callees; guest_fn-dispatches the 8 libgpu/sound callees IN ORDER, IN-CONTEXT,
// using the jal-site ra constants `tools/binary ABI evidence 0x80075130 --contract` reports per call site
// (single exit point — safe for GuestFrame RAII per the tail-jump gotcha in docs/faithful-execution.md).
void Font::init() {
  Core *c = this->core;
  GuestFrame<48, 1> frame(c, kInitSpills);
  uint32_t fsp = c->r[29];
  FntOpenParams fntOpen{c, fsp + 16u};

  // #1 sound/libgs/lib init — dispatched
  tomba::guest::dispatchJalToReturn(*c, 0x8008e040u, 0x80075140u);

  // #2 FUN_800963a0(24) — own
  bankSelect(24);
  // #3 FUN_80096370(0) — own
  bank2Store(0);

  // #4 FUN_80098f90(0, 0xffffff) — dispatched
  tomba::guest::dispatchJalToReturn(*c, 0x80098f90u, 0x80075160u, 0u, 0x00ffffffu);
  // #5 FUN_80091d70(1) — dispatched
  tomba::guest::dispatchJalToReturn(*c, 0x80091d70u, 0x80075168u, 1u);
  // #6 FUN_80091b50(0x800be3d8, 14, 1) — dispatched
  tomba::guest::dispatchJalToReturn(*c, 0x80091b50u, 0x8007517Cu, 0x800be3d8u, 14u, 1u);
  // #7 FUN_80090700(127, 127)  (a1 = a0 in the original delay slot) — dispatched
  tomba::guest::dispatchJalToReturn(*c, 0x80090700u, 0x80075188u, 127u, 127u);
  // #8 FUN_80090980() — dispatched
  tomba::guest::dispatchJalToReturn(*c, 0x80090980u, 0x80075190u);

  // direct: *kTextCursorFlagAddr = 0 (sw)  [800751a0/9c]
  c->mem_w32(kTextCursorFlagAddr, 0);
  // *kTextUnusedFlagAddr = -1 (sh) — original is the DELAY SLOT of the #9 jal (v0=-1), runs before
  // #9's body.
  c->mem_w16(kTextUnusedFlagAddr, 0xffff);
  // #9 FUN_800752b4(2) — own
  glyphClassFill(2);

  // direct: kLineTableHeadAddr = 0 (sw, once), then kLineTableRowCount x sh 0 stepping backward.
  c->mem_w32(kLineTableHeadAddr, 0);
  for (uint32_t addr = kLineTableRowsAddr, n = 0; n < kLineTableRowCount; n++, addr -= kLineTableRowStride) {
    c->mem_w16(addr, 0);
  }

  // FntOpen params consumed by the dispatched #11/#13 (they read the struct by address, a0=fsp+16).
  fntOpen.setCount(7);
  fntOpen.setFlags(258);
  fntOpen.setSize(16384);

  // #10 FUN_80098ce0(1) — dispatched
  tomba::guest::dispatchJalToReturn(*c, 0x80098ce0u, 0x800751F8u, 1u);
  // #11 FUN_80098330(fsp+16) — dispatched (reads the FntOpen struct above)
  tomba::guest::dispatchJalToReturn(*c, 0x80098330u, 0x80075200u, fntOpen.base);
  // #12 FUN_80098150(1) — dispatched
  tomba::guest::dispatchJalToReturn(*c, 0x80098150u, 0x80075208u, 1u);
  // #13 FUN_80098d30(fsp+16) — dispatched (reads the FntOpen struct)
  tomba::guest::dispatchJalToReturn(*c, 0x80098d30u, 0x80075210u, fntOpen.base);
  // #14 FUN_80098db0(1, 0xffffff) — dispatched
  tomba::guest::dispatchJalToReturn(*c, 0x80098db0u, 0x80075220u, 1u, 0x00ffffffu);

  // direct: kTextStateByteA/B = 0 (sb each)
  c->mem_w8(kTextStateByteA, 0);
  c->mem_w8(kTextStateByteB, 0);

  // frame's destructor: lw ra,40(sp); addiu sp,48; jr ra
}

// FUN_80073750 — pure string measurer (disas 0x80073750..0x80073798, no sub-calls):
//   prefix = 0; suffix = 0; sawNewline = false;
//   for (ch = *p; ch != 0; ch = *++p) {
//     if (ch == '\n') sawNewline = true;
//     p++; if (sawNewline) suffix++; else prefix++;
//   }
//   NOTE: the '\n' char itself lands in `suffix` (sawNewline flips true the SAME iteration it's
//   read, and the increment below it uses the now-true flag) and, once true, sawNewline never
//   resets — a SECOND embedded '\n' is just an ordinary char counted into `suffix`.
//   if (sawNewline) return -(max(prefix, suffix));
//   else             return prefix;
int32_t Font::measureLineWidth(Core *c, uint32_t strAddr) {
  int32_t prefix = 0, suffix = 0;
  bool sawNewline = false;
  uint32_t p = strAddr;
  uint8_t ch = c->mem_r8(p);
  while (ch != 0) {
    if (ch == '\n') {
      sawNewline = true;
    }
    p += 1;
    if (sawNewline) {
      suffix += 1;
    } else {
      prefix += 1;
    }
    ch = c->mem_r8(p);
  }
  c->r[4] = p; // ABI leftover: a0 ends at the NUL terminator (see header doc)
  if (sawNewline) {
    if (prefix < suffix) {
      prefix = suffix;
    }
    return -prefix;
  }
  return prefix;
}

namespace {
// Font::drawText's guest-stack frame (sp-=32; sw ra,24(sp)) — confirmed via
// `tools/binary ABI evidence 0x80079374 --contract`: the only real register spill is ra at sp+24 (the
// contract's other sp+16/sp+48 "r8" entries are the incoming/outgoing `color` stack argument, not
// a callee-save spill — handled explicitly below, same as the guest instruction path does).
constexpr GuestFrameSpill kDrawTextSpills[] = {{31, 24}};

constexpr uint32_t kScrGlyphAdvance = 0x1F800180u; // per-call horizontal-advance scratch (role
                                                   // unconfirmed; glyphEmit reads it back)
} // namespace

// FUN_80079374 — WIDE-RE TIER DRAFT (2026-07-09), UNWIRED/UNVERIFIED. See header doc for the
// full RE. Mirrors the guest frame because the callee it tail-calls (still-unowned FUN_80078CA8)
// is reached via typed runtime address dispatch and expects the caller's stack-arg convention (5th arg at sp+16 of
// ITS caller's frame, i.e. THIS frame after the sp-=32 descent) — single exit point, safe for
// GuestFrame RAII per the tail-jump gotcha in docs/faithful-execution.md.
void Font::drawText(Core *c, int32_t x, int32_t y, int32_t w, uint32_t str, uint32_t color) {
  GuestFrame<32, 1> frame(c, kDrawTextSpills);

  // a0' = (int16)x | (y << 16) — packed vertex {x: lo16 sign-extended, y: hi16}
  uint32_t vertex = (uint32_t)(int32_t)(int16_t)(uint16_t)x | ((uint32_t)y << 16);
  // a1' = constant 0x00100008 (original a1/w argument is discarded — confirmed from the guest-visible behavior)
  constexpr uint32_t kA1Const = 0x00100008u;
  // a2' = (int16)w — sign-extended low16(w) ONLY. BUG FIX (verify pass): the prior draft OR'd a
  // fabricated "h" arg into the upper 16 bits (see font.h header for the call-site trace proving
  // there is no h parameter in the real 5-arg guest ABI: x,y,w,str,color).
  uint32_t size = (uint32_t)(int32_t)(int16_t)(uint16_t)w;

  c->mem_w16(kScrGlyphAdvance, 32); // sh v0(32),384(v1) — scratchpad write, role unconfirmed

  c->mem_w32(c->r[29] + 16, color); // 5th arg on stack, at the callee's expected slot
  tomba::guest::dispatchJalToReturn(
      *c, 0x80078CA8u, 0x800793B4u, vertex, kA1Const, size, str); // FUN_80078CA8 (still unowned)
}

// ORACLE: guest 0x80079324
// FUN_80079324 — SIBLING of drawText (0x80079374): the SAME arg-packing wrapper around the same
// still-unowned emitter FUN_80078CA8, differing only in the a1 constant (0x00080008 = {w:8,h:8},
// half-height 8x8 glyphs vs drawText's 0x00100008) and the scratchpad advance value it writes to
// 0x1F800180 before the call (-32 vs drawText's +32), and the tail-call return-address constant
// (0x80079364 vs 0x800793B4). Byte-faithful to guest 0x80079324; mirrors the guest frame (sp-=32,
// ra spilled at sp+24) exactly as drawText does — single exit point, safe for GuestFrame RAII.
void Font::drawTextSmall(Core *c, int32_t x, int32_t y, int32_t w, uint32_t str, uint32_t color) {
  GuestFrame<32, 1> frame(c, kDrawTextSpills);

  // a0' = (int16)x | (y << 16) — packed vertex {x: lo16 sign-extended, y: hi16}
  uint32_t vertex = (uint32_t)(int32_t)(int16_t)(uint16_t)x | ((uint32_t)y << 16);
  // a1' = constant 0x00080008 ({w:8, h:8}) — the incoming a1/w argument is discarded, same as drawText.
  constexpr uint32_t kA1Const = 0x00080008u;
  // a2' = (int16)w — sign-extended low16(w) only.
  uint32_t size = (uint32_t)(int32_t)(int16_t)(uint16_t)w;

  // Load a0..a3 exactly as gen does (a3=str passes through the caller's r7 untouched in gen; set it
  // explicitly here — same value — matching drawText's guest_fn 4th arg).
  c->r[4] = vertex;
  c->r[5] = kA1Const;
  c->r[6] = size;
  c->r[7] = str;

  c->mem_w16(kScrGlyphAdvance, (uint16_t)-32); // sh -32,384(v1) — scratchpad advance = -32

  c->mem_w32(c->r[29] + 16, color);                                // 5th arg on stack, at the callee's expected slot
  tomba::guest::dispatchJalToReturn(*c, 0x80078CA8u, 0x80079364u); // ra=0x80079364; FUN_80078CA8 (still unowned)
}

// FUN_80078CA8 — the font/glyph emitter drawText() tail-calls. WIDE-RE TIER DRAFT (2026-07-10,
// disjoint band), UNWIRED/UNVERIFIED. Faithful to guest 0x80078CA8 (authenticated executable/overlay evidence),
// LIVE BODY ONLY (gen-C lines 1-210; 211-402 is confirmed-unreachable dead code, no label targets
// it). See font.h for the full RE writeup (per-byte dispatch table, scratch-struct layout,
// dead-tail note). Guest-stack frame mirrored (sp-56, spill ra/s0-s5 at their RE'd offsets: r16..
// r21 = s0..s5). Kept register-literal with goto/labels named after the guest addresses (dense
// character-class branching with a shared tail reached from 5 different arms).
namespace {
// Census (channel "textemit"). Reports EVERY call with the string it was handed, because the
// question this answers is which producer draws a given piece of text — and an absent call is the
// answer as often as a present one. The bytes are printed raw: control bytes 0x01..0x04 are the
// icon-glyph arms, and seeing one here but no matching iconGlyphEmit call is a live defect.
void reportTextEmit(Core *c, const char *producer, int x, int y) {
  if (!lucent::channel_on("textemit")) {
    return; // the reporter's own early-out: reading and formatting the guest string is the cost here,
            // not the log call, and every text draw in the game comes through this path
  }
  const uint32_t string = c->r[7];
  std::string bytes, text;
  for (uint32_t i = 0; i < 48u; ++i) {
    const uint8_t byte = c->mem_r8(string + i);
    if (byte == 0) {
      break;
    }
    bytes += std::format("{:02x}", byte);
    text += (byte >= 0x20 && byte < 0x7F) ? (char)byte : '.';
  }
  lucent::debug("textemit",
                "f{} {} at ({},{}) colour {} ra {:08x} str {:08x} = [{}] {}",
                c->game->gpu.s_frame,
                producer,
                x,
                y,
                c->mem_r32(c->r[29] + 16u),
                c->r[31],
                string,
                text,
                bytes);
}

} // namespace

void Font::glyphEmit(Core *c) {
  const uint32_t string = c->r[7];
  uint32_t sp0 = c->r[29];
  c->r[29] = sp0 - 56u;
  c->mem_w32(c->r[29] + 44u, c->r[21]);
  c->r[21] = c->r[4] + c->r[0]; // r21 = vertex arg {x:lo16, y:hi16}
  c->mem_w32(c->r[29] + 24u, c->r[16]);
  c->r[16] = c->r[7] + c->r[0]; // r16 = str cursor (a3)
  c->mem_w32(c->r[29] + 32u, c->r[18]);
  c->r[18] = ((uint32_t)8064u << 16); // 0x1F800000 -- glyph scratch struct base (SCRATCHPAD;
                                      // an earlier note here said 0x800C0000 — wrong, 8064=0x1F80)
  c->r[3] = c->r[6] + c->r[0];        // r3 = size arg {w:lo16, h:hi16} (a2)
  c->r[2] = c->r[6] << 16;
  c->r[2] = (uint32_t)((int32_t)c->r[2] >> 16); // sign-extended low16(size) = w
  c->mem_w32(c->r[29] + 36u, c->r[19]);
  c->r[19] = c->mem_r32(c->r[29] + 72u); // 5th arg (color), caller's stack[+16]
  c->r[2] = (uint32_t)((int32_t)c->r[2] < 16);
  c->mem_w32(c->r[29] + 48u, c->r[31]);
  c->mem_w32(c->r[29] + 40u, c->r[20]);
  c->mem_w32(c->r[29] + 28u, c->r[17]);
  c->mem_w32(c->r[18] + 8u, c->r[21]); // struct+8 (cursor-x u16 slot, written as u32 here -- low16 is x)
  {
    int _t = (c->r[2] != c->r[0]);
    c->mem_w32(c->r[18] + 16u, c->r[5]); // struct+16 = a1 (drawText's 0x00100008 constant)
    if (_t) {
      goto L_80078D04;
    }
  }
  c->r[2] = c->r[6] + 480u;
  c->r[2] = c->r[2] << 6;
  c->r[2] = c->r[2] | 62u;
  goto L_80078D10;
L_80078D04:
  c->r[2] = c->r[6] + 496u;
  c->r[2] = c->r[2] << 6;
  c->r[2] = c->r[2] | 63u;
L_80078D10:
  c->mem_w16(c->r[18] + 14u, (uint16_t)c->r[2]);
  c->r[2] = c->r[0] + 101u;
  c->mem_w8(c->r[18] + 7u, (uint8_t)c->r[2]);
  c->r[2] = (uint32_t)c->mem_r8(c->r[16] + 0u);
  {
    int _t = (c->r[2] == c->r[0]);
    c->r[17] = c->r[3] << 16;
    if (_t) {
      goto L_80078F88; // empty string -- straight to tail
    }
  }
  c->r[20] = 0x1F800000u; // scratchpad base (r20 is reused as scratchpad base here)
  c->r[3] = c->r[2] & 255u;
L_80078D34:
  c->r[2] = c->r[0] + 32u;
  {
    int _t = (c->r[3] != c->r[2]);
    c->r[2] = c->r[0] + 10u; // delay-slot literal, live at L_80078D4C
    if (_t) {
      goto L_80078D4C;
    }
  }
  // byte == 0x20 (' ') -- advance-cursor tail only
  c->r[2] = (uint32_t)c->mem_r16(c->r[18] + 8u);
  c->r[16] = c->r[16] + 1u;
  goto L_80078F70;
L_80078D4C: {
  int _t = (c->r[3] != c->r[2]); // r2 == 10 here (delay-slot literal from above)
  c->r[2] = c->r[0] + 1u;        // delay-slot literal, live at L_80078D74
  if (_t) {
    goto L_80078D74;
  }
}
  // byte == 0x0A ('\n') -- line break: reset x, y += "line height" (struct+18)
  c->r[16] = c->r[16] + 1u;
  c->r[2] = (uint32_t)c->mem_r16(c->r[18] + 10u);
  c->r[4] = (uint32_t)c->mem_r16(c->r[18] + 18u);
  c->r[3] = c->r[21] & 4095u;
  c->mem_w16(c->r[18] + 8u, (uint16_t)c->r[3]);
  c->r[2] = c->r[2] + c->r[4];
  c->mem_w16(c->r[18] + 10u, (uint16_t)c->r[2]);
  goto L_80078F78;
L_80078D74: {
  int _t = (c->r[3] != c->r[2]); // r2 == 1 here
  c->r[2] = c->r[0] + 2u;        // delay-slot literal, live at L_80078DB4
  if (_t) {
    goto L_80078DB4;
  }
}
  // byte == 0x01 -- FUN_80078988(cursorX, cursorY, w, tablePtr=0x80010000+28072)
  c->r[6] = (uint32_t)((int32_t)c->r[17] >> 16);
  c->r[7] = ((uint32_t)32769u << 16);
  c->r[4] = (uint32_t)c->mem_r16(c->r[18] + 8u);
  c->r[7] = c->r[7] + 28072u;
  c->mem_w32(c->r[29] + 16u, c->r[19]);
  c->r[5] = (uint32_t)c->mem_r16(c->r[18] + 10u);
  c->r[4] = c->r[4] << 16;
  c->r[4] = (uint32_t)((int32_t)c->r[4] >> 16);
  c->r[5] = c->r[5] << 16;
  c->r[31] = 0x80078DA8u;
  c->r[5] = (uint32_t)((int32_t)c->r[5] >> 16);
  psx::cpu::dispatchGuestToReturn0(*c,
                                   0x80078988u,
                                   psx::cpu::ExecutionBudget::currentTurn(*c),
                                   __func__); // FUN_80078988 -- still unowned, out of this wave's band
  c->r[2] = (uint32_t)c->mem_r16(c->r[18] + 8u);
  c->r[16] = c->r[16] + 1u;
  goto L_80078F70;
L_80078DB4: {
  int _t = (c->r[3] != c->r[2]); // r2 == 2 here
  c->r[2] = c->r[0] + 3u;        // delay-slot literal, live at L_80078DF4
  if (_t) {
    goto L_80078DF4;
  }
}
  // byte == 0x02 -- FUN_80078988(cursorX, cursorY, w, tablePtr=0x80010000+28076)
  c->r[6] = (uint32_t)((int32_t)c->r[17] >> 16);
  c->r[7] = ((uint32_t)32769u << 16);
  c->r[4] = (uint32_t)c->mem_r16(c->r[18] + 8u);
  c->r[7] = c->r[7] + 28076u;
  c->mem_w32(c->r[29] + 16u, c->r[19]);
  c->r[5] = (uint32_t)c->mem_r16(c->r[18] + 10u);
  c->r[4] = c->r[4] << 16;
  c->r[4] = (uint32_t)((int32_t)c->r[4] >> 16);
  c->r[5] = c->r[5] << 16;
  c->r[31] = 0x80078DE8u;
  c->r[5] = (uint32_t)((int32_t)c->r[5] >> 16);
  psx::cpu::dispatchGuestToReturn0(*c, 0x80078988u, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
  c->r[2] = (uint32_t)c->mem_r16(c->r[18] + 8u);
  c->r[16] = c->r[16] + 1u;
  goto L_80078F70;
L_80078DF4: {
  int _t = (c->r[3] != c->r[2]); // r2 == 3 here
  c->r[2] = c->r[0] + 4u;        // delay-slot literal, live at L_80078E34
  if (_t) {
    goto L_80078E34;
  }
}
  // byte == 0x03 -- FUN_80078988(cursorX, cursorY, w, tablePtr=0x80010000+28068)
  c->r[6] = (uint32_t)((int32_t)c->r[17] >> 16);
  c->r[7] = ((uint32_t)32769u << 16);
  c->r[4] = (uint32_t)c->mem_r16(c->r[18] + 8u);
  c->r[7] = c->r[7] + 28068u;
  c->mem_w32(c->r[29] + 16u, c->r[19]);
  c->r[5] = (uint32_t)c->mem_r16(c->r[18] + 10u);
  c->r[4] = c->r[4] << 16;
  c->r[4] = (uint32_t)((int32_t)c->r[4] >> 16);
  c->r[5] = c->r[5] << 16;
  c->r[31] = 0x80078E28u;
  c->r[5] = (uint32_t)((int32_t)c->r[5] >> 16);
  psx::cpu::dispatchGuestToReturn0(*c, 0x80078988u, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
  c->r[2] = (uint32_t)c->mem_r16(c->r[18] + 8u);
  c->r[16] = c->r[16] + 1u;
  goto L_80078F70;
L_80078E34: {
  int _t = (c->r[3] != c->r[2]);                 // r2 == 4 here
  c->r[6] = (uint32_t)((int32_t)c->r[17] >> 16); // delay-slot, live at L_80078E70 too
  if (_t) {
    goto L_80078E70;
  }
}
  // byte == 0x04 -- FUN_80078988(cursorX, cursorY, w, tablePtr=0x80010000+28064)
  c->r[7] = ((uint32_t)32769u << 16);
  c->r[4] = (uint32_t)c->mem_r16(c->r[18] + 8u);
  c->r[7] = c->r[7] + 28064u;
  c->mem_w32(c->r[29] + 16u, c->r[19]);
  c->r[5] = (uint32_t)c->mem_r16(c->r[18] + 10u);
  c->r[4] = c->r[4] << 16;
  c->r[4] = (uint32_t)((int32_t)c->r[4] >> 16);
  c->r[5] = c->r[5] << 16;
  c->r[31] = 0x80078E64u;
  c->r[5] = (uint32_t)((int32_t)c->r[5] >> 16);
  psx::cpu::dispatchGuestToReturn0(*c, 0x80078988u, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
  c->r[2] = (uint32_t)c->mem_r16(c->r[18] + 8u);
  c->r[16] = c->r[16] + 1u;
  goto L_80078F70;
L_80078E70:
  // default arm -- ordinary glyph: compute per-glyph width/height, prepend GP0 packet at the pool.
  c->r[3] = (uint32_t)c->mem_r8(c->r[16] + 0u);
  c->r[2] = (uint32_t)(int16_t)c->mem_r16(c->r[20] + 384u); // scratchpad 0x1F800180 -- advance value
  c->r[4] = c->r[3] + c->r[2];
  {
    int _t = ((int32_t)c->r[4] >= 0);
    c->r[3] = c->r[4] + c->r[0];
    if (_t) {
      goto L_80078E8C;
    }
  }
  c->r[3] = c->r[4] + 31u;
L_80078E8C:
  c->r[3] = (uint32_t)((int32_t)c->r[3] >> 5);
  c->r[3] = c->r[3] << 5;
  c->r[2] = (uint32_t)c->mem_r16(c->r[18] + 16u);
  c->r[3] = c->r[4] - c->r[3];
  c->r[2] = c->r[2] << 16;
  c->r[2] = (uint32_t)((int32_t)c->r[2] >> 16);
  {
    int64_t _p = (int64_t)(int32_t)c->r[3] * (int64_t)(int32_t)c->r[2];
    c->lo = (uint32_t)_p;
    c->hi = (uint32_t)((uint64_t)_p >> 32);
  }
  c->r[8] = c->lo;
  c->mem_w8(c->r[18] + 12u, (uint8_t)c->r[8]);
  c->r[3] = (uint32_t)c->mem_r8(c->r[16] + 0u);
  c->r[2] = (uint32_t)(int16_t)c->mem_r16(c->r[20] + 384u);
  c->r[3] = c->r[3] + c->r[2];
  {
    int _t = ((int32_t)c->r[3] >= 0);
    if (_t) {
      goto L_80078ECC;
    }
  }
  c->r[3] = c->r[3] + 31u;
L_80078ECC:
  c->r[2] = (uint32_t)c->mem_r16(c->r[18] + 18u);
  c->r[3] = (uint32_t)((int32_t)c->r[3] >> 5);
  c->r[2] = c->r[2] << 16;
  c->r[2] = (uint32_t)((int32_t)c->r[2] >> 16);
  {
    int64_t _p = (int64_t)(int32_t)c->r[3] * (int64_t)(int32_t)c->r[2];
    c->lo = (uint32_t)_p;
    c->hi = (uint32_t)((uint64_t)_p >> 32);
  }
  c->r[3] = (uint32_t)c->mem_r16(c->r[18] + 18u);
  c->r[2] = c->r[0] + 16u;
  c->r[3] = c->r[3] << 16;
  c->r[3] = (uint32_t)((int32_t)c->r[3] >> 16);
  c->r[4] = c->lo;
  {
    int _t = (c->r[3] != c->r[2]);
    c->mem_w8(c->r[18] + 13u, (uint8_t)c->r[4]);
    if (_t) {
      goto L_80078F04;
    }
  }
  c->r[2] = c->r[4] + 8u;
  c->mem_w8(c->r[18] + 13u, (uint8_t)c->r[2]);
L_80078F04: {
  // A glyph is the element of its string named by its byte offset.
  const auto glyph = c->emission.element(c->r[16] - string);
  // prepend a 4-word GP0 packet from the packet pool into the OT bucket for this colorArg.
  const tomba2::render::PacketPool glyphPackets(*c);
  const auto glyphOt = tomba2::render::OrderingTable::active(*c);
  c->r[6] = tomba2::render::PacketPool::kCursor;
  c->r[4] = glyphPackets.cursor();
  c->r[5] = glyphOt.slot(c->r[19]);
  c->r[3] = 4u << 24;
  c->r[2] = glyphOt.link(c->r[4], 4u, c->r[19]);
  c->r[4] = c->r[4] + 4u;
  c->r[2] = c->mem_r32(c->r[18] + 4u);
  c->r[16] = c->r[16] + 1u;
  c->mem_w32(c->r[4] + 0u, c->r[2]);
  c->r[2] = c->mem_r32(c->r[18] + 8u);
  c->r[4] = c->r[4] + 4u;
  c->mem_w32(c->r[4] + 0u, c->r[2]);
  c->r[2] = c->mem_r32(c->r[18] + 12u);
  c->r[4] = c->r[4] + 4u;
  c->mem_w32(c->r[4] + 0u, c->r[2]);
  c->r[2] = c->mem_r32(c->r[18] + 16u);
  c->r[4] = c->r[4] + 4u;
  c->mem_w32(c->r[4] + 0u, c->r[2]);
  c->r[2] = (uint32_t)c->mem_r16(c->r[18] + 8u);
  c->r[4] = c->r[4] + 4u;
  glyphPackets.setCursor(c->r[4]);
}
L_80078F70:
  c->r[2] = c->r[2] + 8u;
  c->mem_w16(c->r[18] + 8u, (uint16_t)c->r[2]);
L_80078F78:
  c->r[2] = (uint32_t)c->mem_r8(c->r[16] + 0u);
  {
    int _t = (c->r[2] != c->r[0]);
    c->r[3] = c->r[2] & 255u;
    if (_t) {
      goto L_80078D34;
    }
  }
L_80078F88:
  // tail: final OT-chained packet via the already-owned guest 0x80083DE0 (draw-mode/texwin header).
  c->r[5] = c->r[0] + c->r[0];
  c->r[6] = c->r[5] + c->r[0];
  const tomba2::render::PacketPool tailPackets(*c);
  c->r[17] = tomba2::render::PacketPool::kCursor;
  c->r[16] = tailPackets.cursor();
  c->r[7] = c->r[0] + 31u;
  c->mem_w32(c->r[29] + 16u, c->r[0]);
  c->r[31] = 0x80078FA8u;
  c->r[4] = c->r[16] + c->r[0];
  psx::cpu::dispatchGuestToReturn0(*c,
                                   0x80083DE0u,
                                   psx::cpu::ExecutionBudget::currentTurn(*c),
                                   __func__); // guest 0x80083DE0 -- already owned, process-globally wired
  const auto tailOt = tomba2::render::OrderingTable::active(*c);
  c->r[4] = tailOt.slot(c->r[19]);
  c->r[3] = 2u << 24;
  c->r[2] = tailOt.link(c->r[16], 2u, c->r[19]);
  c->r[3] = tailPackets.cursor() + 12u;
  tailPackets.setCursor(c->r[3]);
  c->r[3] = c->r[21] & 65535u;
  c->r[2] = (uint32_t)c->mem_r16(c->r[18] + 8u);
  c->r[31] = c->mem_r32(c->r[29] + 48u);
  c->r[21] = c->mem_r32(c->r[29] + 44u);
  c->r[20] = c->mem_r32(c->r[29] + 40u);
  c->r[19] = c->mem_r32(c->r[29] + 36u);
  c->r[18] = c->mem_r32(c->r[29] + 32u);
  c->r[17] = c->mem_r32(c->r[29] + 28u);
  c->r[16] = c->mem_r32(c->r[29] + 24u);
  c->r[2] = c->r[2] << 16;
  c->r[2] = (uint32_t)((int32_t)c->r[2] >> 16);
  c->r[2] = c->r[2] - c->r[3]; // return value -- caller (drawText) discards it
  // sp0 IS the entry sp (saved before the -56 descent); the gen's `sp += 56` operates on the
  // DESCENDED sp. `sp0 + 56` overshot by 56 every call — the caller's frame slid up 0x38, warping
  // every subsequent packet/stack byte (MIRROR_VERIFY invocation #1: exit sp 801FE980 vs 801FE948).
  c->r[29] = sp0;
}

// ------------------------------------------------------------------------------------------------
// WIRING (verify pass, 2026-07-10, docs/fleet-workflow.md §9): drawText re-diffed line-by-line
// against authenticated executable/overlay evidence -- one real bug found+fixed (the fabricated "h" 6th argument,
// see font.h header for the full call-site trace). glyphEmit re-diffed against
// authenticated executable/overlay evidence -- byte-exact, no bugs found (also confirms the dead-tail-code claim:
// the live body's `return` at gen-C line 210 has no label past it). Both are PLAIN intra-shard C
// calls at their call sites (the cited guest address(c), not typed runtime address dispatch), so they wire via the
// oracle-gated tomba::native::declareOverride thunk -- SBS core B keeps running the pure original guest instructions
// body.
namespace {
// ov_drawText: extracts drawText's typed args from the guest ABI registers at function entry
// (a0..a2 = x,y,w; a3 = str; caller's stack[+16] = color -- matches guest 0x80079374's own read of
// sp+48 AFTER its own sp-=32, i.e. the SAME physical slot read here BEFORE any descent).
void ov_drawText(Core *c) {
  reportTextEmit(c, "drawText", (int32_t)c->r[4], (int32_t)c->r[5]);
  int32_t x = (int32_t)c->r[4];
  int32_t y = (int32_t)c->r[5];
  int32_t w = (int32_t)c->r[6];
  uint32_t str = c->r[7];
  uint32_t color = c->mem_r32(c->r[29] + 16u);
  Font::drawText(c, x, y, w, str, color);
}

// ov_drawTextSmall: sibling of ov_drawText for FUN_80079324 — same guest-ABI arg extraction (a0..a2
// = x,y,w; a3 = str; caller's stack[+16] = color, read BEFORE any descent).
void ov_drawTextSmall(Core *c) {
  int32_t x = (int32_t)c->r[4];
  int32_t y = (int32_t)c->r[5];
  int32_t w = (int32_t)c->r[6];
  uint32_t str = c->r[7];
  uint32_t color = c->mem_r32(c->r[29] + 16u);
  Font::drawTextSmall(c, x, y, w, str, color);
}

constexpr uint32_t kIconScratch = 0x1F800020u;
constexpr uint32_t kIconTokenTable = 0x800A55E0u;
constexpr uint32_t kSpritePacketWords = 3u;
constexpr uint32_t kDrawModePacketWords = 2u;
constexpr uint32_t kNoGlyph = 0xFF02u;
constexpr uint32_t kNewlineGlyph = 0x0A0Au;

constexpr GuestFrameSpill kIconGlyphSpills[9] = {
    {17, 28},
    {18, 32},
    {23, 52},
    {22, 48},
    {31, 56},
    {21, 44},
    {20, 40},
    {19, 36},
    {16, 24},
};

struct IconScratch {
  Core *c;

  int16_t x() const {
    return c->mem_r16s(kIconScratch + 8u);
  }
  int16_t y() const {
    return c->mem_r16s(kIconScratch + 10u);
  }
  uint16_t clut() const {
    return c->mem_r16(kIconScratch + 14u);
  }
  void setCommand(uint8_t command) {
    c->mem_w8(kIconScratch + 7u, command);
  }
  void setPosition(int x, int y) {
    c->mem_w16(kIconScratch + 8u, (uint16_t)x);
    c->mem_w16(kIconScratch + 10u, (uint16_t)y);
  }
  void setX(int x) {
    c->mem_w16(kIconScratch + 8u, (uint16_t)x);
  }
  void setY(int y) {
    c->mem_w16(kIconScratch + 10u, (uint16_t)y);
  }
  void setUv(int u, int v) {
    c->mem_w8(kIconScratch + 12u, (uint8_t)u);
    c->mem_w8(kIconScratch + 13u, (uint8_t)v);
  }
  void setClut(uint16_t value) {
    c->mem_w16(kIconScratch + 14u, value);
  }
};

void emitGuestIconSprite(Core *c, uint32_t bucket) {
  const tomba2::render::PacketPool packets(*c);
  const uint32_t packet = packets.cursor();
  tomba2::render::OrderingTable::active(*c).link(packet, kSpritePacketWords, bucket);
  c->mem_w32(packet + 4u, c->mem_r32(kIconScratch + 4u));
  c->mem_w32(packet + 8u, c->mem_r32(kIconScratch + 8u));
  c->mem_w32(packet + 12u, c->mem_r32(kIconScratch + 12u));
  packets.setCursor(packet + 16u);
}

} // namespace

// iconGlyphEmit — FUN_80078988, the SJIS/token ICON-GLYPH string emitter glyphEmit's 0x01..0x04
// special-char arms call (a0=x, a1=y, a2=size-class w, a3=2-byte-token string; 5th stack arg =
// OT bucket). RE from guest 0x80078988 (authenticated executable/overlay evidence): second scratchpad glyph
// struct at 0x1F800020, op-0x75 8x8 sprites, clut from the size class (w<16 → row w+496 x-nibble
// 0x3F, else w+480/0x3E). Token decode per 2-byte big-endian pair:
//   0x0A0A                         -> newline (x = arg x, y += 8)
//   pair+32160 &FFFF < 26          -> code = pair+32193   (SJIS fullwidth A-Z block)
//   pair+32127 &FFFF < 26          -> code = pair+32192   (second letter block)
//   pair+32177 &FFFF < 10          -> code = pair+32193   (SJIS fullwidth digits)
//   else: token table @0x800A55E0 ({strPtr,u16 code} stride 8, 2-byte compare, NULL-terminated)
//         -> matched code, miss -> 0xFF02 (advance-only)
// Emit: glyph quad at (x,y) uv=((code&31)<<3, ((code&0xFFF)>>5)<<3), x += 8; if code&0x8000 a
// combining-mark quad at the advanced x (u = code&0x1000 ? 64 : 56, v=64) then x += 5 more.
// ORACLE: guest 0x80078988
void Font::iconGlyphEmit(Core *c) {
  const uint32_t rawOriginX = c->r[4];
  const int originX = (int16_t)(uint16_t)c->r[4];
  const int originY = (int16_t)(uint16_t)c->r[5];
  const int32_t sizeClass = (int32_t)c->r[6];
  const uint32_t string = c->r[7];
  const uint32_t bucket = c->mem_r32(c->r[29] + 16u);

  GuestFrame<64, 9> frame(c, kIconGlyphSpills);
  GuestReg<16> tokenEntry(c);
  GuestReg<17> cursor(c);
  GuestReg<18> scratch(c);
  GuestReg<19> bucketOffset(c);
  GuestReg<20> packetPoolBase(c);
  GuestReg<21> otBaseRegister(c);
  GuestReg<22> savedOriginX(c);
  GuestReg<23> savedBucket(c);

  cursor = string;
  scratch = kIconScratch;
  savedBucket = bucket;
  savedOriginX = rawOriginX;
  packetPoolBase = tomba2::render::PacketPool::kCursorPage;
  otBaseRegister = tomba2::render::OrderingTable::kBasePointerPage;
  bucketOffset = bucket * 4u;

  // Census (channel "iconglyph"). A token that misses the table becomes kNoGlyph and only advances
  // the cursor — it draws NOTHING while the text around it still lays out correctly. That failure is
  // invisible in the picture except as a gap, so the miss count and its token bytes are reported
  // whether or not anything was drawn: a call that emitted zero glyphs is the interesting case here,
  // not the boring one.
  int tokensSeen = 0, glyphsEmitted = 0, tokensMissed = 0;

  IconScratch glyph{c};
  glyph.setCommand(0x75u);
  glyph.setPosition(originX, originY);
  const uint32_t clutRow = (sizeClass < 16) ? (uint32_t)sizeClass + 496u : (uint32_t)sizeClass + 480u;
  const uint16_t clut = (uint16_t)((clutRow << 6) | ((sizeClass < 16) ? 63u : 62u));
  glyph.setClut(clut);

  while (c->mem_r8(cursor) != 0) {
    const uint32_t pair = ((uint32_t)c->mem_r8(cursor) << 8) | c->mem_r8(cursor + 1u);
    uint32_t rawCode = kNoGlyph;
    if (pair == kNewlineGlyph) {
      rawCode = kNewlineGlyph;
    } else if (((pair + 32160u) & 0xFFFFu) < 26u) {
      rawCode = pair + 32193u;
    } else if (((pair + 32127u) & 0xFFFFu) < 26u) {
      rawCode = pair + 32192u;
    } else if (((pair + 32177u) & 0xFFFFu) < 10u) {
      rawCode = pair + 32193u;
    } else {
      tokenEntry = kIconTokenTable;
      while (c->mem_r32(tokenEntry) != 0) {
        c->r[4] = c->mem_r32(tokenEntry);
        c->r[5] = cursor;
        c->r[6] = 2u;
        tomba::guest::dispatchJalToReturn(*c, 0x8009A640u, 0x80078AA4u);
        if (c->r[2] == 0) {
          rawCode = c->mem_r16(tokenEntry + 4u);
          break;
        }
        tokenEntry += 8u;
      }
    }
    cursor += 2u;
    c->r[6] = rawCode;
    const uint32_t code = rawCode & 0xFFFFu;
    ++tokensSeen;

    if (code == kNoGlyph) {
      ++tokensMissed;
      lucent::debug("iconglyph", "  token {:04x} MISSED the table -> advance-only, nothing drawn", pair);
      glyph.setX(glyph.x() + 8);
      continue;
    }
    if (code == kNewlineGlyph) {
      glyph.setX(originX);
      glyph.setY(glyph.y() + 8);
      continue;
    }

    glyph.setUv((code & 31u) << 3, ((code & 0xFFFu) >> 5) << 3);
    emitGuestIconSprite(c, savedBucket);
    ++glyphsEmitted;
    lucent::debug("iconglyph",
                  "  token {:04x} -> code {:04x} at ({},{}) uv ({},{}) clut {:04x}",
                  pair,
                  code,
                  glyph.x(),
                  glyph.y(),
                  (code & 31u) << 3,
                  ((code & 0xFFFu) >> 5) << 3,
                  glyph.clut());
    glyph.setX(glyph.x() + 8);

    if (code & 0x8000u) {
      glyph.setUv((code & 0x1000u) ? 64 : 56, 64);
      emitGuestIconSprite(c, savedBucket);
      ++glyphsEmitted;
      glyph.setX(glyph.x() + 5);
    }
  }

  const tomba2::render::PacketPool packets(*c);
  const uint32_t drawModePacket = packets.cursor();
  tokenEntry = drawModePacket;
  cursor = tomba2::render::PacketPool::kCursorPage;
  c->mem_w32(c->r[29] + 16u, 0u);
  c->r[4] = drawModePacket;
  c->r[5] = 0u;
  c->r[6] = 0u;
  c->r[7] = 31u;
  tomba::guest::dispatchJalToReturn(*c, 0x80083DE0u, 0x80078C30u);

  const auto ot = tomba2::render::OrderingTable::active(*c);
  c->r[4] = ot.slot(savedBucket);
  ot.link(drawModePacket, kDrawModePacketWords, savedBucket);
  packets.setCursor(drawModePacket + 12u);

  lucent::debug("iconglyph",
                "f{} iconGlyphEmit at ({},{}) size {} bucket {}: {} token(s), {} glyph(s) emitted, {} missed",
                c->game->gpu.s_frame,
                originX,
                originY,
                sizeClass,
                bucket,
                tokensSeen,
                glyphsEmitted,
                tokensMissed);

  c->r[3] = (uint32_t)(int32_t)glyph.x();
  c->r[2] = (uint32_t)((int32_t)glyph.x() - originX);
}

void Font::registerOverrides() {
  static bool done = false;
  if (done) {
    return;
  }
  done = true;
  tomba::native::declareOverride(0x80079374u, "ov_drawText", ov_drawText);
  tomba::native::declareOverride(0x80079324u, "ov_drawTextSmall", ov_drawTextSmall); // 8x8 sibling of drawText
  // A text draw is the object of its string (a3); each glyph names its byte offset in it.
  tomba::native::declareOverride(
      0x80078CA8u, "Font::glyphEmit", Font::glyphEmit, psx::present::Producer{psx::present::Arg::A3});
  tomba::native::declareOverride(0x80078988u, "Font::iconGlyphEmit", Font::iconGlyphEmit); // icon/SJIS glyph strings
}
