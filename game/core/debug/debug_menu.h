// debug_menu.h — Tomba! 2's debug menu: area warp, items and flags, the sound test, the camera pin and the
// diagnostics, each applied through the frame driver's DevControl at a frame boundary.
#pragma once

#include "title_debug_menu.h"

namespace tomba {

class DebugMenu final : public psx::game::TitleDebugMenu {
public:
  DebugMenu();
};

} // namespace tomba
