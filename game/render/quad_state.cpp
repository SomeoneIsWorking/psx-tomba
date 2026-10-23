// game/render/quad_state.cpp — QuadState. See quad_state.h.
#include "quad_state.h"

#include "blend.h"
#include "core.h"
#include "entry/session.h"
#include "gp0_primitive_decode.h"
#include "guest_call.h"
#include "guest_ordering_table.h"
#include "ordering_table.h"
#include "state_bytes.h"

#include <memory>
#include <utility>
#include <vector>

namespace tomba::render {
namespace {

struct Header {
  std::uint32_t table = 0;
  std::uint32_t slot = 0; // OT slot of bucket 0
  std::uint32_t packets = 0;
};

// One linked packet: its bucket and its command words (the tag excluded).
struct Packet {
  std::uint32_t bucket = 0;
  std::vector<std::uint32_t> words;
};

struct Saved {
  Header header;
  std::vector<Packet> packets;

  explicit Saved(std::span<const std::byte> state) {
    psx::present::StateReader reader(state);
    header = reader.get<Header>();
    packets.reserve(header.packets);
    for (std::uint32_t index = 0; index < header.packets; ++index) {
      Packet packet;
      packet.bucket = reader.get<std::uint32_t>();
      packet.words = reader.getAll<std::uint32_t>(reader.get<std::uint32_t>());
      packets.push_back(std::move(packet));
    }
  }
};

void lerpVertex(psx::present::RecordVertex &vertex, const psx::present::RecordVertex &from, float t) {
  vertex.x = psx::present::lerpInt(from.x, vertex.x, t);
  vertex.y = psx::present::lerpInt(from.y, vertex.y, t);
  vertex.subX = from.subX + (vertex.subX - from.subX) * t;
  vertex.subY = from.subY + (vertex.subY - from.subY) * t;
  vertex.r = static_cast<std::uint8_t>(psx::present::lerpInt(from.r, vertex.r, t));
  vertex.g = static_cast<std::uint8_t>(psx::present::lerpInt(from.g, vertex.g, t));
  vertex.b = static_cast<std::uint8_t>(psx::present::lerpInt(from.b, vertex.b, t));
  vertex.u = static_cast<std::uint8_t>(psx::present::lerpInt(from.u, vertex.u, t));
  vertex.v = static_cast<std::uint8_t>(psx::present::lerpInt(from.v, vertex.v, t));
}

void savePackets(Core &core, std::uint32_t emitter, std::uint32_t object, std::uint32_t before, std::uint32_t after) {
  const OrderingTable ot = OrderingTable::active(core);
  const auto slot = core.otTables.slotOf(ot.base());
  if (!slot) {
    return;
  }
  // A zero span takes every packet the key names; otherwise only those a call linked.
  const bool spanned = before != after;
  std::vector<Packet> packets;
  for (std::uint32_t bucket = 0; bucket < OrderingTable::kBucketCount; ++bucket) {
    for (std::uint32_t packet = core.mem_r32(ot.slot(bucket)); !spanned || (packet >= before && packet < after);) {
      const psx::gpu::OtNode node(psx::gpu::mainRamOffsetOf(packet), psx::gpu::OtHeaderWord{core.mem_r32(packet)});
      const std::optional<psx::present::RecordKey> key = core.emission.keyFor(packet);
      if (key && key->producer == emitter && key->object == object) {
        Packet saved;
        saved.bucket = bucket;
        saved.words.resize(node.gp0WordCount());
        for (std::uint32_t word = 0; word < saved.words.size(); ++word) {
          saved.words[word] = core.mem_r32(packet + 4u + word * 4u);
        }
        packets.push_back(std::move(saved));
      }
      if (node.endsChain()) {
        break;
      }
      packet = psx::gpu::guestAddressOf(node.nextRamOffset());
    }
  }
  if (packets.empty()) {
    return;
  }
  psx::present::StateWriter state;
  state.put(Header{slot->table, slot->index, static_cast<std::uint32_t>(packets.size())});
  for (const Packet &packet : packets) {
    state.put(packet.bucket);
    state.put(static_cast<std::uint32_t>(packet.words.size()));
    state.putAll(std::span<const std::uint32_t>(packet.words));
  }
  core.frameStates.save(core.emission.current(), state.bytes());
}

} // namespace

void saveQuads(Core &core, const QuadRange &range) {
  savePackets(core, range.emitter, range.object, range.before, range.after);
}

void saveKeyedQuads(Core &core, std::uint32_t emitter, std::uint32_t object) {
  savePackets(core, emitter, object, 0u, 0u);
}

std::uint32_t nodeObject(Core &core) {
  return eng(&core).graphicsBind.records.object(core.r[4]);
}

void runGuestEmitter(Core &core, std::uint32_t emitter, std::uint32_t object, const char *owner) {
  const std::uint32_t otBase = OrderingTable::active(core).base();
  if (!core.otTables.slotOf(otBase)) {
    // Without a published slot the packets would be keyed with no state to pair them with.
    psx::cpu::callOriginalToReturn(core, emitter, psx::cpu::ExecutionBudget::currentTurn(core), owner);
    return;
  }
  const psx::present::EmissionScope::Guard scope(core.emission, emitter, object, 0);
  const std::uint32_t before = PacketPool(core).cursor();
  psx::cpu::callOriginalToReturn(core, emitter, psx::cpu::ExecutionBudget::currentTurn(core), owner);
  saveQuads(core, {.emitter = emitter, .object = object, .before = before, .after = PacketPool(core).cursor()});
}

void QuadStateRender::render(const psx::present::StateInterval &states, psx::present::PrimitiveSink &sink) const {
  const auto [from, to, t] = states;
  const Saved later(to);
  const std::optional<Saved> earlier = from.data() == to.data() ? std::nullopt : std::optional<Saved>(from);
  const bool paired = earlier && earlier->packets.size() == later.packets.size();
  for (std::size_t index = 0; index < later.packets.size(); ++index) {
    const Packet &packet = later.packets[index];
    auto primitive = psx::gpu::decodePacketPrimitive({.words = packet.words, .places = {}});
    if (!primitive) {
      continue;
    }
    if (paired) {
      const auto before = psx::gpu::decodePacketPrimitive({.words = earlier->packets[index].words, .places = {}});
      if (before && before->kind == primitive->kind && before->vertexCount == primitive->vertexCount) {
        for (int vertex = 0; vertex < primitive->vertexCount; ++vertex) {
          lerpVertex(primitive->vertices[static_cast<std::size_t>(vertex)],
                     before->vertices[static_cast<std::size_t>(vertex)],
                     t);
        }
      }
    }
    sink.emit(psx::present::OtSlot{static_cast<std::uint16_t>(later.header.table),
                                   static_cast<std::uint16_t>(later.header.slot + packet.bucket)},
              *primitive);
  }
}

} // namespace tomba::render
