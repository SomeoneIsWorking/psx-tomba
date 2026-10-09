// The frame a list emitter's call leaves in its ordering table, and its state render beside it: what the
// emitter tests compare to show a render at t = 1 draws the packets the guest body wrote.
#pragma once

#include "core.h"
#include "frame_record.h"
#include "frame_state.h"
#include "gp0_primitive_decode.h"
#include "state_producer.h"

#include <algorithm>
#include <cstdint>
#include <variant>
#include <vector>

namespace tomba::test {

inline constexpr std::uint16_t kOtTable = 0;
inline constexpr std::uint32_t kOtBuckets = 0x800u;
inline constexpr std::uint32_t kOtChainEnd = 0x00FFFFFFu;

class CollectedSink final : public psx::present::PrimitiveSink {
public:
  void emit(psx::present::OtSlot slot, const psx::present::DrawPrimitive &primitive) override {
    psx::present::DrawPrimitive placed = primitive;
    placed.slot = slot;
    drawn.push_back(placed);
  }
  std::vector<psx::present::DrawPrimitive> drawn;
};

// Every packet of the OT at `ot`, high bucket to low, keyed by the emission scope and slotted by bucket.
inline psx::present::FrameRecord walkOrderingTable(Core &core, std::uint32_t ot) {
  psx::present::FrameRecord record;
  std::uint16_t drawMode = 0; // the texture page a sprite takes from the last draw mode packet
  for (std::uint32_t bucket = kOtBuckets; bucket-- > 0;) {
    std::uint32_t packet = core.mem_r32(ot + bucket * 4u) & kOtChainEnd;
    while (packet != kOtChainEnd) {
      const std::uint32_t tag = core.mem_r32(packet);
      std::vector<std::uint32_t> words;
      for (std::uint32_t word = 0; word < tag >> 24; ++word) {
        words.push_back(core.mem_r32(packet + 4u + word * 4u));
      }
      if (!words.empty() && words[0] >> 24 == 0xE1u) {
        drawMode = static_cast<std::uint16_t>(words[0]);
      }
      auto primitive = psx::gpu::decodePacketPrimitive(words);
      if (primitive) {
        if (primitive->kind != psx::present::PrimitiveKind::Polygon) {
          psx::gpu::applyTexPageAttribute(primitive->state, drawMode);
        }
        primitive->key = core.emission.keyFor(0x80000000u | packet);
        primitive->slot = psx::present::OtSlot{kOtTable, bucket};
        record.append(*primitive);
      }
      packet = tag & kOtChainEnd;
    }
  }
  return record;
}

inline std::vector<psx::present::DrawPrimitive> primitivesOf(const psx::present::FrameRecord &record,
                                                             std::uint32_t producer) {
  std::vector<psx::present::DrawPrimitive> found;
  for (const psx::present::RecordEntry &entry : record.entries()) {
    const auto &primitive = std::get<psx::present::DrawPrimitive>(entry);
    if (primitive.key && primitive.key->producer == producer) {
      found.push_back(primitive);
    }
  }
  return found;
}

// Every field a render owns, against the packet the guest body wrote.
inline bool samePrimitive(const psx::present::DrawPrimitive &a, const psx::present::DrawPrimitive &b) {
  psx::present::DrawPrimitive left = a;
  left.key = b.key;
  left.sourceAddress = b.sourceAddress;
  return left == b;
}

inline bool sameFrame(const std::vector<psx::present::DrawPrimitive> &a,
                      const std::vector<psx::present::DrawPrimitive> &b) {
  return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](const auto &x, const auto &y) {
           return samePrimitive(x, y);
         });
}

} // namespace tomba::test
