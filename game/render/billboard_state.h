// game/render/billboard_state.h — what one billboardEmit call drew, and its render.
//
// FUN_8003C8F4 projects each particle's four corner vectors through the composed billboard transform in GTE
// CR0-7 and links one textured quad per visible particle. Its state is that transform, and per drawn particle
// its corners, bucket and packet; the render moves the transform and the corners by t and projects again.
#pragma once

#include "core.h"
#include "state_producer.h"

#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace tomba::render {

class BillboardState {
public:
  static constexpr std::uint32_t kEmitter = 0x8003C8F4u; // FUN_8003C8F4, billboardEmit
  static constexpr std::uint32_t kCornerWords = 8u;      // V0..V2 as RTPT reads them, then V3
  static constexpr std::uint32_t kPacketWords = 9u;

  // Reads the GTE control registers the particles are projected under.
  explicit BillboardState(const psx::cpu::Gte &gte);

  // One drawn particle: its corner vectors, the bucket it links into and its packet's command words.
  void add(std::span<const std::uint32_t, kCornerWords> corners,
           std::uint32_t bucket,
           std::span<const std::uint32_t, kPacketWords> command);
  // Saves under the innermost open object; nothing when the active OT is not a named table.
  void save(Core &core) const;

  static void registerStateRenders(psx::present::StateProducerTable &renders, Core &core);

private:
  std::vector<std::uint32_t> particles_; // per particle: corners, bucket, command
  std::array<std::uint32_t, 32> control_{};
};

} // namespace tomba::render
