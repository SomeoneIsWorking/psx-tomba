// The selector's picture of Tomba! 1: its title screen's background and logo, without the menu text.
#pragma once

#include "art_scene.h"

#include <optional>

namespace psx::cd {
class Disc;
} // namespace psx::cd

namespace tomba1 {

// The title overlay's background and logo sprite lists over their files (tomba1-decomp
// docs/notes/title-screen.md); nullopt when the disc does not hold them.
std::optional<psx::present::ArtScene> titleArt(psx::cd::Disc &disc);

} // namespace tomba1
