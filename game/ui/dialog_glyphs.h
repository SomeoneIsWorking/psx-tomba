// game/ui/dialog_glyphs.h — the dialog box's text, FUN_8007CC00.
// FUN_8007C940 lays the text into the glyph list at 0x800ECB88; FUN_8007CC00 draws it as 8x16 sprites into bucket 2.
#pragma once
#include "state_producer.h"

#include <cstdint>
class Core;

namespace tomba::native {
class NativeOverrideCatalog;
} // namespace tomba::native

namespace tomba {

class DialogGlyphs {
public:
  static constexpr std::uint32_t kEmit = 0x8007CC00u;

  // FUN_8007CC00(node a0).
  static void emit(Core *c);

  static void registerOverrides(native::NativeOverrideCatalog &natives);
  // A box's glyphs are its object, saved as their packets anchored at the box's rect; the render moves the rect.
  static void registerStateRenders(psx::present::StateProducerTable &renders);
};

} // namespace tomba
