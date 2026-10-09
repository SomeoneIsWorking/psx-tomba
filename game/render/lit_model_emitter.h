// game/render/lit_model_emitter.h — the point-lit GT3/GT4 model emitters of the A01, A05-A08 overlays.
//
// One guest body, copied into each overlay at its own address (A05 0x8013544C/0x8013590C, A07
// 0x8012CDF4/0x8012D2B4, A08 0x80129BAC/0x8012A06C; only local branch targets differ). A01's copy
// (0x801316A8/0x80131BB0) reads two more flag bits, A06's (0x8013C0D8/0x8013C5B4) one. Each takes (a0 = record list, a1
// = OT base, a2 = count, a3 = fixed light intensity or 0) and returns the address past the list in v0. Per record: RTPT
// (and RTPS for the 4th corner), backface and screen cull, an OT bucket from the record's depth mode, then one DPCS per
// corner whose IR0 is twice the corner's distance to the light at 0x1F800160 (Trig::vecLen), capped at 0x4000, unless
// a3 gives it.
//
// The screen cull is the guest's per-corner unsigned test with the X bound taken from the draw window
// (horizontal_visibility_cull.h), so at 4:3 the packets and the scratchpad are the guest's.
#pragma once

#include "model_packet.h"

#include <cstdint>

class Core;

namespace tomba2::horizontal_cull {
class Visibility;
}

namespace tomba2::render {

class LitModelEmitter {
public:
  // `hideFlag` hides the record while 0x1F80009C is set (GT3 after staging its depths, GT4 before): A01
  // bit 6, A06 bit 2. A01's bit 3 adds 0x80 to the depth before it is compressed.
  struct FlagBits {
    std::uint32_t hideFlag;
    bool deepen;
  };
  static constexpr FlagBits kRetailFlags{0u, false};
  static constexpr FlagBits kA01Flags{kHideFlag, true};
  static constexpr FlagBits kA06Flags{4u, false};

  // Guest register arguments in, v0 = past the list out; the guest's stack frame and scratchpad
  // staging are reproduced.
  static void gt3(Core &core, const horizontal_cull::Visibility &visible, FlagBits flags);
  static void gt4(Core &core, const horizontal_cull::Visibility &visible, FlagBits flags);
  // The body over any memory; `call.sp` is the sp inside the guest frame. Returns the address past the list.
  static std::uint32_t emit(const EmitMemory &memory,
                            const ListCall &call,
                            const horizontal_cull::Visibility &visible,
                            FlagBits flags,
                            bool quad);
  static void registerOverrides();
  // The renders of the saved calls, at every copy's entries.
  static void registerStateRenders(Core &core);
};

} // namespace tomba2::render
