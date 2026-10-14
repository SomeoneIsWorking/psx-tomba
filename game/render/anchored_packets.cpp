// game/render/anchored_packets.cpp — AnchoredPackets. See anchored_packets.h.
#include "anchored_packets.h"

#include "blend.h"
#include "gp0_primitive_decode.h"
#include "guest_ordering_table.h"
#include "state_bytes.h"

#include <cstdlib>
#include <lucent/log.h>
#include <optional>

namespace tomba::render {
namespace {

struct Header {
  std::uint32_t table = 0;
  std::uint32_t slot = 0; // OT slot of bucket 0
  std::uint32_t anchor = 0;
  std::uint32_t texturePage = 0;
  std::uint32_t packets = 0;
  std::uint32_t words = 0;
};

struct Saved {
  Header header;
  std::vector<std::uint32_t> buckets;
  std::vector<std::uint32_t> lengths;
  std::vector<std::uint32_t> words;

  explicit Saved(std::span<const std::byte> state) {
    psx::present::StateReader reader(state);
    header = reader.get<Header>();
    buckets = reader.getAll<std::uint32_t>(header.packets);
    lengths = reader.getAll<std::uint32_t>(header.packets);
    words = reader.getAll<std::uint32_t>(header.words);
  }
};

std::int32_t low(std::uint32_t pair) {
  return static_cast<std::int16_t>(pair);
}
std::int32_t high(std::uint32_t pair) {
  return static_cast<std::int16_t>(pair >> 16);
}

class AnchoredRender final : public psx::present::StateProducer {
public:
  void render(std::span<const std::byte> from,
              std::span<const std::byte> to,
              float t,
              psx::present::PrimitiveSink &sink) const override {
    const Saved later(to);
    std::uint32_t anchor = later.header.anchor;
    if (from.data() != to.data()) {
      const Saved earlier(from);
      if (earlier.header.packets == later.header.packets) {
        anchor = psx::present::lerpHalves(earlier.header.anchor, later.header.anchor, t);
      }
    }
    const std::int32_t dx = low(anchor) - low(later.header.anchor);
    const std::int32_t dy = high(anchor) - high(later.header.anchor);
    // The last packet linked heads its bucket, so the walk meets the packets last first.
    std::size_t at = later.words.size();
    for (std::size_t packet = later.buckets.size(); packet-- > 0;) {
      at -= later.lengths[packet];
      const std::span<const std::uint32_t> command(later.words.data() + at, later.lengths[packet]);
      std::optional<psx::present::DrawPrimitive> primitive = psx::gpu::decodePacketPrimitive(command);
      if (!primitive) {
        lucent::error("anchored-packets", "saved packet 0x{:08X} did not decode", command.empty() ? 0u : command[0]);
        std::abort();
      }
      if (primitive->kind != psx::present::PrimitiveKind::Polygon) {
        psx::gpu::applyTexPageAttribute(primitive->state, static_cast<std::uint16_t>(later.header.texturePage));
      }
      for (int vertex = 0; vertex < primitive->vertexCount; ++vertex) {
        primitive->vertices[static_cast<std::size_t>(vertex)].x += dx;
        primitive->vertices[static_cast<std::size_t>(vertex)].y += dy;
      }
      sink.emit(psx::present::OtSlot{static_cast<std::uint16_t>(later.header.table),
                                     later.header.slot + later.buckets[packet]},
                *primitive);
    }
  }
};

} // namespace

void AnchoredPackets::add(std::uint32_t bucket, std::span<const std::uint32_t> command) {
  buckets_.push_back(bucket);
  lengths_.push_back(static_cast<std::uint32_t>(command.size()));
  words_.insert(words_.end(), command.begin(), command.end());
}

void AnchoredPackets::save(Core &core) const {
  const auto slot = core.otTables.slotOf(OrderingTable::active(core).base());
  if (!slot) {
    return;
  }
  psx::present::StateWriter state;
  state.put(Header{slot->table,
                   slot->index,
                   anchor_,
                   texturePage_,
                   static_cast<std::uint32_t>(buckets_.size()),
                   static_cast<std::uint32_t>(words_.size())});
  state.putAll(std::span<const std::uint32_t>(buckets_));
  state.putAll(std::span<const std::uint32_t>(lengths_));
  state.putAll(std::span<const std::uint32_t>(words_));
  core.frameStates.save(core.emission.current(), state.bytes());
}

std::unique_ptr<psx::present::StateProducer> AnchoredPackets::render() {
  return std::make_unique<AnchoredRender>();
}

} // namespace tomba::render
