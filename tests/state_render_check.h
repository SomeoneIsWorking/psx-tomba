// The frame a list emitter's call leaves in its ordering table, and the frame the compositor builds from its
// saved state: what the emitter tests compare to show a render at t = 1 draws the packets the guest body wrote.
#pragma once

#include "core.h"
#include "frame_composer.h"
#include "gpu_ports.h"

#include <algorithm>
#include <cstdint>
#include <variant>
#include <vector>

namespace tomba::test {

inline constexpr std::uint16_t kOtTable = 0;
inline constexpr std::uint32_t kOtBuckets = 0x800u;
inline constexpr std::uint32_t kOtChainEnd = 0x00FFFFFFu;

// ClearOTagR: each bucket links to the one below it, and bucket 0 ends the chain. The table is named
// so a producer can save the slot it draws into.
inline void clearOrderingTable(Core &core, std::uint32_t ot) {
  core.otTables.clear();
  core.otTables.name(kOtTable, ot, kOtBuckets, sizeof(std::uint32_t), psx::gpu::OtWalk::HighToLow);
  core.mem_w32(ot, kOtChainEnd);
  for (std::uint32_t bucket = 1; bucket < kOtBuckets; ++bucket) {
    core.mem_w32(ot + bucket * 4u, (ot + (bucket - 1u) * 4u) & kOtChainEnd);
  }
}

// DrawOTag: the GPU's record of the OT at `ot`, sent from its top bucket by DMA2 in linked-list mode.
inline psx::present::FrameRecord submittedFrame(Core &core, std::uint32_t ot) {
  constexpr std::uint32_t kGp0 = 0x1F801810u;
  constexpr std::uint32_t kGp1 = 0x1F801814u;
  constexpr std::uint32_t kDma2Address = 0x1F8010A0u;
  constexpr std::uint32_t kDma2Control = 0x1F8010A8u;
  constexpr std::uint32_t kDma2LinkedListToGpu = 0x01000401u;
  core.mem_w32(kGp1, 0x00000000u);
  core.mem_w32(kGp0, 0xE3000000u);
  core.mem_w32(kGp0, 0xE4000000u | 1023u | (511u << 10));
  core.mem_w32(kGp0, 0xE5000000u);
  psx::gpu::sealRecord(core);
  core.mem_w32(kDma2Address, ot + (kOtBuckets - 1u) * 4u);
  core.mem_w32(kDma2Control, kDma2LinkedListToGpu);
  return psx::gpu::sealRecord(core);
}

inline std::vector<psx::present::DrawPrimitive> primitivesOf(const psx::present::FrameRecord &record,
                                                             std::uint32_t producer) {
  std::vector<psx::present::DrawPrimitive> found;
  for (const psx::present::RecordEntry &entry : record.entries()) {
    const auto *primitive = std::get_if<psx::present::DrawPrimitive>(&entry);
    if (primitive != nullptr && primitive->key && primitive->key->producer == producer) {
      found.push_back(*primitive);
    }
  }
  return found;
}

// The object's primitives in `record`.
inline std::vector<psx::present::DrawPrimitive> objectPrimitives(const psx::present::FrameRecord &record,
                                                                 psx::present::ObjectId id) {
  std::vector<psx::present::DrawPrimitive> found;
  for (const psx::present::DrawPrimitive &primitive : primitivesOf(record, id.producer)) {
    if (primitive.key->object == id.object) {
      found.push_back(primitive);
    }
  }
  return found;
}

struct ComposeArgs {
  const psx::present::FrameRecord &shown;
  const psx::present::FrameState &from;
  const psx::present::FrameState &to;
  float t = 1.0f;
};

// The object's primitives the compositor draws over `shown` from its saved states; empty when it kept the
// guest's packets instead (they carry their scope serial, the compositor's do not).
inline std::vector<psx::present::DrawPrimitive>
composedPrimitives(const psx::present::StateProducers &renders, const ComposeArgs &args, psx::present::ObjectId id) {
  const auto composed = psx::present::composeFrame(args.shown, &args.from, args.to, args.t, renders);
  std::vector<psx::present::DrawPrimitive> found;
  if (!composed) {
    return found;
  }
  for (const psx::present::DrawPrimitive &primitive : objectPrimitives(*composed, id)) {
    if (primitive.key->serial == 0) {
      found.push_back(primitive);
    }
  }
  return found;
}

// Every field a render owns, against the packet the guest body wrote.
inline psx::present::DrawPrimitive renderedFields(psx::present::DrawPrimitive primitive) {
  primitive.key = {};
  primitive.sourceAddress = {};
  return primitive;
}
inline bool samePrimitive(const psx::present::DrawPrimitive &a, const psx::present::DrawPrimitive &b) {
  return renderedFields(a) == renderedFields(b);
}

inline bool sameFrame(const std::vector<psx::present::DrawPrimitive> &a,
                      const std::vector<psx::present::DrawPrimitive> &b) {
  return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](const auto &x, const auto &y) {
           return samePrimitive(x, y);
         });
}

} // namespace tomba::test
