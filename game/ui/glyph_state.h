// game/ui/glyph_state.h — the state of one text glyph, and its render.
//
// A glyph is a variable-size textured sprite Font::glyphEmit links into the string's bucket; the draw mode packet
// that gives it its texture page follows the whole string. As an object it is its character's address; its state
// is the sprite's four command words and where it links, and the render moves its position.
#pragma once

#include "core.h"

#include <cstdint>

namespace tomba2::ui {

class GlyphState {
public:
  // The texture page the string's draw mode packet sets (glyphEmit's SetDrawMode call takes it from a3).
  static constexpr std::uint32_t kTexturePage = 31u;

  // Saves the glyph drawn under the innermost open object: its command words and the bucket of the active OT.
  static void save(Core &core, const std::uint32_t (&command)[4], std::uint32_t bucket);
  // Installs the render at producer `producer`.
  static void registerRender(Core &core, std::uint32_t producer);
};

} // namespace tomba2::ui
