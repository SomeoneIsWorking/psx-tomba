#include "wide_window.h"

#include "core.h"
#include "game.h"

namespace tomba::wide_window {

Window drawWindow(Core *core) {
  const int margin = marginColumns(core);
  return {-margin, kGuestWidth + margin};
}

int marginColumns(Core *core) {
  // Latched by the record present from the configured aspect; the previous present's value.
  return core->game->guestDisplay.plan().presentationHorizontalMargin;
}

} // namespace tomba::wide_window
