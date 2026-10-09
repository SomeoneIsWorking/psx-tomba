// game/render/unlit_model_emitter.h — the unlit GT3/GT4 model emitters: the record's own colours, no
// lighting.
//
// One body, copied across MAIN and the overlays, differing only in data:
//   - the scratch words the GTE flag/MAC0 and the OT bucket are staged through (resident 0x1F800080/84,
//     overlays 0x1F800000/04);
//   - the colour mask (resident and A01 keep every code byte, the others only the first GT4 word's);
//   - the depth mode and where its SZ words are staged (DepthRule);
//   - the resident GT4 projects corner 3 before corners 0..2;
//   - A01's depth cue of each corner by SZ / 4 once the bucket is known.
// Copies:
//   - resident 0x8007FDB0/0x8008007C (FUN_800803DC);
//   - SOP 0x801099B4/0x80109C80 (SopGround::draw), unchanged in A0B and A0C;
//   - A01 0x80132690/0x801329C4, the drawer 0x80132DC0's branch with neither 0x800B8873 nor 0x800B8816;
//   - the flagged copy in A0A-A0F and A0H-A0J (0 average, 1 farthest, anything else nearest), whose GT4
//     jumps past its link after sorting a farthest/nearest record, so only averaged quads are drawn;
//   - SOP's body with a near clamp in A0G, A0H, A0I and A0J;
//   - SOP's body with the lit emitters' depth code and a hide flag in A06;
//   - A06 0x8013CF00/0x8013D1E4, the drawer 0x8013D568's unlit pair, with a U scroll.
// Each takes (a0 = record list, a1 = OT base, a2 = count) and returns the address past the list in v0.
// The screen cull goes through horizontal_visibility_cull.h, so at 4:3 the packets are the guest's.
#pragma once

#include "emit_memory.h"
#include "list_job.h"
#include "model_packet.h"

#include <cstdint>

namespace tomba2::horizontal_cull {
class Visibility;
}

namespace tomba2::render {

class UnlitModelEmitter {
public:
  // How the flag byte picks the depth mode.
  enum class ModeCode {
    OneFarTwoNear, // 1 farthest, 2 nearest, anything else average
    ZeroAverage,   // 0 average, 1 farthest, anything else nearest
    LowBits,       // the low 2 bits: 0 average, 2 nearest, 1 and 3 farthest
  };
  // Where the farthest/nearest modes stage the SZ words: per mode at the frame base (farthest) or right
  // after it (nearest), at the frame base for every record, or at the frame base for those two modes only.
  enum class Staging { PerMode, Always, Unaveraged };
  struct DepthRule {
    ModeCode code;
    std::uint32_t modeMask;
    Staging staging;
    NearClamp nearClamp;
    bool gt4DropsSorted; // GT4 skips its link once a farthest/nearest record is sorted
  };
  struct Variant {
    ModelStage stage;
    std::uint32_t colourMask;
    std::uint32_t firstGt4ColourMask;
    bool gt4CornerThreeFirst;
    std::uint32_t gt3FrameBytes;
    std::uint32_t gt4FrameBytes;
    DepthRule depth;
    std::uint32_t hideFlag; // hides the record while 0x1F80009C is set; 0 for none
    bool cue;
    std::uint32_t uScroll; // flag bit 3 adds this u16 >> 4 to every corner's U; 0 for none
  };
  static constexpr Variant kResident{kResidentStage,
                                     kColourCodeMask,
                                     kColourCodeMask,
                                     true,
                                     0x18u,
                                     0x20u,
                                     {ModeCode::OneFarTwoNear, 3u, Staging::PerMode, kNoNearClamp, false},
                                     0u,
                                     false,
                                     0u};
  static constexpr Variant kSop{kOverlayStage,
                                kColourMask,
                                kColourCodeMask,
                                false,
                                0x18u,
                                0x20u,
                                {ModeCode::OneFarTwoNear, 0xFFu, Staging::PerMode, kNoNearClamp, false},
                                0u,
                                false,
                                0u};
  static constexpr Variant kA01Cue{kOverlayStage,
                                   kColourCodeMask,
                                   kColourCodeMask,
                                   false,
                                   0x10u,
                                   0x10u,
                                   {ModeCode::OneFarTwoNear, 0xFFu, Staging::Always, kNoNearClamp, false},
                                   0u,
                                   true,
                                   0u};
  static constexpr Variant kFlagged{kOverlayStage,
                                    kColourMask,
                                    kColourCodeMask,
                                    false,
                                    0x10u,
                                    0x10u,
                                    {ModeCode::ZeroAverage, 0xFFu, Staging::Unaveraged, kNoNearClamp, true},
                                    0u,
                                    false,
                                    0u};
  // SOP's body with the bucket depth pulled to the near depth from `reach` in front of it (A0G, A0I),
  // further (A0H), or after a lift of 0x50 (A0J).
  static constexpr Variant kSopNear{kOverlayStage,
                                    kColourMask,
                                    kColourCodeMask,
                                    false,
                                    0x18u,
                                    0x20u,
                                    {ModeCode::OneFarTwoNear, 0xFFu, Staging::PerMode, {0, 0x1B7u}, false},
                                    0u,
                                    false,
                                    0u};
  static constexpr Variant kSopNearWide{kOverlayStage,
                                        kColourMask,
                                        kColourCodeMask,
                                        false,
                                        0x18u,
                                        0x20u,
                                        {ModeCode::OneFarTwoNear, 0xFFu, Staging::PerMode, {0, 0x27Fu}, false},
                                        0u,
                                        false,
                                        0u};
  static constexpr Variant kSopNearLifted{kOverlayStage,
                                          kColourMask,
                                          kColourCodeMask,
                                          false,
                                          0x18u,
                                          0x20u,
                                          {ModeCode::OneFarTwoNear, 0xFFu, Staging::PerMode, {0x50, 0x1B7u}, false},
                                          0u,
                                          false,
                                          0u};
  // SOP's body with the lit emitters' depth code, hiding flag bit 2 records while 0x1F80009C is set.
  static constexpr Variant kA06{kOverlayStage,
                                kColourMask,
                                kColourCodeMask,
                                false,
                                0x18u,
                                0x20u,
                                {ModeCode::LowBits, 3u, Staging::PerMode, kNoNearClamp, false},
                                4u,
                                false,
                                0u};
  // A06's third pair: A01's with flagged depth from the low 2 bits, the resident stage, the resident GT4
  // corner order and a U scroll.
  static constexpr Variant kA06Scroll{kResidentStage,
                                      kColourCodeMask,
                                      kColourCodeMask,
                                      true,
                                      0x10u,
                                      0x10u,
                                      {ModeCode::ZeroAverage, 3u, Staging::Unaveraged, kNoNearClamp, false},
                                      0u,
                                      false,
                                      0x8014A450u};

  // The guest call: the registers in, the end of the list in v0, and the call saved under the open object.
  static void gt3(Core &core, const horizontal_cull::Visibility &visible, const Variant &variant);
  static void gt4(Core &core, const horizontal_cull::Visibility &visible, const Variant &variant);
  // The body over any memory; returns the address past the list.
  static std::uint32_t emit(const EmitMemory &memory,
                            const ListCall &call,
                            const horizontal_cull::Visibility &visible,
                            const Variant &variant,
                            bool quad);
  static void registerOverrides();
  // The renders of the saved calls, at every entry the emitter is declared at.
  static void registerStateRenders(Core &core);
};

} // namespace tomba2::render
