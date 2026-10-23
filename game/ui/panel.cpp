// class Panel — implementation of the override registration. See panel.h.
#include "panel.h"
#include "core.h"
#include "core/overrides/native_override_catalog.h"

namespace tomba {

void Panel::install(native::NativeOverrideCatalog &natives) {
  natives.declare(kBuild, "Panel::build", Panel::drawPanel);
  natives.declare(kFill, "Panel::fillQuad", Panel::fillQuad);
}

} // namespace tomba
