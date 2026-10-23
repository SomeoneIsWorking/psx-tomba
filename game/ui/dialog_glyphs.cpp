// game/ui/dialog_glyphs.cpp — DialogGlyphs, guest FUN_8007CC00. See dialog_glyphs.h.
#include "ui/dialog_glyphs.h"

#include "core.h"
#include "core/overrides/native_override_catalog.h"
#include "guest_abi.h"
#include "render/anchored_packets.h"
#include "render/guest_ordering_table.h"
#include "render/libgpu_draw_mode.h"

namespace tomba {
namespace {

constexpr std::uint32_t kGlyphs = 0x800ECB88u;
constexpr std::uint32_t kGlyphStride = 8u;
constexpr std::uint32_t kGlyphCount = 0x1F80017Eu; // s16

// The sprite template the body keeps on the scratchpad.
constexpr std::uint32_t kTemplateCode = 0x1F800007u;
constexpr std::uint32_t kTemplateClut = 0x1F80000Eu;
constexpr std::uint32_t kTemplateWidth = 0x1F800010u;
constexpr std::uint32_t kTemplateHeight = 0x1F800012u;

// Node fields.
constexpr std::uint32_t kVariant = 3u;
constexpr std::uint32_t kHighlightable = 0x47u;
constexpr std::uint32_t kRect = 0x54u; // s16 x, s16 y

constexpr std::uint8_t kSprite = 0x65;
constexpr std::uint16_t kGlyphWidth = 8;
constexpr std::uint16_t kGlyphHeight = 16;
constexpr std::uint8_t kWide = 0x80;
constexpr std::uint8_t kCharMask = 0x7F;
constexpr std::uint16_t kNoClutRow = 0xFF;
constexpr std::uint16_t kHighlightRow = 0x12;
constexpr std::uint16_t kHighlightClut = 0x7CBE;
constexpr std::uint16_t kClutRowBase = 0x1F0;

constexpr std::uint32_t kBucket = 2u;
constexpr std::uint32_t kSpriteBytes = 0x14u;
constexpr std::uint32_t kSpriteWords = 4u;
constexpr std::uint32_t kModeBytes = 0xCu;
constexpr std::uint32_t kModeWords = 2u;
constexpr std::uint32_t kTexturePage = 0x1Fu;

constexpr std::uint32_t kFrameBytes = 0x20u;
constexpr GuestFrameSpill kSpills[2] = {{31 /*ra*/, 0x1C}, {16, 0x18}};
constexpr std::uint32_t kWindowArg = 0x10u; // SetDrawMode's fifth argument, on the stack

} // namespace

void DialogGlyphs::emit(Core *c) {
  const std::uint32_t node = c->r[4];
  ::GuestFrame<kFrameBytes, 2> frame(c, kSpills);
  const psx::present::EmissionScope::Guard object(c->emission, kEmit, node, 0);
  render::AnchoredPackets drawn(c->mem_r32(node + kRect), kTexturePage);

  std::uint16_t row = kNoClutRow;
  if (c->mem_r8(node + kHighlightable) == 1 && c->mem_r8(node + kVariant) == 1) {
    row = kHighlightRow;
    c->mem_w16(kTemplateClut, kHighlightClut);
  }
  c->mem_w8(kTemplateCode, kSprite);
  c->mem_w16(kTemplateWidth, kGlyphWidth);
  c->mem_w16(kTemplateHeight, kGlyphHeight);

  const render::PacketPool pool(*c);
  std::uint32_t packet = pool.cursor();
  for (std::uint32_t index = 0; static_cast<std::int32_t>(index) < c->mem_r16s(kGlyphCount); ++index) {
    const std::uint32_t glyph = kGlyphs + index * kGlyphStride;
    const std::uint8_t character = c->mem_r8(glyph + 3u);
    const auto charRow = static_cast<std::uint16_t>(character & kCharMask);
    if (charRow != row && row != kHighlightRow) {
      row = charRow;
      c->mem_w16(kTemplateClut, static_cast<std::uint16_t>(((row + kClutRowBase) << 6) | 0x3Fu));
    }
    c->mem_w8(packet + 7u, c->mem_r8(kTemplateCode));
    c->mem_w16(packet + 0xEu, c->mem_r16(kTemplateClut));
    c->mem_w16(packet + 8u, c->mem_r16(glyph));
    c->mem_w16(packet + 0xAu, c->mem_r8(glyph + 2u));
    c->mem_w8(packet + 0xCu, c->mem_r8(glyph + 4u));
    c->mem_w8(packet + 0xDu, c->mem_r8(glyph + 6u));
    const std::uint16_t width = c->mem_r16(kTemplateWidth);
    c->mem_w16(packet + 0x10u, (character & kWide) != 0 ? static_cast<std::uint16_t>(width << 1) : width);
    c->mem_w16(packet + 0x12u, c->mem_r16(kTemplateHeight));
    render::OrderingTable::active(*c).link({.packet = packet, .words = kSpriteWords, .bucket = kBucket});
    const std::uint32_t command[kSpriteWords] = {
        c->mem_r32(packet + 4u), c->mem_r32(packet + 8u), c->mem_r32(packet + 0xCu), c->mem_r32(packet + 0x10u)};
    drawn.add(kBucket, command);
    packet += kSpriteBytes;
  }

  pool.setCursor(packet + kModeBytes);
  c->mem_w32(c->r[29] + kWindowArg, 0);
  c->r[2] = render::setDrawMode(
      *c, {.packet = packet, .drawOnDisplay = 0, .dither = 0, .texturePage = kTexturePage, .window = 0});
  render::OrderingTable::active(*c).link({.packet = packet, .words = kModeWords, .bucket = kBucket});
  drawn.save(*c);
}

void DialogGlyphs::registerOverrides(native::NativeOverrideCatalog &natives) {
  natives.declare(kEmit, "DialogGlyphs::emit", &DialogGlyphs::emit);
}

void DialogGlyphs::registerStateRenders(psx::present::StateProducerTable &renders) {
  renders.install(kEmit, render::AnchoredPackets::render());
}

} // namespace tomba
