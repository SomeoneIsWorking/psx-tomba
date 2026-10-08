// game/render/text_label.cpp — Render::textLabelEmit, the per-character 3D TEXT-LABEL renderer
// (FUN_80039F4C, renderWalk case 0x8003C0E8 — the REDIRECT census "0x80039F4C score strip").
//
// WHAT IT DRAWS (RE: Ghidra scratch/decomp/quad_emitters.c + ground truth authenticated executable/overlay evidence
// guest 0x80039F4C, cross-checked instruction-by-instruction):
//   - runs FUN_8003F174(node, 1) first — the node's MESH pass: per cmd (node+0xC0[i]) it loads GTE
//     CR0-7 DIRECTLY from the PRE-COMPOSED matrix stored on the cmd at +0x18..0x34 (NOT the
//     cmdListDispatch camera∘object compose — a different cmd layout for this node class) and runs
//     guest 0x8003F698 (the generic geomblk submit) — still substrate.
//   - then per CHARACTER of the label text: one glyph quad from the fixed template
//     V0(-3,-7,-1) V1(5,-7,-1) V2(-3,9,-1) V3(5,9,-1) (built into the REAL guest stack sp+16..47),
//     projected by guest 0x8003F7D8 (RTPT/RTPS/AVSZ4, same shape as QuadRtptSubmit::submitQuad) under
//     the per-char PRE-COMPOSED MATRIX at cmd+0x18 loaded via libgte SetRotMatrix/SetTransMatrix
//     (guest 0x80084660/guest 0x80084690 — NOT "pool-span markers"; that older note was a mis-RE).
//     Glyph UV comes from guest 0x80039E80 (char*8 → atlas u, ((char+32)>>5)*16+8 → v; space = skip);
//     material patches: code 0x2D (textured raw), tpage half 0x1F, clut 0x7DFF (0x7C7F for the
//     "Clear" variant node+3==2). Text = strcpy("Clear")+strcat(DAT_80014A1C) when node+3==2, else
//     string table 0x800A33CC[node.s16[+96] * 3].ptr (word +4 of the 12-byte entry).
//   - packet bump-copied at the pool tail (40 bytes) and OT-linked at (otz-1).
//
// Writes guest packets only; the glyphs reach the screen through the guest OT.
#include "core.h"
#include "core/entry/game_ctx.h"
#include "core/overrides/guest_jal.h" // GuestFrame / GuestFrameSpill / guest_call
#include "core/overrides/native_override_catalog.h"
#include "game.h"
#include "guest_abi.h"
#include "guest_ordering_table.h"
#include "horizontal_visibility_cull.h"
#include "render.h"
#include <stdint.h>

