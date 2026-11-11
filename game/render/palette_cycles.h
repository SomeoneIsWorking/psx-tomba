// palette_cycles.h — the CLUT rows Tomba! 2's palette cycle owners upload, declared to the replay so an in-between
// cross-fades a row between the frames of its two ticks.
#pragma once

#include "palette_cycle.h"

#include <cstdint>
#include <string_view>
#include <vector>

class Core;

namespace tomba::render {

// The scripted cycle routine the overlays carry (A00 FUN_80115B88, A0D FUN_8010B288, SOP FUN_8010C6A0, ...): per
// descriptor a countdown runs out, the script's {frame, ticks} pair uploads `image + frame * 32` to (x, y) with
// LoadImage, and {0xFF, n} rewinds the script n bytes.
struct ScriptedPaletteCycles {
  static constexpr std::uint32_t kStride = 12u; // {u8 *script, u16 *image, u16 x, u16 y}
  static constexpr std::uint32_t kScript = 0u;
  static constexpr std::uint32_t kImage = 4u;
  static constexpr std::uint32_t kX = 8u;
  static constexpr std::uint32_t kY = 10u;
  static constexpr std::uint8_t kLoop = 0xFFu;
  static constexpr std::uint32_t kFrameBytes = 32u; // 16 colours

  std::string_view image;
  std::uint32_t code = 0; // the routine, resident while `image` holds it
  std::uint32_t descriptors = 0;
  std::uint32_t count = 0;
};

inline constexpr ScriptedPaletteCycles kSopPaletteCycles{
    .image = "SOP", .code = 0x8010C26Cu, .descriptors = 0x8010D2FCu, .count = 3u};

class PaletteCycles final : public psx::present::PaletteCycles {
public:
  void declare(Core &core, std::vector<psx::present::PaletteCycle> &out) const override;
};

} // namespace tomba::render
