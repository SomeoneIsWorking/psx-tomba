// game/render/anchored_packets.h — a 2D object's GP0 packets, placed from one anchor and moved by it.
//
// UI sprites, their quads and text glyphs are drawn at a screen anchor (s16 x, s16 y) the caller hands over.
// The state is that anchor and the command words of each packet with the OT bucket it links into; the render
// moves the anchor by t and every vertex with it.
#pragma once

#include "core.h"
#include "state_producer.h"

#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace tomba::render {

class AnchoredPackets {
public:
  // `texturePage` is the draw mode a sprite takes from the draw mode packet its writer links after it.
  AnchoredPackets(std::uint32_t anchor, std::uint32_t texturePage) : anchor_(anchor), texturePage_(texturePage) {}

  // A packet's command words (without its tag), linked into `bucket` of the active OT.
  void add(std::uint32_t bucket, std::span<const std::uint32_t> command);
  // Saves the packets under the innermost open object; nothing when the active OT is not a named table.
  void save(Core &core) const;

  static std::unique_ptr<psx::present::StateProducer> render();

private:
  std::uint32_t anchor_;
  std::uint32_t texturePage_;
  std::vector<std::uint32_t> buckets_;
  std::vector<std::uint32_t> lengths_;
  std::vector<std::uint32_t> words_;
};

} // namespace tomba::render
