// game/render/sway_model_emitter.h — A01's flag-driven scenery emitters: vertex sway, UV scroll and depth cue.
//
// A01's scenery walker FUN_80132358 calls two GT3/GT4 pairs besides the lit one (lit_model_emitter.h).
// Each takes (a0 = record list, a1 = OT base, a2 = count) and returns the address past the list in v0;
// depth is the flag byte's low 2 bits (1 farthest, 2 nearest, else AVSZ) over the SZ values staged at
// 0x1F800084. The flag byte's other bits:
//   - sway pair (0x80130838 / 0x80130D9C): bit 6 hides the record while 0x1F80009C is set; GT3 bit 3
//     sways each corner's SX by rcos(phase + x) >> 9 (toward +X when VZ is odd) and SY by
//     rsin(phase + z) >> 9 (phase s16 at 0x80139004) and puts it 0x80 buckets deeper, otherwise bit 4
//     scrolls the UVs by the bytes at 0x801388EC/0x801388EE. The GT4 never sways.
//   - cue pair (0x8012F8D8 / 0x8013000C): after the bucket, GT3 bit 3 sways as above (0x96 deeper);
//     else bit 6 hides; else each corner is depth-cued by DPCS with IR0 = SZ / 4, plus the top 10 bits of
//     the scratchpad LCG at 0x1F800080 when bit 2 is set. GT4 bit 7 instead cues only the corners whose
//     bits 3..6 are set and skips the scroll. Bit 4 then scrolls the UVs.
// The screen cull goes through horizontal_visibility_cull.h, so at 4:3 the packets are the guest's.
#pragma once

#include "emit_memory.h"
#include "list_job.h"

#include <cstdint>

class Core;

namespace tomba2::horizontal_cull {
class Visibility;
}

namespace tomba2::render {

class SwayModelEmitter {
public:
  enum class Kind : std::uint32_t { SwayGt3, ScrollGt4, CueGt3, CueGt4 };

  // The guest calls: the frame, the registers in, the end of the list in v0, the call saved under the open object.
  static void swayGt3(Core &core, const horizontal_cull::Visibility &visible);
  static void scrollGt4(Core &core, const horizontal_cull::Visibility &visible);
  static void cueGt3(Core &core, const horizontal_cull::Visibility &visible);
  static void cueGt4(Core &core, const horizontal_cull::Visibility &visible);
  // The body over any memory; `call.sp` is the sp inside the guest frame. Returns the address past the list.
  static std::uint32_t
  emit(const EmitMemory &memory, const ListCall &call, const horizontal_cull::Visibility &visible, Kind kind);
  static void registerOverrides();
  // The renders of the saved calls, at each of the four entries.
  static void registerStateRenders(Core &core);
};

} // namespace tomba2::render
