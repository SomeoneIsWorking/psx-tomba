// game/render/model_packet.h — the record-to-packet steps every guest GT3/GT4 model list emitter shares.
//
// Each emitter (lit_model_emitter.h, unlit_model_emitter.h, sway_model_emitter.h) takes (a0 = record
// list, a1 = OT base, a2 = count, a3) and returns the address past the list in v0. Per record it projects
// the corners, rejects on GTE overflow, back face or the screen cull, picks a depth, compresses it into an
// OT bucket and links one POLY_GT3 (10 words) or POLY_GT4 (13 words) packet from the pool. The bodies
// differ in the order of those steps, the scratch words they stage through and what they add (lighting,
// sway, UV scroll); the steps themselves are owned here once. A rejected record's packet writes stay in
// the pool unlinked, as in the guest.
#pragma once

#include "core.h"
#include "gte_registers.h"
#include "guest_ordering_table.h"
#include "model_element.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <new>

namespace tomba2::horizontal_cull {
class Visibility;
}

namespace tomba2::render {

// Record offsets of one corner's s16 model coordinates.
struct ModelCorner {
  std::uint32_t x, y, z;
};

// A GT3 or GT4 record and its packet.
struct ModelShape {
  ModelList list;
  std::uint32_t recordBytes;
  std::uint32_t packetBytes;
  std::uint32_t packetWords;
  std::array<ModelCorner, 4> corners;
  int cornerCount;
  std::uint32_t firstDepth; // the first SZ register the depth reads
  std::uint32_t average;    // AVSZ3 or AVSZ4
  std::uint32_t triangle;   // record offset of corner 0's VXY
};

inline constexpr ModelShape kModelGt3{ModelList::Gt3,
                                      0x24u,
                                      0x28u,
                                      9u,
                                      {{{0x10u, 0x12u, 0x14u}, {0x18u, 0x1Au, 0x16u}, {0x1Cu, 0x1Eu, 0x20u}, {}}},
                                      3,
                                      gte::kSz1,
                                      gte::kAvsz3,
                                      0x10u};
inline constexpr ModelShape kModelGt4{
    ModelList::Gt4,
    0x2Cu,
    0x34u,
    12u,
    {{{0x14u, 0x16u, 0x18u}, {0x1Cu, 0x1Eu, 0x1Au}, {0x20u, 0x22u, 0x24u}, {0x28u, 0x2Au, 0x26u}}},
    4,
    gte::kSz0,
    gte::kAvsz4,
    0x14u};

// Scratch words the GTE flag/MAC0 and the OT bucket are staged through.
struct ModelStage {
  std::uint32_t gte;
  std::uint32_t bucket;
};

inline constexpr ModelStage kOverlayStage{0x1F800000u, 0x1F800004u};
inline constexpr ModelStage kResidentStage{0x1F800080u, 0x1F800084u};

// Where the lit and A01 emitters stage the SZ values their depth reads.
inline constexpr std::uint32_t kDepthStage = 0x1F800084u;

// Colour word masks: the GPU ignores each channel's low nibble; the code byte survives only in the first.
inline constexpr std::uint32_t kColourMask = 0x00F0F0F0u;
inline constexpr std::uint32_t kColourCodeMask = 0xFFF0F0F0u;

enum class ModelDepth { Average, Farthest, Nearest };

// The flag bit that hides an A01 record while the scratchpad word 0x1F80009C is set.
inline constexpr std::uint32_t kHideFlag = 0x40u;

// The bucket depth a near record is pulled to.
inline constexpr std::int32_t kNearDepth = 0x28;
// Adds `lift` to the bucket depth, then moves a depth within `reach` in front of kNearDepth onto it.
struct NearClamp {
  std::int32_t lift;
  std::uint32_t reach;
};
inline constexpr NearClamp kNoNearClamp{0, 0u};

// The scene-entity walkers (A01 FUN_80132358, A08 FUN_8012A7CC) call the emitters outside any producer;
// there the list is the drawn object, a table slot of the area's scenery.
class ModelObjectScope {
public:
  ModelObjectScope(psx::present::EmissionScope &scope, std::uint32_t entry, std::uint32_t list) {
    if (!scope.isOpen()) {
      guard_ = new (storage_) psx::present::EmissionScope::Guard(scope, entry, list, 0);
    }
  }
  ~ModelObjectScope() {
    if (guard_ != nullptr) {
      guard_->~Guard();
    }
  }
  ModelObjectScope(const ModelObjectScope &) = delete;
  ModelObjectScope &operator=(const ModelObjectScope &) = delete;
  ModelObjectScope(ModelObjectScope &&) = delete;
  ModelObjectScope &operator=(ModelObjectScope &&) = delete;

private:
  alignas(psx::present::EmissionScope::Guard) std::byte storage_[sizeof(psx::present::EmissionScope::Guard)];
  psx::present::EmissionScope::Guard *guard_ = nullptr;
};

// One record's packet.
class ModelPacket {
public:
  ModelPacket(Core &core, const ModelShape &shape, std::uint32_t record, std::uint32_t packet, ModelStage stage);

