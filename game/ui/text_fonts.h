// game/ui/text_fonts.h — Tomba! 2's text fonts (tomba2-decomp `docs/notes/text-fonts.md`) as psxport draws them
// from outline fonts (plan 0207).
//
// All three lie on the 4bpp page at (960, 256), DR_TPAGE 0x1F. The 8x16 font from v 40 and the 8x8 font from v 0 are
// 32 cells a row, cell = ASCII - 0x20 (Font::glyphEmit through drawText and drawTextSmall; DialogGlyphs::emit, whose
// codes are ASCII - 0x20; Font::iconGlyphEmit's full-width codes on the 8x8 font). Each starts its fourth row with
// the circle, cross, triangle and square buttons. The 16x16 icons from v 120, 16 a row, start with the same four
// (DialogGlyphs::emit's wide codes 0x60 on, DialogGlyphs::prompt). Glyphs are CLUT index 1 with index 5 at their
// edges, over a shadow in index 6 one texel right and below; a cell the tables leave out draws its bitmap. The letters
// are serifed (I, 1, l), so the face keeps its own proportions at each glyph's height.
#pragma once

#include "host_input.h"
#include "outline_text.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace tomba::text_fonts {

using psx::present::OutlineGlyph;
using psx::present::OutlineGrid;

enum Font : std::uint8_t {
  kText,  // 8x16
  kSmall, // 8x8
  kIcons, // 16x16
};

inline constexpr psx::gpu::TexPage kPage{.x = 960, .y = 256, .depth = psx::gpu::TexDepth::Indexed4};

inline constexpr OutlineGrid kTextGrid{.v = 40, .cellWidth = 8, .cellHeight = 16, .columns = 32};
inline constexpr OutlineGrid kSmallGrid{.cellWidth = 8, .cellHeight = 8, .columns = 32};
inline constexpr OutlineGrid kIconGrid{.v = 120, .cellWidth = 16, .cellHeight = 16, .columns = 16};

// The first cell of each font's button row.
inline constexpr int kLetterButtons = 0x60;
inline constexpr int kIconButtons = 0;

inline constexpr std::array<std::uint16_t, 4> kButtons{
    psx::input::kButtonCircle, psx::input::kButtonCross, psx::input::kButtonTriangle, psx::input::kButtonSquare};

// ASCII 0x21 to 0x7E: 0x5C is drawn as the yen sign, 0x2A as the reference mark, which keeps its bitmap; 0x5F is
// blank in the 8x16 font.
inline constexpr std::size_t kLetters = 0x7E - 0x21;

constexpr std::array<OutlineGlyph, kLetters + kButtons.size()> letterGlyphs(const OutlineGrid &grid) {
  std::array<OutlineGlyph, kLetters + kButtons.size()> glyphs{};
  std::size_t at = 0;
  for (char32_t ascii = 0x21; ascii <= 0x7E; ascii++) {
    if (ascii == U'*') {
      continue;
    }
    glyphs[at++] = grid.glyph(static_cast<int>(ascii) - 0x20, ascii == U'\\' ? U'¥' : ascii);
  }
  for (std::size_t b = 0; b < kButtons.size(); b++) {
    glyphs[at++] = grid.button(kLetterButtons + static_cast<int>(b), kButtons[b]);
  }
  return glyphs;
}

constexpr std::array<OutlineGlyph, kButtons.size()> iconGlyphs() {
  std::array<OutlineGlyph, kButtons.size()> glyphs{};
  for (std::size_t b = 0; b < kButtons.size(); b++) {
    glyphs[b] = kIconGrid.button(kIconButtons + static_cast<int>(b), kButtons[b]);
  }
  return glyphs;
}

inline constexpr auto kTextGlyphs = letterGlyphs(kTextGrid);
inline constexpr auto kSmallGlyphs = letterGlyphs(kSmallGrid);
inline constexpr auto kIconGlyphs = iconGlyphs();

inline constexpr std::array<int, 1> kShadowIndices{6};

constexpr psx::present::OutlineFont font(std::span<const OutlineGlyph> glyphs) {
  return {.page = kPage,
          .glyphs = glyphs,
          .textFace = "FiraSans-Regular.ttf",
          .proportional = true,
          .shadowIndices = kShadowIndices,
          .shadowX = 1,
          .shadowY = 1};
}

// By Font.
inline constexpr std::array<psx::present::OutlineFont, 3> kFonts{
    font(kTextGlyphs), font(kSmallGlyphs), font(kIconGlyphs)};

} // namespace tomba::text_fonts
