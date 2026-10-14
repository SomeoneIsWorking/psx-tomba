// game/render/horizontal_visibility_cull.cpp — the cull's draw window, read from tomba::wide_window.
#include "horizontal_visibility_cull.h"

#include "core.h"
#include "wide_window.h"

namespace tomba::horizontal_cull {

Visibility forDrawWindow(::Core *core) {
  return Visibility(tomba::wide_window::drawWindow(core));
}

} // namespace tomba::horizontal_cull