namespace {
// Guest-stack frame contract — tools/binary ABI evidence 0x80039F4C --scaffold --guestabi (ground truth).
constexpr GuestFrameSpill kSpills_80039F4C[7] = {
    {20, 104},
    {31 /*ra*/, 112},
    {21, 108},
    {19, 100},
    {18, 96},
    {17, 92},
    {16, 88},
};

// The fixed glyph template (model space, s16) the guest builds at sp+16..47 every call.
constexpr int16_t kGlyphX[4] = {-3, 5, -3, 5};
constexpr int16_t kGlyphY[4] = {-7, -7, 9, 9};
constexpr int16_t kGlyphZ = -1;

void textLabelBody(Core *c) {
  GuestFrame<120, 7> frame(c, kSpills_80039F4C);
  const uint32_t sp = c->r[29];
  const uint32_t node = c->r[4];
  c->r[20] = node; // gen keeps the node live in r20

  // (1) mesh pass: per-cmd pre-composed-matrix geomblk submit (still substrate).
  c->r[4] = node;
  c->r[5] = 1;
  tomba::guest::dispatchJalToReturn(*c, 0x8003F174u, 0x80039F78u);

  // (2) glyph template into the REAL guest stack (sp+16..47) — byte order per gen.
  for (int i = 0; i < 4; i++) {
    c->mem_w16(sp + 16u + (uint32_t)i * 8u, (uint16_t)kGlyphX[i]);
    c->mem_w16(sp + 18u + (uint32_t)i * 8u, (uint16_t)kGlyphY[i]);
    c->mem_w16(sp + 20u + (uint32_t)i * 8u, (uint16_t)kGlyphZ);
  }

  // (3) label text pointer: "Clear"+suffix into the guest stack buffer (sp+48), or the string table.
  uint32_t text;
  if (c->mem_r8(node + 3u) == 2u) {
    c->r[4] = sp + 48u;
    c->r[5] = c->mem_r32(0x800A3A8Cu); // strcpy(buf, "Clear")
    c->r[18] = sp + 48u;               // gen: r18 = buf (live)
    tomba::guest::dispatchJalToReturn(*c, 0x8009A5B0u, 0x80039FE4u);
    c->r[4] = c->r[18];
    c->r[5] = 0x80014A1Cu; // strcat(buf, suffix)
    tomba::guest::dispatchJalToReturn(*c, 0x8009A490u, 0x80039FF4u);
    text = c->r[18];
  } else {
    const int32_t idx = (int32_t)(int16_t)c->mem_r16(node + 96u);
    // gen: mem32(0x800A33C8 + idx*12 + 4) — 12-byte string-table entries based at 0x800A33C8, the
    // string POINTER is word +4 of the entry (a first draft mis-based this at 0x800A33CC and read
    // the next word — one whole extra/different string, caught by SBS at f190).
    text = c->mem_r32(0x800A33C8u + (uint32_t)(idx * 12) + 4u);
  }
  c->r[18] = text;

  // (4) per-character loop — counts re-read from the node each iteration, exactly like gen.
  if (c->mem_r8(node + 9u) == 0u) {
    return;
  }
  c->r[17] = 0;
  if ((int32_t)c->mem_r8(node + 8u) <= 0) {
    return;
  }
  c->r[21] = tomba2::render::PacketPool::kCursorPage; // gen's live pool-base register (callees spill it)
  c->r[19] = node;                                    // cmd cursor (node + i*4; cmd read at +0xC0)
  for (;;) {
    const uint32_t ch = c->mem_r8(c->r[18]);
    if (ch == 0u) {
      break;
    }
    // glyph UV fill into the packet at the pool tail (space → v0=-1 → skip).
    c->r[4] = c->r[18];
    const tomba2::render::PacketPool pool(*c);
    c->r[5] = pool.cursor();
    tomba::guest::dispatchJalToReturn(*c, 0x80039E80u, 0x8003A05Cu);
    if ((int32_t)c->r[2] != -1) {
      const uint32_t cmd = c->mem_r32(c->r[19] + 192u);
      c->r[16] = pool.allocate(40u); // this glyph's packet
      c->r[4] = cmd + 24u;
      tomba::guest::dispatchJalToReturn(*c, 0x80084660u, 0x8003A080u); // SetRotMatrix(cmd+0x18)
      c->r[4] = c->mem_r32(c->r[19] + 192u) + 24u;
      tomba::guest::dispatchJalToReturn(*c, 0x80084690u, 0x8003A08Cu); // SetTransMatrix(cmd+0x18)
      c->r[4] = c->r[16];
      c->r[5] = sp + 16u;
      c->r[6] = sp + 80u;
      tomba::guest::dispatchJalToReturn(*c, 0x8003F7D8u, 0x8003A09Cu); // project the template
      const int32_t otzm1 = (int32_t)c->r[2] - 1;
      if (otzm1 >= 0) {
        const uint32_t pk = c->r[16];
        auto sx = [&](uint32_t off) {
          return (uint32_t)c->mem_r16(pk + off);
        };
        // X over the draw window: [0, 320) at 4:3.
        const std::uint32_t xs[4] = {sx(8), sx(16), sx(24), sx(32)};
        const bool xok =
            tomba2::horizontal_cull::forDrawWindow(c).keepsX(xs, 4, tomba2::horizontal_cull::Domain::Packed16);
        const bool yok = sx(10) < 240u || sx(18) < 240u || sx(26) < 240u || sx(34) < 240u;
        if (xok && yok) {
          c->mem_w8(pk + 7u, 45u);                                            // code 0x2D (textured raw)
          c->mem_w16(pk + 22u, 31u);                                          // tpage half
          c->mem_w16(pk + 14u, c->mem_r8(node + 3u) == 2u ? 31871u : 32255u); // clut
          tomba2::render::OrderingTable::active(*c).link(pk, 9u, (uint32_t)otzm1);
        }
      }
    }
    c->r[19] += 4u;
    c->r[17] += 1u;
    c->r[18] += 1u;
    if ((int32_t)c->r[17] >= (int32_t)c->mem_r8(node + 9u)) {
      break;
    }
    if ((int32_t)c->r[17] >= (int32_t)c->mem_r8(node + 8u)) {
      break;
    }
  }
}

void ov_textLabelEmit(Core *c) {
  rend(c)->textLabelEmit();
}
} // namespace

void Render::textLabelEmit() {
  textLabelBody(mCore);
}

void text_label_install() {
  tomba::native::declareOverride(0x80039F4Cu, "ov_textLabelEmit", ov_textLabelEmit);
}
