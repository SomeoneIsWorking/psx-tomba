#include "render/palette_cycles.h"

#include "core.h"
#include "frame/image_residency.h"

#include <algorithm>
#include <array>

namespace tomba::render {
namespace {

constexpr std::array kScripted{
    ScriptedPaletteCycles{.image = "A00", .code = 0x80115B88u, .descriptors = 0x80147D84u, .count = 12u},
    ScriptedPaletteCycles{.image = "A00", .code = 0x8013C0BCu, .descriptors = 0x8014ABDCu, .count = 1u},
    ScriptedPaletteCycles{.image = "A0D", .code = 0x8010B288u, .descriptors = 0x801175E0u, .count = 1u},
    kSopPaletteCycles,
};

// A00 FUN_80131840: an object stepping five rows through its own phase words (+0x42, +0x48/+0x4A/+0x4C), its last
// frame the one state 0x11 at 0x800BF9E0 uploads.
constexpr std::uint32_t kA00RowStepper = 0x80131840u;
constexpr std::array kA00SteppedRows{
    psx::present::PaletteCycle{.x = 1008, .y = 231, .frames = 0x8014A3A4u, .count = 4u},
    psx::present::PaletteCycle{.x = 1008, .y = 229, .frames = 0x8014A3A4u, .count = 4u},
    psx::present::PaletteCycle{.x = 1008, .y = 222, .frames = 0x8014A444u, .count = 4u},
    psx::present::PaletteCycle{.x = 800, .y = 190, .frames = 0x8014A4C4u, .count = 7u},
    psx::present::PaletteCycle{.x = 816, .y = 205, .frames = 0x8014A5A4u, .count = 7u},
};

constexpr std::uint32_t kMaxScriptPairs = 128u;

// The frames a descriptor's script uploads from here on: up to its loop marker, then the loop it rewinds to.
std::uint32_t scriptFrames(Core &core, std::uint32_t script) {
  std::uint32_t frames = 0;
  std::uint32_t at = script;
  for (std::uint32_t pair = 0; pair < kMaxScriptPairs && core.mem_r8(at) != ScriptedPaletteCycles::kLoop; ++pair) {
    frames = std::max<std::uint32_t>(frames, core.mem_r8(at) + 1u);
    at += 2u;
  }
  if (core.mem_r8(at) != ScriptedPaletteCycles::kLoop) {
    return frames;
  }
  for (std::uint32_t loop = at - core.mem_r8(at + 1u); loop < at; loop += 2u) {
    frames = std::max<std::uint32_t>(frames, core.mem_r8(loop) + 1u);
  }
  return frames;
}

} // namespace

void PaletteCycles::declare(Core &core, std::vector<psx::present::PaletteCycle> &out) const {
  for (const ScriptedPaletteCycles &cycles : kScripted) {
    if (!imageHolds(core, cycles.code, cycles.image)) {
      continue;
    }
    for (std::uint32_t i = 0; i < cycles.count; ++i) {
      const std::uint32_t descriptor = cycles.descriptors + i * ScriptedPaletteCycles::kStride;
      out.push_back({.x = core.mem_r16(descriptor + ScriptedPaletteCycles::kX),
                     .y = core.mem_r16(descriptor + ScriptedPaletteCycles::kY),
                     .frames = core.mem_r32(descriptor + ScriptedPaletteCycles::kImage),
                     .count = scriptFrames(core, core.mem_r32(descriptor + ScriptedPaletteCycles::kScript))});
    }
  }
  if (imageHolds(core, kA00RowStepper, "A00")) {
    out.insert(out.end(), kA00SteppedRows.begin(), kA00SteppedRows.end());
  }
}

} // namespace tomba::render
