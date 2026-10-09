// game/render/overlay_gt3gt4.h — PC-native bodies for the overlay model GT3/GT4 packet emitters:
// the A00 cluster (FUN_80146478/801465EC/801467BC) and A08's plain pair (FUN_80140FBC/801411D8). See
// overlay_gt3gt4.cpp for the RE trace and wiring rationale.
//
// A08's pair is A00's body plus a texture scroll: a record whose flag byte has bit 2 set gets the s16 at
// 0x80145A6E added to its packet's UV halfwords. Writes the guest's own GTE, OT and GP0 packet state
// 1:1 with the guest MIPS.
#pragma once
#include "emit_memory.h"
#include "list_job.h"

#include <cstdint>

struct Core;
class Game;

namespace tomba2::horizontal_cull {
class Visibility;
}

class OverlayGt3Gt4 {
public:
  // No texture scroll (A00).
  static constexpr std::uint32_t kNoScroll = 0u;
  // A08's scroll offset, s16.
  static constexpr std::uint32_t kA08Scroll = 0x80145A6Eu;

  // FUN_80146478(block=a0, ot_base=a1) -> first byte past the block's records, in v0.
  // The FIELD SUBMITTER's block dispatcher and the busiest single substrate-dispatch target in the
  // game (127,275 typed runtime address dispatch hits over 6000 frames of the seesaw-weight replay, 4x the runner-up).
  static void submitBlock(Core *c);

  // (rec=a0, ot_base=a1, count=a2) -> advanced rec ptr in v0. `uvScroll` names the overlay's scroll word.
  static void gt3(Core &c, std::uint32_t uvScroll, const tomba2::horizontal_cull::Visibility &visible);
  static void gt4(Core &c, std::uint32_t uvScroll);
  // The leaf bodies over any memory; each returns the address past its list.
  static std::uint32_t emitGt3(const psx::present::EmitMemory &memory,
                               const tomba2::render::ListCall &call,
                               std::uint32_t uvScroll,
                               const tomba2::horizontal_cull::Visibility &visible);
  static std::uint32_t
  emitGt4(const psx::present::EmitMemory &memory, const tomba2::render::ListCall &call, std::uint32_t uvScroll);

  // All addresses go into the ONE process-global registry via tomba::native::declareOverlayOverride, which
  // also lands the shared thunk in that overlay's image-qualified runtime dispatcher table, so direct
  // guest calls are intercepted alongside typed runtime address dispatch ones. The A00 leaves are reached
  // by direct call from this cluster's own dispatcher and from a tail-shared copy of the same call sequence
  // folded into FUN_80147FC4; A08's from the scenery walker FUN_8012A7CC.
  static void registerOverrides(Game *game);
  // The renders of the leaves' saved calls.
  static void registerStateRenders(Core &core);
};
