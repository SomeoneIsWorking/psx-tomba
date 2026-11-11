#include "render/palette_cycles.h"

#include "core.h"
#include "frame/image_residency.h"

#include <algorithm>
#include <array>

namespace tomba::render {
namespace {

// Each the routine at `code`; A04's and A06's pick one of two descriptor tables by a bit of 0x800BFE56.
constexpr std::array kScripted{
    ScriptedPaletteCycles{.image = "A00", .code = 0x80115B88u, .descriptors = 0x80147D84u, .count = 12u},
    ScriptedPaletteCycles{.image = "A00", .code = 0x8013C0BCu, .descriptors = 0x8014ABDCu, .count = 1u},
    ScriptedPaletteCycles{.image = "A01", .code = 0x801112C0u, .descriptors = 0x801352CCu, .count = 6u},
    ScriptedPaletteCycles{.image = "A01", .code = 0x8012D6B0u, .descriptors = 0x80137B3Cu, .count = 1u},
    ScriptedPaletteCycles{.image = "A02", .code = 0x8011321Cu, .descriptors = 0x80129074u, .count = 9u},
    ScriptedPaletteCycles{.image = "A04", .code = 0x80123B90u, .descriptors = 0x80141584u, .count = 1u},
    ScriptedPaletteCycles{.image = "A04", .code = 0x80116884u, .descriptors = 0x8014097Cu, .count = 1u},
    ScriptedPaletteCycles{.image = "A04", .code = 0x80116884u, .descriptors = 0x80140BACu, .count = 2u},
    ScriptedPaletteCycles{.image = "A06", .code = 0x801271E4u, .descriptors = 0x80147A30u, .count = 1u},
    ScriptedPaletteCycles{.image = "A06", .code = 0x80115BC8u, .descriptors = 0x80142A5Cu, .count = 1u},
    ScriptedPaletteCycles{.image = "A06", .code = 0x80115BC8u, .descriptors = 0x80142F50u, .count = 5u},
    ScriptedPaletteCycles{.image = "A07", .code = 0x80113C20u, .descriptors = 0x80132B14u, .count = 5u},
    ScriptedPaletteCycles{.image = "A08", .code = 0x801152F0u, .descriptors = 0x80142EA8u, .count = 25u},
    ScriptedPaletteCycles{.image = "A08", .code = 0x8012579Cu, .descriptors = 0x8014579Cu, .count = 1u},
    ScriptedPaletteCycles{.image = "A09", .code = 0x8010A060u, .descriptors = 0x8010E0DCu, .count = 9u},
    ScriptedPaletteCycles{.image = "A09", .code = 0x8010B3A8u, .descriptors = 0x8010E260u, .count = 1u},
    ScriptedPaletteCycles{.image = "A0A", .code = 0x8010B9ACu, .descriptors = 0x8011937Cu, .count = 5u},
    ScriptedPaletteCycles{.image = "A0D", .code = 0x8010B288u, .descriptors = 0x801175E0u, .count = 1u},
    ScriptedPaletteCycles{.image = "A0E", .code = 0x8010BEB0u, .descriptors = 0x801181ECu, .count = 1u},
    ScriptedPaletteCycles{.image = "A0F", .code = 0x8010DF5Cu, .descriptors = 0x8011AE60u, .count = 12u},
    kSopPaletteCycles,
};

// A row an object steps through its own phase word, from a fixed table of frames.
struct SteppedRow {
  std::string_view image;
  std::uint32_t code = 0;
  psx::present::PaletteCycle row;
};

// A00 FUN_80131840 steps five rows by +0x42 and +0x48/+0x4A/+0x4C, its last frame the one state 0x11 at 0x800BF9E0
// uploads; A00 FUN_8012C910 steps (880, 213) through three frames every fourth tick by +0x42; A06 FUN_80117CF4 and A08
// FUN_80127C58 step one row every fourth tick through the frame order {0,1,2,3,2,1} at 0x80144E08 and 0x80145A64. A06's
// row is y 208 or 192 by a bit of 0x800BFE56.
constexpr std::array kSteppedRows{
    SteppedRow{"A00", 0x80131840u, {.x = 1008, .y = 231, .frames = 0x8014A3A4u, .count = 4u}},
    SteppedRow{"A00", 0x80131840u, {.x = 1008, .y = 229, .frames = 0x8014A3A4u, .count = 4u}},
    SteppedRow{"A00", 0x80131840u, {.x = 1008, .y = 222, .frames = 0x8014A444u, .count = 4u}},
    SteppedRow{"A00", 0x80131840u, {.x = 800, .y = 190, .frames = 0x8014A4C4u, .count = 7u}},
    SteppedRow{"A00", 0x80131840u, {.x = 816, .y = 205, .frames = 0x8014A5A4u, .count = 7u}},
    SteppedRow{"A00", 0x8012C910u, {.x = 880, .y = 213, .frames = 0x80149EE4u, .count = 3u}},
    SteppedRow{"A06", 0x80117CF4u, {.x = 1008, .y = 208, .frames = 0x80144D88u, .count = 4u}},
    SteppedRow{"A06", 0x80117CF4u, {.x = 1008, .y = 192, .frames = 0x80144D88u, .count = 4u}},
    SteppedRow{"A08", 0x80127C58u, {.x = 1008, .y = 231, .frames = 0x801459E4u, .count = 4u}},
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
  for (const SteppedRow &stepped : kSteppedRows) {
    if (imageHolds(core, stepped.code, stepped.image)) {
      out.push_back(stepped.row);
    }
  }
}

} // namespace tomba::render
