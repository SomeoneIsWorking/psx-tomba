// game/ui/dialog_glyphs.cpp — DialogGlyphs, guest FUN_8007CC00 and FUN_8007CDD4. See dialog_glyphs.h.
#include "ui/dialog_glyphs.h"

#include "core.h"
#include "core/overrides/native_override_catalog.h"
#include "guest_abi.h"
#include "render/crop_layers.h"
#include "render/guest_ordering_table.h"
#include "render/libgpu_draw_mode.h"

#include <iterator>

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

// The prompt: a 16x16 sprite 16 pixels in from the rect's right and bottom edges.
constexpr std::uint32_t kPromptFrameBytes = 0x28u;
constexpr GuestFrameSpill kPromptSpills[3] = {{31 /*ra*/, 0x20}, {17, 0x1C}, {16, 0x18}};
constexpr std::uint16_t kPromptClut = 0x7C3F;
constexpr std::uint16_t kPromptSize = 16;
constexpr std::uint8_t kPromptV = 0x78;
// 0x800BFE4C: the button layout; 0 and 1 pick the prompt's column, any other leaves u as the pool holds it.
constexpr std::uint32_t kButtonLayout = 0x800BFE4Cu;
constexpr std::uint8_t kLayoutColumns[2] = {0x00, 0x10};

} // namespace

void DialogGlyphs::emit(Core *c) {
  const std::uint32_t node = c->r[4];
  ::GuestFrame<kFrameBytes, 2> frame(c, kSpills);

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
    packet += kSpriteBytes;
  }

  pool.setCursor(packet + kModeBytes);
  c->mem_w32(c->r[29] + kWindowArg, 0);
  c->r[2] = render::setDrawMode(
      *c, {.packet = packet, .drawOnDisplay = 0, .dither = 0, .texturePage = kTexturePage, .window = 0});
  render::OrderingTable::active(*c).link({.packet = packet, .words = kModeWords, .bucket = kBucket});
}

void DialogGlyphs::prompt(Core *c) {
  const std::uint32_t rect = c->r[4];
  ::GuestFrame<kPromptFrameBytes, 3> frame(c, kPromptSpills);

  const render::PacketPool pool(*c);
  const std::uint32_t packet = pool.cursor();
  c->mem_w8(packet + 7u, kSprite);
  c->mem_w16(packet + 0xEu, kPromptClut);
  c->mem_w16(packet + 8u, static_cast<std::uint16_t>(c->mem_r16(rect) + c->mem_r16(rect + 4u) - kPromptSize));
  c->mem_w16(packet + 0xAu, static_cast<std::uint16_t>(c->mem_r16(rect + 2u) + c->mem_r16(rect + 6u) - kPromptSize));
  const std::uint8_t layout = c->mem_r8(kButtonLayout);
  if (layout < std::size(kLayoutColumns)) {
    c->mem_w8(packet + 0xCu, kLayoutColumns[layout]);
  }
  c->mem_w8(packet + 0xDu, kPromptV);
  c->mem_w16(packet + 0x10u, kPromptSize);
  c->mem_w16(packet + 0x12u, kPromptSize);
  render::OrderingTable::active(*c).link({.packet = packet, .words = kSpriteWords, .bucket = kBucket});

  const std::uint32_t mode = packet + kSpriteBytes;
  pool.setCursor(mode + kModeBytes);
  c->mem_w32(c->r[29] + kWindowArg, 0);
  render::setDrawMode(*c, {.packet = mode, .drawOnDisplay = 0, .dither = 0, .texturePage = kTexturePage, .window = 0});
  render::OrderingTable::active(*c).link({.packet = mode, .words = kModeWords, .bucket = kBucket});
  c->r[2] = c->mem_r32(mode);
}

void DialogGlyphs::registerOverrides(native::NativeOverrideCatalog &natives) {
  natives.declare(kEmit, "DialogGlyphs::emit", render::uiDraw<&DialogGlyphs::emit>);
  natives.declare(kPrompt, "DialogGlyphs::prompt", render::uiDraw<&DialogGlyphs::prompt>);
}

} // namespace tomba
