// The HUD pass 0x8004E468 draws each element at an authored 4:3 anchor; under a widening each element's packets
// move to the drawn picture's edge on its anchor's side through the host packets, guest RAM unchanged.
#pragma once

#include "mesh_draw.h"

namespace tomba1 {

// False when any entry was refused.
bool installHudAnchor(const InstallOverride &install);

} // namespace tomba1
