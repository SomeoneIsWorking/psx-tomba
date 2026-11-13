// The entity sprite drawers as native overrides: each part of an entity's current frame, or its model quad, is a
// raw-textured quad around its anchor, culled at the record canvas instead of the retail 320x224 literals.
#pragma once

#include "mesh_draw.h"

namespace tomba1 {

// False when any entry was refused.
bool installSpriteParts(const InstallOverride &install);

} // namespace tomba1