  std::uint32_t record() const {
    return mRecord;
  }
  std::uint32_t packet() const {
    return mPacket;
  }
  // The record's second colour word's top byte: depth mode and per-emitter flags.
  std::uint32_t flags() const;

  // GT3: RTPT, flag, back face, cull; the packet's colours (`colourMask`), UVs and SXYs.
  bool projectGt3(std::uint32_t colourMask, const horizontal_cull::Visibility &visible) const;
  // GT4 with corners 0..2 by RTPT then corner 3 by RTPS.
  bool
  projectGt4(std::uint32_t firstColourMask, std::uint32_t colourMask, const horizontal_cull::Visibility &visible) const;
  // GT4 with corner 3 by RTPS first (the resident emitter).
  bool projectGt4CornerThreeFirst(std::uint32_t colourMask, const horizontal_cull::Visibility &visible) const;

  // The record's SZ values, one word per corner from `address`.
  void stageDepths(std::uint32_t address) const;
  // The depth into the bucket stage: AVSZ, or the farthest/nearest of the SZ words at `staged`, / 4.
  void depth(ModelDepth mode, std::uint32_t staged) const;
  // Adds `bias` to the bucket stage, capped at the farthest bucket.
  void biasBucket(std::int32_t bias) const;
  // The near clamp on the bucket stage; nothing for kNoNearClamp.
  void clampNear(const NearClamp &clamp) const;
  // Compresses the bucket stage into an OT bucket; kNoBucket outside [4, 0x7FF].
  std::int32_t bucket() const;
  // The packet's last UV word(s): GT3 its u16 at record 0x22, GT4 both halves of record 0x10.
  void storeLastUv() const;
  // Adds `amount` to every corner's U, wrapping in the byte.
  void scrollU(std::uint32_t amount) const;
  // DPCS: the corner's colour toward the far colour by `intensity`, in place.
  void shade(int corner, std::int32_t intensity) const;
  // Depth cue: shades the corner by its SZ word at `staged` (one word per corner), / 4.
  void cueByDepth(int corner, std::uint32_t staged) const;
  // Links the packet into the bucket stage's OT bucket.
  void link(std::uint32_t orderingTable) const;

  // 0 average, 2 nearest, 1 and 3 farthest (the point-lit emitters).
  static ModelDepth litDepth(std::uint32_t flags);
  // 1 farthest, 2 nearest, anything else average; SOP compares the whole flag byte, the others its low 2 bits.
  static ModelDepth depthOf(std::uint32_t flags, std::uint32_t modeMask);
  // 0 average, 1 farthest, anything else nearest (the flagged overlay copy).
  static ModelDepth flaggedDepth(std::uint32_t flags);
  // `hideFlag` hides the record while the scratchpad word 0x1F80009C is set.
  static bool hidden(Core &core, std::uint32_t flags, std::uint32_t hideFlag = kHideFlag);

  // Packet offsets of each corner's colour and SXY; a corner's SY is its SX + 2.
  static constexpr std::array<std::uint32_t, 4> kColour{4u, 0x10u, 0x1Cu, 0x28u};
  static constexpr std::array<std::uint32_t, 4> kScreen{8u, 0x14u, 0x20u, 0x2Cu};
  // Packet offsets of each corner's UV.
  static constexpr std::array<std::uint32_t, 4> kUv{0xCu, 0x18u, 0x24u, 0x30u};

private:
  bool transformFailed() const;
  bool facesAway() const;
  void loadTriangle() const;
  void storeTriangle() const;
  void loadCornerThree() const;
  bool onScreen(const horizontal_cull::Visibility &visible) const;

  Core &mCore;
  const ModelShape &mShape;
  std::uint32_t mRecord;
  std::uint32_t mPacket;
  ModelStage mStage;
};

// Runs `emit(packet)` for each record of the list in a0 (count a2), each the element of its list and index;
// the pool advances past each packet `emit` linked. Returns the address past the list.
template <typename Emit> std::uint32_t emitModelList(Core &core, const ModelShape &shape, ModelStage stage, Emit emit) {
  std::uint32_t record = core.r[4];
  const std::uint32_t count = core.r[6];
  const PacketPool pool(core);
  std::uint32_t packet = pool.cursor();
  for (std::uint32_t index = 0; index != count; ++index, record += shape.recordBytes) {
    const auto element = core.emission.element(modelElement(shape.list, index));
    if (emit(ModelPacket(core, shape, record, packet, stage))) {
      packet += shape.packetBytes;
    }
  }
  pool.setCursor(packet);
  return record;
}

} // namespace tomba2::render
