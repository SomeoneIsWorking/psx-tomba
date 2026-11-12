// game/render/quad_rtpt_submit.h — FUN_8003B054, a quad record's corner rotation, and FUN_8003B320, a POLY_FT4
// template's four world corners projected through the GTE view and a copy linked into the active OT.
//
// FUN_8003B320's callers (MAIN 0x8003B704's beam, 0x8003C048, 0x8003F898; the A0A 0x80114E64 and A0F 0x801175C0
// hanging strips) build the template and corners on their stack each frame.
#pragma once

#include "emit_memory.h"
#include "list_job.h"

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

  // FUN_8003B054(dst = a0, src = a1, cornerIndex = a2): src's four corner fields into dst, rotated by cornerIndex
  // (0..3); 1..3 shrink one or both bytes of each by 1.
  static void rotateQuadCorners(Core *c);

  // FUN_8003B320 over `memory`: `call.list` the 10-word template, `call.a1` the corners (V0-V2 for RTPT, then V3 for
  // RTPS), `call.count` the bucket bias, `call.sp` the guest stack it scratches 16 bytes below. The template takes the
  // projected SXYs; AVSZ4 plus the bias, compressed, is the bucket; the quad drops on a GTE overflow, an out-of-range
  // bucket or when no corner lies in the draw window.
  static void
  submit(const psx::present::EmitMemory &memory, const render::ListCall &call, const psx::present::ScreenCull &visible);

  static void registerOverrides(native::NativeOverrideCatalog &natives);
};

} // namespace tomba
