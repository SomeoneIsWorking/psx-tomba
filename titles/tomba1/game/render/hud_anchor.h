// The HUD pass 0x8004E468 draws each element at an authored 4:3 place; under a widening an element wholly on one side
// of the picture's centre column moves to the drawn picture's edge on that side through the host packets, one across
// it stays, guest RAM unchanged.
#pragma once

#include "mesh_draw.h"

namespace tomba1 {

// False when any entry was refused.
bool installHudAnchor(const InstallOverride &install);

} // namespace tomba1
