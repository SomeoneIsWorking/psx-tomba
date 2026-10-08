// class Panel — the 2D UI panel emitters Tomba! 2 owns natively.
//
//   FUN_8004FFB4  fillQuad: one textured FT4 fill from a {x,y,w,h} rect (panel_fill.cpp). The 9-slice
//                 builder FUN_8005019C calls it five times through the guest dispatch.
//   FUN_8007FCC8  pushDialogBackdrop: the flat rectangle behind the message box (dialog_backdrop.cpp).
#pragma once
#include <cstdint>
class Core;

class Panel {
public:
  // Registers FUN_8004FFB4. Idempotent.
  static void install();

  // FUN_8007FCC8: one GP0 0x60 packet linked into the near or far OT bucket. `mode` bit 0x80 selects
  // the far bucket; any of bits 0x7F set means "no fill" (black) instead of the panel blue.
  static void pushDialogBackdrop(Core *c, int16_t x, int16_t y, int16_t w, int16_t h, uint32_t mode);

  // FUN_8004FFB4: GP0 0x2C/0x2E textured quad with attr-driven CLUT, flat shading and
  // semi-transparency, plus a 5-way UV table on the uvIndex argument.
  static void fillQuad(Core *c);
};
