// Tomba! 2's screen-edge cull: every culling owner compares each corner against a literal 320 and 240
// (the quad submitter 0x8003B320, sltiu 0x140 at 0x8003B438); none reads the DRAWENV it publishes.
#pragma once

#include "screen_cull.h"

struct Core;

namespace tomba::horizontal_cull {

inline constexpr int kGuestFrameWidth = 320;
// Widening moves OFX at unchanged OFY and H (0x800509B4: SetGeomOffset(160, 120), SetGeomScreen(350)).
inline constexpr int kGuestFrameHeight = 240;

inline constexpr int kTriangleCorners = 3;
inline constexpr int kQuadCorners = 4;

// The cull for the window this port draws into.
psx::present::ScreenCull forDrawWindow(::Core *core);

} // namespace tomba::horizontal_cull
