// The horizontal window this port draws into.
#pragma once

#include "screen_cull.h"

struct Core;

namespace tomba::wide_window {

// Retail composes 320 px wide and every stock right-edge cull tests against that.
inline constexpr int kGuestWidth = 320;

// The record path's canvas, [-M, 320 + M); [0, 320) at 4:3.
psx::present::CullWindow drawWindow(Core *core);

// Columns the record path's canvas adds each side of the displayed buffer; 0 at 4:3.
int marginColumns(Core *core);

} // namespace tomba::wide_window
