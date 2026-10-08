#include "wide_window.h"

#include "core.h"
#include "game.h"
#include "gpu_vk.h"

namespace tomba2::wide_window {

Window drawWindow(Core *core) {
  if (gpu_vk_wide_engine(core)) {
    return {0, gpu_vk_wide_engine_w(core)};
  }
  const int margin = marginColumns(core);
  return {-margin, kGuestWidth + margin};
}

int marginColumns(Core *core) {
  // Latched by the record present from the configured aspect; the previous present's value.
  return core->game->guestDisplay.plan().presentationHorizontalMargin;
}

} // namespace tomba2::wide_window
