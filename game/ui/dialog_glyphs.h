// game/ui/dialog_glyphs.h — the dialog box's text, FUN_8007CC00, and its continue prompt, FUN_8007CDD4.
// FUN_8007C940 lays the text into the glyph list at 0x800ECB88; FUN_8007CC00 draws it as 8x16 sprites into bucket 2.
#pragma once

#include <cstdint>
class Core;

namespace tomba::native {
class NativeOverrideCatalog;
} // namespace tomba::native

namespace tomba {

class DialogGlyphs {
public:
  static constexpr std::uint32_t kEmit = 0x8007CC00u;
  static constexpr std::uint32_t kPrompt = 0x8007CDD4u;

  // FUN_8007CC00(node a0).
  static void emit(Core *c);
  // FUN_8007CDD4(rect a0 = node+0x54): the 16x16 prompt in the box's bottom-right corner, bucket 2; FUN_8007D594
  // calls it on the blinking half of the wait.
  static void prompt(Core *c);

  static void registerOverrides(native::NativeOverrideCatalog &natives);
};

} // namespace tomba
