// game/render/letterbox_bars.h — FUN_80026864, the cutscene letterbox: slot type 1 of the 8-slot array
// at 0x80100400 (Array8Dispatch, handler table 0x8009D314).
#pragma once

#include <cstdint>

class Core;

namespace tomba2::render {

class LetterboxBars {
public:
  // Slot fields.
  static constexpr std::uint32_t kState = 4u;  // u8: 0 idle, 1 grow, 2 hold, 3 shrink
  static constexpr std::uint32_t kHeight = 8u; // s16 bar height in lines
  // Guest globals.
  static constexpr std::uint32_t kEffectsArmed = 0x1F80019Au; // u8, the slots draw only at 2
  static constexpr std::uint32_t kCue = 0x1F800137u;          // u8: 1 opens the bars, 0 or 2 closes them
  static constexpr std::int16_t kFullHeight = 12;
  static constexpr std::int16_t kGuestHeight = 224;

  // FUN_80026864(slot=a0): steps the bar height and draws a black 0x60 rect at the top and bottom.
  // The rects span the guest's 320 columns plus `margin` each side, so the wide canvas is covered.
  static void tick(Core &core, std::uint32_t slot, int margin);
  static void registerOverrides();
};

} // namespace tomba2::render
