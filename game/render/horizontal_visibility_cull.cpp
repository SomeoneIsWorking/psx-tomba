#include "horizontal_visibility_cull.h"

#include "wide_window.h"

namespace tomba::horizontal_cull {

psx::present::ScreenCull forDrawWindow(::Core *core) {
  return {tomba::wide_window::drawWindow(core), kGuestFrameHeight};
}

} // namespace tomba::horizontal_cull
