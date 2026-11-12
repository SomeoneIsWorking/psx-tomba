#include "wide_window.h"

#include "core.h"
#include "game.h"

namespace tomba::wide_window {

psx::present::CullWindow drawWindow(Core *core) {
  return psx::present::recordCanvasColumns(core->game->guestDisplay.plan(), kGuestWidth);
}

int marginColumns(Core *core) {
  // Latched by the record present from the configured aspect; the previous present's value.
  return core->game->guestDisplay.plan().presentationHorizontalMargin;
}

} // namespace tomba::wide_window
