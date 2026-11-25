// game/render/quad_rtpt_submit.h — FUN_8003B054, a quad record's corner rotation, and FUN_8003B320, a POLY_FT4
// template's four world corners projected through the GTE view and a copy linked into the active OT.
//
// FUN_8003B320's callers (MAIN 0x8003B704's beam, 0x8003C048, 0x8003F898; the A0A 0x80114E64 and A0F 0x801175C0
// hanging strips) build the template and corners on their stack each frame; A01's chain FUN_80128B08 builds its GT4
// template in the scratchpad for its copy FUN_80133F38.
#pragma once

#include "emit_memory.h"
#include "list_job.h"

#include <array>
#include <cstdint>

struct Core;

namespace tomba::native {
class NativeOverrideCatalog;
} // namespace tomba::native

namespace psx::present {
class ScreenCull;
}

namespace tomba {

class QuadRtptSubmit {
public:
  static constexpr std::uint32_t kSubmit = 0x8003B320u;
  // A01's chain links (FUN_80128B08 -> FUN_80133F38): a POLY_GT4 template, no flat side, returning 1 when dropped.
  static constexpr std::uint32_t kA01ChainLink = 0x80133F38u;

  // The template a submitter fills.
  struct Layout {
    std::uint32_t templateWords = 0;
    std::array<std::uint32_t, 4> screen{}; // each corner's SXY offset
    // A quad whose NCLIP magnitude is no more than this drops; negative: none does.
    std::int32_t flatArea = -1;
  };
  static constexpr Layout kFt4{.templateWords = 10u, .screen = {8u, 0x10u, 0x18u, 0x20u}, .flatArea = -1};
  static constexpr Layout kGt4{.templateWords = 13u, .screen = {8u, 0x14u, 0x20u, 0x2Cu}, .flatArea = 7};

  // FUN_8003B054(dst = a0, src = a1, cornerIndex = a2): src's four corner fields into dst, rotated by cornerIndex
  // (0..3); 1..3 shrink one or both bytes of each by 1.
  static void rotateQuadCorners(Core *c);

  // FUN_8003B320 over `memory`: `call.list` the `layout` template, `call.a1` the corners (V0-V2 for RTPT, then V3 for
  // RTPS), `call.count` the bucket bias, `call.sp` the guest stack it scratches 16 bytes below. The template takes the
  // projected SXYs; AVSZ4 plus the bias, compressed, is the bucket; the quad drops on a GTE overflow, an out-of-range
  // bucket, a flat quad or when no corner lies in the draw window. Whether it linked.
  static bool submit(const psx::present::EmitMemory &memory,
                     const render::ListCall &call,
                     const psx::present::ScreenCull &visible,
                     const Layout &layout);

  static void registerOverrides(native::NativeOverrideCatalog &natives);
};

} // namespace tomba
