// game/render/horizontal_visibility_cull.cpp — the cull's draw window, read from tomba2::wide_window.
#include "horizontal_visibility_cull.h"

#include "core.h"
#include "wide_window.h"

namespace tomba2::horizontal_cull {

Visibility forDrawWindow(::Core *core) {
  const tomba2::wide_window::Window window = tomba2::wide_window::drawWindow(core);
  return Visibility(window.left, window.right);
}

} // namespace tomba2::horizontal_cull
