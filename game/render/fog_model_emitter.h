// game/render/fog_model_emitter.h — the GT3/GT4 model emitters that fog the record's colours by depth: A04
// 0x8013F418/0x8013F6E4 and A05 0x8013AC90/0x8013AF0C (the scenery walker's branch at 0x8013623C, taken while the byte
// 0x800E7EAA is below 12 and bit 0x20 of its record's +0x5E6 is clear).
//
// Per record: RTPT (and RTPS for the 4th corner), GTE overflow, back face and screen cull, then DPCT of the record's
// colours toward the far colour by IR0 = clamp(AVSZ * 4 - fog bias, 0, 0x1000) (DPCS for the 4th); the bias is 0x200
// in A04 and 0x40 in A05. Flag bit 2 keeps the record's own colours; in A04, without it, bit 3 hides the record while
// the signed byte 0x1F800207 is 15..17 and bit 5 while 0x1F80009C is set. A zero flag byte sorts by AVSZ, otherwise by
// the farthest SZ (bit 1 clear) or the nearest, /4, into buckets (4, 0x7FF). Each takes (a0 = record list, a1 = OT
// base, a2 = count), returns the address past the list in v0, and stages nothing in the scratchpad. The screen cull
// is the draw window's (horizontal_visibility_cull.h).
#pragma once

#include "emit_memory.h"
#include "list_job.h"

#include <cstdint>

class Core;

namespace tomba::native {
class NativeOverrideCatalog;
} // namespace tomba::native

namespace psx::present {
class ScreenCull;
}

namespace tomba::render {

class FogModelEmitter {
public:
  struct Variant {
    std::int32_t fogBias;
    bool hides; // flag bits 3 and 5 hide the record
  };
  static constexpr Variant kA04{0x200, true};
  static constexpr Variant kA05{0x40, false};
  // The body over any memory; returns the address past the list.
  static std::uint32_t emit(const psx::present::EmitMemory &memory,
                            const ListCall &call,
                            const psx::present::ScreenCull &visible,
                            const Variant &variant,
                            bool quad);
  static void registerOverrides(native::NativeOverrideCatalog &natives);
};

} // namespace tomba::render
