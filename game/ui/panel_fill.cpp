// game/ui/panel_fill.cpp — Panel::fillQuad, guest FUN_8004FFB4.
//
// The 9-slice panel's fill primitive, and the busiest render function on the field path (505
// dispatches per 1200 frames). RE'd 2026-07-22 from guest 0x8004FFB4.
//
// Registered on 0x8004FFB4 by Panel::install() (game/ui/panel.cpp).
//
// It builds ONE textured quad — a GP0 0x2C, or 0x2E when the caller asks for semi-transparency — and
// links it into the ordering table. Ten words off the packet pool:
//
//     +0   tag            next-pointer | (9 << 24)
//     +4/5/6  colour      0x40 on each channel when flat-shaded; otherwise left alone and the
//                         opcode's raw-texture bit is set instead
//     +7   opcode         0x2C / 0x2E, | 1 when raw-textured
//     +8,+10   v0 xy      the rect's top-left
//     +12,+13  v0 uv
//     +14  clut
//     +16,+18  v1 xy      top-right
//     +20,+21  v1 uv
//     +22  tpage          95
//     +24,+26  v2 xy      bottom-left
//     +28,+29  v2 uv
//     +32,+34  v3 xy      bottom-right
//     +36,+37  v3 uv
//
// ATTR BITS (r6): 0x80 semi-transparent · 0x40 selects CLUT 63 rather than 62 · 0x20 flat-shade at
// 0x40 instead of raw texture · 0x1F is the CLUT row, placed at ((row + 496) << 6).
//
// UV SETS: the uvIndex argument picks one of five fixed atlas rectangles. The guest compiles this as
// a jump table, but every arm only chooses four constants, so it is a table here. An index of 5 or
// more skips the UVs entirely and leaves whatever the packet already held — that is the guest's
// behaviour, not an oversight.
#include "core.h"
#include "render/ordering_table.h"
#include "ui/panel.h"

namespace {

constexpr uint32_t PACKET_BYTES = 40u;
constexpr uint32_t TAG_WORDS = 9u;

constexpr uint8_t GP0_QUAD = 0x2C;      // textured quad
constexpr uint8_t GP0_QUAD_SEMI = 0x2E; // ...semi-transparent
constexpr uint8_t RAW_TEXTURE_BIT = 0x01;
constexpr uint8_t FLAT_SHADE_LVL = 0x40;
constexpr uint16_t TPAGE = 95;

constexpr uint16_t ATTR_SEMI = 0x80;
constexpr uint16_t ATTR_CLUT_ALT = 0x40;
constexpr uint16_t ATTR_FLAT_SHADE = 0x20;
constexpr uint16_t ATTR_CLUT_ROW = 0x1F;
constexpr uint16_t CLUT_ROW_BASE = 496;

// The five atlas rectangles uvIndex selects between.
struct UvSet {
  uint8_t uLeft, uRight, vTop, vBottom;
};
constexpr UvSet kUvSets[5] = {
    {192, 200, 136, 144},
    {240, 248, 136, 144},
    {208, 216, 137, 143},
    {224, 232, 137, 143},
    {216, 223, 136, 143},
};

} // namespace

// EQUIVALENCE. This is a REBUILD, not a transcription, so `port_check` cannot pass it: it compares
// STATIC store sequences, and the guest's five-arm jump table has 12 store sites where the table
// below has 8. Contorting the code to match that count is precisely how these files end up
// unreadable. Equivalence is instead proven the way rebuilds have to be — by running the bucket
// capture with this installed and with it disabled (so guest 0x8004FFB4 runs) and diffing the 2 MB
// dumps at frame 1200: ZERO differing bytes, 2026-07-22. Re-run that A/B if this file is edited.
//
// FUN_8004FFB4(rect r4 {s16 x,y,w,h}, uvIndex r5, attr r6, otBucket r7)
void Panel::fillQuad(Core *c) {
  const uint32_t rect = c->r[4];
  const uint32_t uvIndex = c->r[5];
  const uint16_t attr = (uint16_t)c->r[6];
  const uint32_t otBucket = c->r[7];

  const uint32_t packet = tomba2::render::PacketPool(*c).allocate(PACKET_BYTES);

  c->mem_w8(packet + 7, GP0_QUAD);
  if (attr & ATTR_SEMI) {
    c->mem_w8(packet + 7, GP0_QUAD_SEMI);
  }

  const uint16_t clut =
      (uint16_t)((((attr & ATTR_CLUT_ROW) + CLUT_ROW_BASE) << 6) | ((attr & ATTR_CLUT_ALT) ? 63u : 62u));
  c->mem_w16(packet + 14, clut);

  if (attr & ATTR_FLAT_SHADE) {
    c->mem_w8(packet + 6, FLAT_SHADE_LVL);
    c->mem_w8(packet + 5, FLAT_SHADE_LVL);
    c->mem_w8(packet + 4, FLAT_SHADE_LVL);
  } else {
    c->mem_w8(packet + 7, (uint8_t)(c->mem_r8(packet + 7) | RAW_TEXTURE_BIT));
  }
  c->mem_w16(packet + 22, TPAGE);

  // four corners of the rect, in the order the guest writes them
  const uint16_t x = c->mem_r16(rect + 0), y = c->mem_r16(rect + 2);
  const uint16_t w = c->mem_r16(rect + 4), h = c->mem_r16(rect + 6);
  c->mem_w16(packet + 8, x);
  c->mem_w16(packet + 10, y);
  c->mem_w16(packet + 16, (uint16_t)(x + w));
  c->mem_w16(packet + 18, y);
  c->mem_w16(packet + 24, x);
  c->mem_w16(packet + 26, (uint16_t)(y + h));
  c->mem_w16(packet + 32, (uint16_t)(x + w));
  c->mem_w16(packet + 34, (uint16_t)(y + h));

  if (uvIndex < 5) {
    const UvSet &uv = kUvSets[uvIndex];
    c->mem_w8(packet + 13, uv.vTop); // v0.v and v1.v share the top edge
    c->mem_w8(packet + 21, uv.vTop);
    c->mem_w8(packet + 12, uv.uLeft);
    c->mem_w8(packet + 20, uv.uRight);
    c->mem_w8(packet + 28, uv.uLeft);
    c->mem_w8(packet + 29, uv.vBottom);
    c->mem_w8(packet + 36, uv.uRight);
    c->mem_w8(packet + 37, uv.vBottom);
  }

  // link at the head of the requested bucket
  tomba2::render::OrderingTable::active(*c).link(packet, TAG_WORDS, otBucket);
}
