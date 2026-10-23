// game/ui/panel_fill.cpp — Panel::fillQuad, guest FUN_8004FFB4.
// Builds one textured quad (GP0 0x2C, or 0x2E semi) and links it into the ordering table.
#include "core.h"
#include "render/guest_ordering_table.h"
#include "ui/panel.h"

namespace tomba {

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

// A rebuild, not a transcription; equivalence is an A/B dump diff, not port_check's static store-site count.
void Panel::fill(const psx::present::EmitMemory &memory, const render::OrderingTable &ot, const FillArgs &args) {
  const uint32_t rect = args.rect;
  const uint16_t attr = args.attr;
  const uint32_t packet = tomba::render::PacketPool(memory).allocate(PACKET_BYTES);

  memory.mem_w8(packet + 7, GP0_QUAD);
  if (attr & ATTR_SEMI) {
    memory.mem_w8(packet + 7, GP0_QUAD_SEMI);
  }

  const uint16_t clut =
      (uint16_t)((((attr & ATTR_CLUT_ROW) + CLUT_ROW_BASE) << 6) | ((attr & ATTR_CLUT_ALT) ? 63u : 62u));
  memory.mem_w16(packet + 14, clut);

  if (attr & ATTR_FLAT_SHADE) {
    memory.mem_w8(packet + 6, FLAT_SHADE_LVL);
    memory.mem_w8(packet + 5, FLAT_SHADE_LVL);
    memory.mem_w8(packet + 4, FLAT_SHADE_LVL);
  } else {
    memory.mem_w8(packet + 7, (uint8_t)(memory.mem_r8(packet + 7) | RAW_TEXTURE_BIT));
  }
  memory.mem_w16(packet + 22, TPAGE);

  // four corners of the rect, in the order the guest writes them
  const uint16_t x = memory.mem_r16(rect + 0), y = memory.mem_r16(rect + 2);
  const uint16_t w = memory.mem_r16(rect + 4), h = memory.mem_r16(rect + 6);
  memory.mem_w16(packet + 8, x);
  memory.mem_w16(packet + 10, y);
  memory.mem_w16(packet + 16, (uint16_t)(x + w));
  memory.mem_w16(packet + 18, y);
  memory.mem_w16(packet + 24, x);
  memory.mem_w16(packet + 26, (uint16_t)(y + h));
  memory.mem_w16(packet + 32, (uint16_t)(x + w));
  memory.mem_w16(packet + 34, (uint16_t)(y + h));

  if (args.uvIndex < 5) {
    const UvSet &uv = kUvSets[args.uvIndex];
    memory.mem_w8(packet + 13, uv.vTop); // v0.v and v1.v share the top edge
    memory.mem_w8(packet + 21, uv.vTop);
    memory.mem_w8(packet + 12, uv.uLeft);
    memory.mem_w8(packet + 20, uv.uRight);
    memory.mem_w8(packet + 28, uv.uLeft);
    memory.mem_w8(packet + 29, uv.vBottom);
    memory.mem_w8(packet + 36, uv.uRight);
    memory.mem_w8(packet + 37, uv.vBottom);
  }

  ot.link({.packet = packet, .words = TAG_WORDS, .bucket = args.bucket});
}

// FUN_8004FFB4(rect r4 {s16 x,y,w,h}, uvIndex r5, attr r6, otBucket r7)
void Panel::fillQuad(Core *c) {
  fill(*c,
       render::OrderingTable::active(*c),
       {.rect = c->r[4], .uvIndex = c->r[5], .attr = static_cast<uint16_t>(c->r[6]), .bucket = c->r[7]});
}

} // namespace tomba
