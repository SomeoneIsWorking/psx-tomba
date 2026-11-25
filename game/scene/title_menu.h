// game/scene/title_menu.h — the title menu: one vertical list of New Game, Load Game and Options (DEMO FUN_80106824).
#pragma once

#include <cstdint>

class Core;

namespace tomba::native {
class NativeOverrideCatalog;
} // namespace tomba::native

namespace tomba {

class TitleMenu {
public:
  // FUN_80106824(menu, item) draws the list with `item` selected. Retail draws two lists (menu 0 New Game / Load
  // Game, menu 1 Start Game / Options); the port has one, so `menu` is unused.
  static constexpr std::uint32_t kDrawItems = 0x80106824u;
  // The list's items, top to bottom, as the title's s2 keeps the selection in sm[0x68].
  enum Item : std::uint8_t { kNewGame, kLoadGame, kOptions, kItems };
  // The sub-machines' edges that move the selection.
  static constexpr std::uint16_t kPrevious = 0x0010u; // Up
  static constexpr std::uint16_t kNext = 0x0040u;     // Down

  static void registerOverrides(native::NativeOverrideCatalog &natives);

private:
  static void drawItems(Core *c);
};

} // namespace tomba
