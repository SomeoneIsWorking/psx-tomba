// game/scene/title_menu.h — the title menu's items drawn as a vertical list (DEMO FUN_80106824).
#pragma once

#include <cstdint>

class Core;

namespace tomba::native {
class NativeOverrideCatalog;
} // namespace tomba::native

namespace tomba {

class TitleMenu {
public:
  // FUN_80106824(menu, item): menu 0 is New Game / Load Game, 1 is Start Game / Options.
  static constexpr std::uint32_t kDrawItems = 0x80106824u;
  // The sub-machines' edges that move the selection.
  static constexpr std::uint16_t kPrevious = 0x0010u; // Up
  static constexpr std::uint16_t kNext = 0x0040u;     // Down

  static void registerOverrides(native::NativeOverrideCatalog &natives);

private:
  static void drawItems(Core *c);
};

} // namespace tomba
