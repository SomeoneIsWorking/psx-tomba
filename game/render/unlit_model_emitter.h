// game/render/unlit_model_emitter.h — the unlit GT3/GT4 model emitters: the record's own colours, no
// lighting.
//
// One body, copied across MAIN and the overlays, differing only in data:
//   - the scratch words the GTE flag/MAC0 and the OT bucket are staged through (resident 0x1F800080/84,
//     overlays 0x1F800000/04);
//   - the colour mask (resident and A01 keep every code byte, the others only the first GT4 word's);
//   - the depth mode, where its SZ words are staged and the band bias of its compression (DepthRule);
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
//   - A06 0x8013CF00/0x8013D1E4, the drawer 0x8013D568's unlit pair, with a U scroll;
//   - A0E 0x80113AEC/0x80113DE0, SOP's body hiding the arena's west records by model x;
//   - A0D 0x80112DE4/0x801131B0, the drawer 0x80113628's pair: A0A's body with a screen wave;
//   - A03 0x80110094, the drawer 0x801109D4's GT3 (its GT4 is subdivided_ground.h): SOP's projection, always
//     averaged, its band picked from depth - 0xE9, two-sided on flag bit 0;
//   - A02 0x80124DB8/0x80125100, the drawer 0x8012555C's pair: the resident body with flagged depth taken
//     from the SZ registers and a U scroll picked by flag bit 4, 3 or 2.
// Each takes (a0 = record list, a1 = OT base, a2 = count) and returns the address past the list in v0.
// The screen cull goes through horizontal_visibility_cull.h, so at 4:3 the packets are the guest's.
#pragma once

#include "emit_memory.h"
#include "guest_ordering_table.h"
#include "list_job.h"
#include "model_packet.h"

#include <array>
#include <cstdint>

namespace tomba::native {
class NativeOverrideCatalog;
} // namespace tomba::native

namespace tomba::horizontal_cull {
class Visibility;
}

