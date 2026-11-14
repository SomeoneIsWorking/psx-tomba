// The loading card 0x80023CE0 (area change and New Game, through 0x8001CA84): a checkerboard of 48-pixel tiles in two
// frames under the logo, which the guest draws over [0, 336). Under a widening the tiles continue the board over the
// canvas as host packets, guest RAM unchanged.
#pragma once

#include "mesh_draw.h"

namespace tomba1 {

// False when the entry was refused.
bool installLoadingCard(const InstallOverride &install);

} // namespace tomba1
