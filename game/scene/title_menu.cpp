// game/scene/title_menu.cpp — TitleMenu::drawItems, the title menu's items as a column.
#include "title_menu.h"

#include "core.h"
#include "core/overrides/native_override_catalog.h"
#include "entry/session.h"
#include "guest_abi.h"
#include "guest_call.h"
#include "render/guest_ordering_table.h"
#include "ui/text_fonts.h"

#include <algorithm>
#include <array>

namespace tomba {

namespace {

constexpr GuestFrameSpill kSpills[] = {
    {31, 0x40}, {23, 0x3C}, {22, 0x38}, {21, 0x34}, {20, 0x30}, {19, 0x2C}, {18, 0x28}, {17, 0x24}, {16, 0x20}};
constexpr std::uint32_t kAttributes = 0x10u; // u8 colour, u8 bucket, u16 CLUT
constexpr std::uint32_t kPlacement = 0x18u;  // u16 x, u16 y, u16 width, u16 height (0: the definition's)

constexpr std::uint32_t kQuads = 0x8007E1B8u;
constexpr std::uint32_t kCursor = 0x8007E998u;
constexpr std::uint32_t kReturnAfterCursor = 0x801068B4u;
constexpr std::uint32_t kReturnAfterItem = 0x80106904u;
constexpr std::uint32_t kSpriteDefs = 0x80017334u; // u32[] -> s16 definition index
constexpr std::uint32_t kModelTable = 0x800ECF58u;
constexpr std::uint32_t kBucket = 4u;
constexpr std::uint8_t kDimmed = 0x50u; // the unselected item's modulation; 0 draws raw

// Sprite definitions by TitleMenu::Item.
constexpr std::array<std::uint32_t, TitleMenu::kItems> kLabels{0x8Eu, 0x8Fu, 0x91u};
// The column is centred on retail's row across and ends on it, clear of the copyright line below.
constexpr int kCentreX = 160;
constexpr int kBottomY = 180;
constexpr int kPitch = 18;
// The sword's centre sits this far left of the column, as retail puts it left of New Game.
constexpr int kCursorGap = 10;
constexpr int kCursorRaise = 4;

std::uint32_t definition(Core *c, std::uint32_t label) {
  return c->mem_r32(kSpriteDefs + (label * 4u));
}

// A label's width: its definition's one piece.
int labelWidth(Core *c, std::uint32_t label) {
  const std::uint32_t table = c->mem_r32(kModelTable);
  const auto index = static_cast<std::int16_t>(c->mem_r16(definition(c, label)));
  const std::uint32_t entry = static_cast<std::uint32_t>(index) * 4u;
  const std::uint32_t pieces = table + c->mem_r16(table + entry + 2u);
  return c->mem_r8(pieces + 0x0Au);
}

} // namespace

// FUN_80106824(menu r4, item r5)
void TitleMenu::drawItems(Core *c) {
  GuestFrame<0x48, 9> frame(c, kSpills);
  const std::uint32_t sp = c->r[29];
  const std::size_t selected = std::min<std::size_t>(c->r[5], kItems - 1u);
  int widest = 0;
  for (const std::uint32_t label : kLabels) {
    widest = std::max(widest, labelWidth(c, label));
  }
  const int left = kCentreX - (widest / 2);
  const int top = kBottomY - ((static_cast<int>(kLabels.size()) - 1) * kPitch);

  c->r[4] = static_cast<std::uint32_t>(left - kCursorGap);
  c->r[5] = static_cast<std::uint32_t>(top + (static_cast<int>(selected) * kPitch) - kCursorRaise);
  c->r[6] = kBucket;
  c->r[31] = kReturnAfterCursor;
  psx::cpu::callGuestNow(*c, __func__, kCursor);

  for (std::size_t row = 0; row < kLabels.size(); ++row) {
    c->mem_w8(sp + kAttributes, row == selected ? 0u : kDimmed);
    c->mem_w8(sp + kAttributes + 1u, kBucket);
    c->mem_w16(sp + kAttributes + 2u, 0u);
    c->mem_w16(sp + kPlacement, static_cast<std::uint16_t>(left + (labelWidth(c, kLabels[row]) / 2)));
    c->mem_w16(sp + kPlacement + 2u, static_cast<std::uint16_t>(top + (static_cast<int>(row) * kPitch)));
    c->mem_w16(sp + kPlacement + 4u, 0u);
    c->mem_w16(sp + kPlacement + 6u, 0u);
    c->r[4] = sp + kPlacement;
    c->r[5] = definition(c, kLabels[row]);
    c->r[6] = c->mem_r32(kModelTable);
    c->r[7] = sp + kAttributes;
    c->r[31] = kReturnAfterItem;
    // The label's one piece is the quad linked at the pool's cursor.
    const std::uint32_t packet = render::PacketPool(*c).cursor();
    psx::cpu::callGuestNow(*c, __func__, kQuads);
    sessionOf(c).cropLayers.glyph(packet, text_fonts::kLabels);
  }
}

void TitleMenu::registerOverrides(native::NativeOverrideCatalog &natives) {
  natives.declareOverlay("DEMO", kDrawItems, "TitleMenu::drawItems", &TitleMenu::drawItems);
}

} // namespace tomba