namespace tomba::render {

class UnlitModelEmitter {
public:
  // How the flag byte picks the depth mode.
  enum class ModeCode : std::uint8_t {
    OneFarTwoNear, // 1 farthest, 2 nearest, anything else average
    ZeroAverage,   // 0 average, 1 farthest, anything else nearest
    LowBits,       // the low 2 bits: 0 average, 2 nearest, 1 and 3 farthest
  };
  // Where the farthest/nearest modes stage the SZ words: per mode at the frame base (farthest) or right
  // after it (nearest), at the frame base for every record, at the frame base for those two modes only, or
  // not at all (read from the SZ registers).
  enum class Staging : std::uint8_t { PerMode, Always, Unaveraged, Registers };
  struct DepthRule {
    ModeCode code;
    std::uint32_t modeMask;
    Staging staging;
    NearClamp nearClamp;
    bool gt4DropsSorted;   // GT4 skips its link once a farthest/nearest record is sorted
    std::int32_t bandBias; // OrderingTable::compressDepth's band bias
  };
  // How a scroll word reads: its u16 >> 4, or its low byte.
  enum class ScrollRead : std::uint8_t { HalfShift4, Byte };
  // Flag bit `flag` adds word `u` to every corner's U and word `v` (0 for none) to its V, and moves the record to
  // `bucket` (0 to keep its own).
  struct ScrollBit {
    std::uint32_t flag;
    std::uint32_t u;
    std::uint32_t v;
    std::int32_t bucket;
  };
  struct UvScroll {
    ScrollRead read;
    bool firstOnly; // only the first set bit of `bits` applies
    std::array<ScrollBit, 3> bits;
  };
  static constexpr UvScroll kNoScroll{};
  // Flag bit `flag` moves each corner's SY by rsin(the s32 at `phase` + model x * 4) >> 10 and sorts the record
  // `bucketBias` further back; the calls are the guest's rsin return addresses, a corner each.
  struct ScreenWave {
    std::uint32_t flag;
    std::uint32_t phase;
    std::int32_t bucketBias;
    std::array<std::uint32_t, 3> gt3Calls;
    std::array<std::uint32_t, 4> gt4Calls;
  };
  static constexpr ScreenWave kNoWave{};
  // The moving words the area variants read: UV scrolls the area objects step and A0D's wave phase.
  static constexpr std::uint32_t kA06ScrollU = 0x8014A450u;
  static constexpr std::uint32_t kA02ScrollU = 0x8012C928u;
  static constexpr std::uint32_t kA02ScrollV = 0x8012C92Au;
  static constexpr std::uint32_t kA02Band = 0x8012C92Cu;
  static constexpr std::uint32_t kA0fScrollU = 0x8012092Eu;
  static constexpr std::uint32_t kA0dWavePhase = 0x8011CC68u;
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
    UvScroll uvScroll;
    ModelXHide xHide;
    ScreenWave wave;
    std::uint32_t twoSidedFlag; // a GT3 with this flag bit skips the back-face test; 0 for none
  };
  static constexpr Variant kResident{kResidentStage,
                                     kColourCodeMask,
                                     kColourCodeMask,
                                     true,
                                     0x18u,
                                     0x20u,
                                     {ModeCode::OneFarTwoNear, 3u, Staging::PerMode, kNoNearClamp, false, 0},
                                     0u,
                                     false,
                                     kNoScroll,
                                     kNoXHide,
                                     kNoWave,
                                     0u};
  static constexpr Variant kSop{kOverlayStage,
                                kColourMask,
                                kColourCodeMask,
                                false,
                                0x18u,
                                0x20u,
                                {ModeCode::OneFarTwoNear, 0xFFu, Staging::PerMode, kNoNearClamp, false, 0},
                                0u,
                                false,
                                kNoScroll,
                                kNoXHide,
                                kNoWave,
                                0u};
  static constexpr Variant kA01Cue{kOverlayStage,
                                   kColourCodeMask,
                                   kColourCodeMask,
                                   false,
                                   0x10u,
                                   0x10u,
                                   {ModeCode::OneFarTwoNear, 0xFFu, Staging::Always, kNoNearClamp, false, 0},
                                   0u,
                                   true,
                                   kNoScroll,
                                   kNoXHide,
                                   kNoWave,
                                   0u};
  static constexpr Variant kFlagged{kOverlayStage,
                                    kColourMask,
                                    kColourCodeMask,
                                    false,
                                    0x10u,
                                    0x10u,
                                    {ModeCode::ZeroAverage, 0xFFu, Staging::Unaveraged, kNoNearClamp, true, 0},
                                    0u,
                                    false,
                                    kNoScroll,
                                    kNoXHide,
                                    kNoWave,
                                    0u};
  // SOP's body with the bucket depth pulled to the near depth from `reach` in front of it (A0G, A0I),
  // further (A0H), or after a lift of 0x50 (A0J).
  static constexpr Variant kSopNear{kOverlayStage,
                                    kColourMask,
                                    kColourCodeMask,
                                    false,
                                    0x18u,
                                    0x20u,
                                    {ModeCode::OneFarTwoNear, 0xFFu, Staging::PerMode, {0, 0x1B7u}, false, 0},
                                    0u,
                                    false,
                                    kNoScroll,
                                    kNoXHide,
                                    kNoWave,
                                    0u};
  static constexpr Variant kSopNearWide{kOverlayStage,
                                        kColourMask,
                                        kColourCodeMask,
                                        false,
                                        0x18u,
                                        0x20u,
                                        {ModeCode::OneFarTwoNear, 0xFFu, Staging::PerMode, {0, 0x27Fu}, false, 0},
                                        0u,
                                        false,
                                        kNoScroll,
                                        kNoXHide,
                                        kNoWave,
                                        0u};
  static constexpr Variant kSopNearLifted{kOverlayStage,
                                          kColourMask,
                                          kColourCodeMask,
                                          false,
                                          0x18u,
                                          0x20u,
                                          {ModeCode::OneFarTwoNear, 0xFFu, Staging::PerMode, {0x50, 0x1B7u}, false, 0},
                                          0u,
                                          false,
                                          kNoScroll,
                                          kNoXHide,
                                          kNoWave,
                                          0u};
  // SOP's body with the lit emitters' depth code, hiding flag bit 2 records while 0x1F80009C is set.
  static constexpr Variant kA06{kOverlayStage,
                                kColourMask,
                                kColourCodeMask,
                                false,
                                0x18u,
                                0x20u,
                                {ModeCode::LowBits, 3u, Staging::PerMode, kNoNearClamp, false, 0},
                                4u,
                                false,
                                kNoScroll,
                                kNoXHide,
                                kNoWave,
                                0u};
  // A06's third pair: A01's with flagged depth from the low 2 bits, the resident stage, the resident GT4
  // corner order and a U scroll.
  static constexpr Variant kA06Scroll{kResidentStage,
                                      kColourCodeMask,
                                      kColourCodeMask,
                                      true,
                                      0x10u,
                                      0x10u,
                                      {ModeCode::ZeroAverage, 3u, Staging::Unaveraged, kNoNearClamp, false, 0},
                                      0u,
                                      false,
                                      {ScrollRead::HalfShift4, true, {{{8u, kA06ScrollU, 0u, 0}}}},
                                      kNoXHide,
                                      kNoWave,
                                      0u};

