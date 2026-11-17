// game/ui/subscreen_hint.h — the field subscreen's hint box (FUN_80033FF4): its frame, the parchment strip and
// the open tab's hint, which the Items tab replaces with a selected suit's lent effects.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

class Core;

namespace tomba::native {
class NativeOverrideCatalog;
} // namespace tomba::native

namespace tomba {

// Small-font text into the hint box, from a string buffer in the caller's guest frame.
class HintWriter {
public:
  // The hint's own text: x 0x28, y 0xBF, colour 0x12, four buckets in front of the box.
  inline static constexpr int kLeft = 0x28;
  inline static constexpr int kTop = 0xBF;
  inline static constexpr int kRight = 0x130;
  inline static constexpr int kGlyph = 8;
  inline static constexpr int kLineHeight = 9;
  inline static constexpr std::uint32_t kColour = 0x12u;
  inline static constexpr std::size_t kMaxText = 39;

  // Where the text goes: a guest buffer for the string and the ordering-table bucket.
  struct Target {
    std::uint32_t buffer;
    std::uint32_t bucket;
  };
  struct Place {
    int x;
    int y;
  };

  HintWriter(Core &core, Target target) : core_(core), target_(target) {}

  void text(Place place, std::uint32_t colour, std::string_view text) const;

private:
  Core &core_;
  Target target_;
};

class SubscreenHint {
public:
  static void registerOverrides(native::NativeOverrideCatalog &natives);

private:
  // FUN_80033FF4(state, bucket).
  static void draw(Core *c);
};

} // namespace tomba
