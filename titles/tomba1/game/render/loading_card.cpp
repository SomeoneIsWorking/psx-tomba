#include "loading_card.h"

#include "core.h"
#include "gp0_primitive_decode.h"
#include "guest_widescreen_projection.h"
#include "r3000.h"
#include "resumable_guest_call.h"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <lucent/log.h>

namespace tomba1 {
namespace {

using psx::cpu::kRa;

constexpr std::uint32_t kCardEntry = 0x80023CE0u;
// The card's packet cursor; the guest writes the logo, the tiles row by row, then the draw mode.
constexpr std::uint32_t kCardCursor = 0x8009C8A8u;
// The flip 0x80016940 draws the table from its last entry.
constexpr std::uint32_t kOrderingTable = 0x1F8001E0u;
constexpr std::uint32_t kWalkStart = 0xC9Cu;

// FUN_80024008's logo packet, then FUN_80023ED0's tile sprites and the closing SetDrawMode.
constexpr std::uint32_t kLogoBytes = 0x28u;
constexpr std::uint32_t kTileBytes = 0x14u;
constexpr std::uint32_t kDrawModeBytes = 0x0Cu;
// The tile stride and the board the guest loops over (7 columns, 6 rows).
constexpr int kTileSize = 0x30;
constexpr int kGuestColumns = 7;
constexpr int kRows = 6;
// The display 0x80016C4C builds.
constexpr int kPictureWidth = 320;
// A sprite's GP0 words after its tag: command and colour, position, texture and CLUT, size.
constexpr std::size_t kSpriteWords = 4;
constexpr std::size_t kPositionWord = 1;

int floorDiv(int value, int divisor) {
  return value >= 0 ? value / divisor : -((-value + divisor - 1) / divisor);
}

// 0x80023CE0 (frame, logo phase): the guest's board, then the tiles of the columns it never reaches.
void drawCard(Core *core) {
  Core &c = *core;
  const std::uint32_t first = c.mem_r32(kCardCursor);
  psx::cpu::callOriginalResumingToReturn(c, "LoadingCard::draw", kCardEntry, c.r[kRa]);
  const psx::present::HorizontalMargins edges = psx::present::frameWindows(c).drawn;
  if (edges.total() == 0) {
    return;
  }
  const std::uint32_t tiles = first + kLogoBytes;
  const std::uint32_t drawMode = tiles + kGuestColumns * kRows * kTileBytes;
  if (c.mem_r32(kCardCursor) != drawMode + kDrawModeBytes) {
    lucent::error("tomba1-loading-card",
                  "the card wrote 0x{:08X}..0x{:08X}, not its logo, {} tiles and a draw mode",
                  first,
                  c.mem_r32(kCardCursor),
                  kGuestColumns * kRows);
    std::abort();
  }
  // The guest's tiles at (0, 0) and (1, 0) hold the board's two frames.
  std::array<std::array<std::uint32_t, kSpriteWords>, 2> frames{};
  for (std::uint32_t parity = 0; parity < 2u; ++parity) {
    for (std::uint32_t word = 0; word < kSpriteWords; ++word) {
      frames[parity][word] = c.mem_r32(tiles + parity * kTileBytes + 4u + word * 4u);
    }
  }
  const std::uint32_t root = c.mem_r32(kOrderingTable) + kWalkStart;
  const int firstColumn = floorDiv(-edges.left, kTileSize);
  const int lastColumn = floorDiv(kPictureWidth + edges.right + kTileSize - 1, kTileSize);
  for (int row = 0; row < kRows; ++row) {
    for (int column = firstColumn; column < lastColumn; ++column) {
      if (column >= 0 && column < kGuestColumns) {
        continue;
      }
      std::array<std::uint32_t, kSpriteWords> words = frames[static_cast<std::uint32_t>(column + row) & 1u];
      words[kPositionWord] = (static_cast<std::uint32_t>(row * kTileSize) << 16u) |
                             (static_cast<std::uint32_t>(column * kTileSize) & 0xFFFFu);
      c.hostPackets.insertAfter(c, root, drawMode, {.words = words, .places = {}}, 0u);
    }
  }
}

} // namespace

bool installLoadingCard(const InstallOverride &install) {
  return install(kCardEntry, "LoadingCard::draw", drawCard);
}

} // namespace tomba1
