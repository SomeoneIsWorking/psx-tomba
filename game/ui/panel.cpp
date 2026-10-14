// class Panel — implementation of the override registration. See panel.h.
#include "panel.h"
#include "core.h"
#include "core/overrides/native_override_catalog.h"

namespace tomba {

void Panel::install(native::NativeOverrideCatalog &natives) {
  natives.declare(0x8004FFB4u, "Panel::fillQuad", Panel::fillQuad);
}

} // namespace tomba
