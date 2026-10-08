// The horizontal window this port draws into.
#pragma once

struct Core;

namespace tomba2::wide_window {

// The horizontal window the guest's own routines are written against. Retail composes 320 px wide
// and every stock right-edge cull tests against that, whatever this port presents into.
inline constexpr int kGuestWidth = 320;

// Guest screen columns [left, right) that reach the picture.
struct Window {
  int left = 0;
  int right = kGuestWidth;
};

// The record path's canvas, [-M, 320 + M); the wide engine's [0, render width) on the native path.
// [0, 320) at 4:3, on the oracle and on both SBS legs.
Window drawWindow(Core *core);

// Columns the record path's canvas adds each side of the displayed buffer; 0 at 4:3 and on the oracle.
int marginColumns(Core *core);

} // namespace tomba2::wide_window
