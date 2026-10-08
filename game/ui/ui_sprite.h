// game/ui/ui_sprite.h — thin entry points onto the shared 2D sprite emitter (FUN_8007E1B8).
// See ui_sprite.cpp for the RE, the stack-frame requirement, and why def 152 is named for what it
// does rather than what it means.
#pragma once
class Core;

class UiSprite {
public:
  static void drawFromTable(Core *c);   // FUN_8007E8DC(x, y, attr, defIndex)
  static void drawFixedDef152(Core *c); // FUN_8007E998(x, y, attr) — defIndex pinned to 152

  // compose (FUN_8007E6DC): the emitter the two above feed. Walks a definition's PIECES, emitting one
  // textured-sprite packet each through a scratchpad template, then a draw-mode packet carrying the
  // definition's tpage. See ui_sprite_compose.cpp.
  static void compose(Core *c);
};
