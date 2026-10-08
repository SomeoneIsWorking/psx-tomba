// class Panel — implementation of the override registration. See panel.h.
#include "panel.h"
#include "core.h"
#include "core/overrides/native_override_catalog.h"

void Panel::install() {
  static bool done = false;
  if (done) {
    return;
  }
  done = true;
  tomba::native::declareOverride(0x8004FFB4u, "Panel::fillQuad", Panel::fillQuad);
}
