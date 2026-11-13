// class Panel — implementation of the override registration. See panel.h.
#include "panel.h"
#include "core.h"
#include "core/overrides/native_override_catalog.h"
#include "render/crop_layers.h"

namespace tomba {

void Panel::install(native::NativeOverrideCatalog &natives) {
  natives.declare(kBuild, "Panel::build", render::uiDraw<Panel::drawPanel>);
  natives.declare(kFill, "Panel::fillQuad", render::uiDraw<Panel::fillQuad>);
}

} // namespace tomba
