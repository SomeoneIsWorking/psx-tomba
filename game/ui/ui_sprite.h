// game/ui/ui_sprite.h — the shared 2D sprite emitters (FUN_8007E1B8, FUN_8007E6DC) and their entry points.
// See ui_sprite.cpp for the RE, the stack-frame requirement, and why def 152 is named for what it
// does rather than what it means.
#pragma once
#include "state_producer.h"

#include <cstdint>
class Core;

namespace tomba::native {
class NativeOverrideCatalog;
} // namespace tomba::native

namespace tomba {

class UiSprite {
public:
  static constexpr std::uint32_t kQuads = 0x8007E1B8u;
  static constexpr std::uint32_t kCompose = 0x8007E6DCu;

  static void drawFromTable(Core *c);   // FUN_8007E8DC(x, y, attr, defIndex)
  static void drawFixedDef152(Core *c); // FUN_8007E998(x, y, attr) — defIndex pinned to 152

  // quads (FUN_8007E1B8): one textured quad per piece of a definition, mirrored as the attribute flags say.
  // See ui_sprite_quads.cpp.
  static void quads(Core *c);
  // compose (FUN_8007E6DC): walks a definition's PIECES, emitting one textured-sprite packet each through a
  // scratchpad template, then a draw-mode packet carrying the definition's tpage. See ui_sprite_compose.cpp.
  static void compose(Core *c);

  static void registerOverrides(native::NativeOverrideCatalog &natives);
  // Each call of either emitter is an object (its call site, n-th in the frame) saved as its anchor and
  // packets; the render moves the anchor.
  static void registerStateRenders(psx::present::StateProducerTable &renders);
};

} // namespace tomba
