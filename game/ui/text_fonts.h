// game/ui/text_fonts.h — Tomba! 2's text fonts (tomba2-decomp `docs/notes/text-fonts.md`) as psxport draws them
// from outline fonts (plan 0207).
//
// All three lie on the 4bpp page at (960, 256), DR_TPAGE 0x1F. The 8x16 font from v 40 and the 8x8 font from v 0 are
// 32 cells a row, cell = ASCII - 0x20 (Font::glyphEmit through drawText and drawTextSmall; DialogGlyphs::emit, whose
// codes are ASCII - 0x20; Font::iconGlyphEmit's full-width codes on the 8x8 font). Each starts its fourth row with
// the circle, cross, triangle and square buttons. The 16x16 icons from v 120, 16 a row, start with the same four
// (DialogGlyphs::emit's wide codes 0x60 on, DialogGlyphs::prompt). The buttons are ringed in several colours, so under
// a PlayStation pad they keep their bitmaps. Glyphs are CLUT index 1 with index 5 at their edges, over a shadow in
// index 6 one texel right and below; a cell the tables leave out draws its bitmap. The letters are serifed (I, 1, l),
// so the face keeps its own proportions at each glyph's height. The letter fonts' symbol cells (the token table's
// 0x64-0x98, and ASCII 0x2A drawn as the reference mark) come from Noto Sans JP; a cell filled in a second colour
// keeps its bitmap, as do the diagonal arrows and the nested squares, which the face has no shape for.
#pragma once

#include "host_input.h"
#include "outline_text.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace tomba::text_fonts {

using psx::present::OutlineFace;
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

// ASCII 0x21 to 0x7E: 0x5C is drawn as the yen sign, 0x2A as the reference mark; 0x5F is blank in the 8x16 font.
inline constexpr std::size_t kLetters = 0x7E - 0x20;

struct SymbolCell {
  int cell = 0;
  char32_t codepoint = 0;
};

// The symbol cells both letter fonts draw in one colour.
inline constexpr std::array<SymbolCell, 38> kSymbols{{
    {0x6C, U'、'}, {0x6D, U'。'}, {0x6E, U'，'}, {0x6F, U'ー'}, {0x70, U'‐'}, {0x71, U'＼'}, {0x72, U'〜'},
    {0x73, U'…'},  {0x74, U'‥'},  {0x75, U'‘'},  {0x76, U'’'},  {0x77, U'“'}, {0x78, U'”'},  {0x79, U'「'},
    {0x7A, U'」'}, {0x7B, U'☆'},  {0x7C, U'★'},  {0x7F, U'○'},  {0x80, U'●'}, {0x81, U'◎'},  {0x83, U'◆'},
    {0x85, U'■'},  {0x87, U'▲'},  {0x89, U'▼'},  {0x8A, U'♪'},  {0x8B, U'♂'}, {0x8C, U'♀'},  {0x8D, U'℃'},
    {0x8E, U'※'},  {0x8F, U'『'}, {0x90, U'』'}, {0x91, U'♦'},  {0x92, U'♣'}, {0x93, U'♥'},  {0x94, U'♠'},
    {0x95, U'⁉'},  {0x96, U'‼'},  {0x97, U'©'},
}};
// The cells the 8x8 font draws hollow and the 8x16 font fills in a second colour: the arrows (white arrows, as drawn),
// ◇ □ △ ▽.
inline constexpr std::array<SymbolCell, 8> kHollowSymbols{{
    {0x64, U'⇨'},
    {0x65, U'⇦'},
    {0x66, U'⇧'},
    {0x67, U'⇩'},
    {0x82, U'◇'},
    {0x84, U'□'},
    {0x86, U'△'},
    {0x88, U'▽'},
}};

template <std::size_t Hollow>
constexpr std::array<OutlineGlyph, kLetters + kButtons.size() + kSymbols.size() + Hollow>
letterGlyphs(const OutlineGrid &grid) {
  std::array<OutlineGlyph, kLetters + kButtons.size() + kSymbols.size() + Hollow> glyphs{};
  std::size_t at = 0;
  for (char32_t ascii = 0x21; ascii <= 0x7E; ascii++) {
    const int cell = static_cast<int>(ascii) - 0x20;
    glyphs[at++] = ascii == U'*'    ? grid.glyph(cell, U'※', OutlineFace::Symbol)
                   : ascii == U'\\' ? grid.glyph(cell, U'¥')
                                    : grid.glyph(cell, ascii);
  }
  for (std::size_t b = 0; b < kButtons.size(); b++) {
    glyphs[at++] = grid.button(kLetterButtons + static_cast<int>(b), kButtons[b], true);
  }
  for (const SymbolCell &symbol : kSymbols) {
    glyphs[at++] = grid.glyph(symbol.cell, symbol.codepoint, OutlineFace::Symbol);
  }
  for (std::size_t h = 0; h < Hollow; h++) {
    glyphs[at++] = grid.glyph(kHollowSymbols.at(h).cell, kHollowSymbols.at(h).codepoint, OutlineFace::Symbol);
  }
  return glyphs;
}

constexpr std::array<OutlineGlyph, kButtons.size()> iconGlyphs() {
  std::array<OutlineGlyph, kButtons.size()> glyphs{};
  for (std::size_t b = 0; b < kButtons.size(); b++) {
    glyphs[b] = kIconGrid.button(kIconButtons + static_cast<int>(b), kButtons[b], true);
  }
  return glyphs;
}

inline constexpr auto kTextGlyphs = letterGlyphs<0>(kTextGrid);
inline constexpr auto kSmallGlyphs = letterGlyphs<kHollowSymbols.size()>(kSmallGrid);
inline constexpr auto kIconGlyphs = iconGlyphs();

inline constexpr std::array<int, 1> kShadowIndices{6};

constexpr psx::present::OutlineFont font(std::span<const OutlineGlyph> glyphs) {
  return {.page = kPage,
          .glyphs = glyphs,
          .textFace = "FiraSans-Regular.ttf",
          .symbolFace = "NotoSansJP-Symbols.ttf",
          .proportional = true,
          .shadowIndices = kShadowIndices,
          .shadowX = 1,
          .shadowY = 1};
}

// By Font.
inline constexpr std::array<psx::present::OutlineFont, 3> kFonts{
    font(kTextGlyphs), font(kSmallGlyphs), font(kIconGlyphs)};

} // namespace tomba::text_fonts
