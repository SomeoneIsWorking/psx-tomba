// The selector's picture of Tomba! 2: its title-screen illustration, read from the disc's texture groups.
#pragma once

#include "art_scene.h"

#include <optional>

namespace psx::cd {
class Disc;
} // namespace psx::cd

namespace tomba {

// Texture group 2 drawn as the title screen draws its picture (tomba2-decomp docs/notes/texture-groups.md); nullopt
// when the disc does not hold it.
std::optional<psx::present::ArtScene> titleArt(psx::cd::Disc &disc);

} // namespace tomba