  // SOP's body hiding by model x while 0x1F80009C is set: A0E's boss arena, drawn by the walker 0x80114320.
  static constexpr Variant kA0eArena{kOverlayStage,
                                     kColourMask,
                                     kColourCodeMask,
                                     false,
                                     0x18u,
                                     0x20u,
                                     {ModeCode::OneFarTwoNear, 0xFFu, Staging::PerMode, kNoNearClamp, false, 0},
                                     0u,
                                     false,
                                     kNoScroll,
                                     {true, 0x24EA, 0x2508},
                                     kNoWave,
                                     0u};

  // A02's pair: the resident projection, 0 average, 1 farthest, 2 and 3 nearest, three scroll words.
  static constexpr Variant kA02{
      kResidentStage,
      kColourCodeMask,
      kColourCodeMask,
      true,
      0u,
      0u,
      {ModeCode::ZeroAverage, 3u, Staging::Registers, kNoNearClamp, false, 0},
      0u,
      false,
      {ScrollRead::HalfShift4, true, {{{0x10u, kA02Band, 0u, 0}, {8u, kA02ScrollV, 0u, 0}, {4u, kA02ScrollU, 0u, 0}}}},
      kNoXHide,
      kNoWave,
      0u};

  // A0A's pair: SOP's projection, 1 farthest, 2 nearest from the SZ registers, two U/V byte scrolls that add up.
  static constexpr Variant kA0a{
      kOverlayStage,
      kColourMask,
      kColourCodeMask,
      false,
      0u,
      0u,
      {ModeCode::OneFarTwoNear, 3u, Staging::Registers, kNoNearClamp, false, 0},
      0u,
      false,
      {ScrollRead::Byte, false, {{{4u, 0x8011BF14u, 0x8011BF16u, 0}, {8u, 0x8011BF1Au, 0x8011BF16u, 0}}}},
      kNoXHide,
      kNoWave,
      0u};
  // A0F's pair: A0A's body whose scrolls exclude each other and sort the scrolled record far back.
  static constexpr Variant kA0f{
      kOverlayStage,
      kColourMask,
      kColourCodeMask,
      false,
      0u,
      0u,
      {ModeCode::OneFarTwoNear, 3u, Staging::Registers, kNoNearClamp, false, 0},
      0u,
      false,
      {ScrollRead::Byte, true, {{{4u, kA0fScrollU, 0x80120930u, 0x7FB}, {8u, 0x80120938u, 0x8012093Au, 0x7FD}}}},
      kNoXHide,
      kNoWave,
      0u};

  // A0D's pair: A0A's body with a screen wave on flag bit 4 in place of its scrolls.
  static constexpr Variant kA0d{kOverlayStage,
                                kColourMask,
                                kColourCodeMask,
                                false,
                                0u,
                                0u,
                                {ModeCode::OneFarTwoNear, 3u, Staging::Registers, kNoNearClamp, false, 0},
                                0u,
                                false,
                                kNoScroll,
                                kNoXHide,
                                {4u,
                                 kA0dWavePhase,
                                 0x80,
                                 {0x801130C0u, 0x801130E4u, 0x80113108u},
                                 {0x80113514u, 0x80113538u, 0x8011355Cu, 0x80113580u}},
                                0u};
  // A03's GT3 0x80110094, under the drawer 0x801109D4: SOP's projection, always averaged, compressed with
  // kA03BandBias, two-sided on flag bit 0.
  static constexpr Variant kA03{kOverlayStage,
                                kColourMask,
                                kColourCodeMask,
                                false,
                                0u,
                                0u,
                                {ModeCode::OneFarTwoNear, 0u, Staging::PerMode, kNoNearClamp, false, kA03BandBias},
                                0u,
                                false,
                                kNoScroll,
                                kNoXHide,
                                kNoWave,
                                1u};

  // The guest call: the registers in, the end of the list in v0, and the call saved under the open object.
  static void gt3(Core &core, const horizontal_cull::Visibility &visible, const Variant &variant);
  static void gt4(Core &core, const horizontal_cull::Visibility &visible, const Variant &variant);
  // The body over any memory; returns the address past the list.
  static std::uint32_t emit(const psx::present::EmitMemory &memory,
                            const ListCall &call,
                            const horizontal_cull::Visibility &visible,
                            const Variant &variant,
                            bool quad);
  static void registerOverrides(native::NativeOverrideCatalog &natives);
};

} // namespace tomba::render
