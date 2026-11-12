// The area overlays' screen-space sprite-group drawers as native overrides, culled at the record canvas instead of
// the retail 321x241 literals. Each is bound when its overlay's content lands.
#pragma once

#include "mesh_draw.h"

#include <cstdint>

namespace tomba1 {

// Binds the drawers of the overlay whose content identity is `content`; false when any entry was refused.
bool installOverlaySpriteGroups(const InstallOverride &install, std::uint64_t content);

} // namespace tomba1
