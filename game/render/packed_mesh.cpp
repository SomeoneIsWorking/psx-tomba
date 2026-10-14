// game/render/packed_mesh.cpp — PackedMesh. See packed_mesh.h.
#include "packed_mesh.h"

#include "core.h"
#include "core/overrides/native_override_catalog.h"
#include "gte_registers.h"
#include "guest_ordering_table.h"
#include "list_state_producer.h"
#include "model_element.h"

#include <memory>

namespace tomba::render {
namespace {

constexpr std::uint32_t kFrameBytes = 0x28u;
// The frame's words: four staged corners, the GTE flag, then the OTZ and its bucket.
constexpr std::uint32_t kCornerBytes = 8u;
constexpr std::uint32_t kFlagWord = 0x20u;
constexpr std::uint32_t kBucketWord = 0x24u;

constexpr std::uint32_t kPacketWords = 12u;
constexpr std::uint8_t kGouraudQuad = 0x3C;
constexpr std::uint8_t kGouraudQuadSemi = 0x3E;
constexpr std::uint32_t kSemiBit = 0x40000000u;
constexpr std::uint32_t kTexturePageMask = 0x007FFFFFu;
constexpr std::uint32_t kClutRowShift = 22u;

// Record byte offsets of each corner's x, y and z.
struct CornerBytes {
  std::uint32_t x, y, z;
};
constexpr CornerBytes kCorners[4] = {
    {0x1Cu, 0x1Eu, 0x0Fu}, {0x1Du, 0x1Fu, 0x13u}, {0x20u, 0x22u, 0x17u}, {0x21u, 0x23u, 0x1Bu}};

class Mesh {
public:
  Mesh(const psx::present::EmitMemory &memory, const ListCall &call)
      : memory_(memory), call_(call), frame_(call.sp - kFrameBytes) {}

  // The record and the packet it becomes.
  struct Quad {
    std::uint32_t record;
    std::uint32_t packet;
  };

  // One record into its packet; false when the GTE overflows or the depth leaves the OT.
  bool write(const Quad &quad) const {
    const auto [record, packet] = quad;
    const std::uint32_t texturePage = memory_.mem_r32(record + 4u);
    for (std::uint32_t corner = 0; corner < 4u; ++corner) {
      stage(quad, corner);
    }
    loadTriangle();
    gte().execute(psx::gte::kRtpt);
    if (overflowed()) {
      return false;
    }
    for (std::uint32_t corner = 0; corner < 3u; ++corner) {
      memory_.storeGteXy(packet + kScreen[corner], static_cast<int>(psx::gte::kSxy0 + corner));
    }
    loadLastCorner();
    memory_.mem_w32(packet + kUv[0], memory_.mem_r32(record) + (call_.a1 << kClutRowShift));
    gte().execute(psx::gte::kRtps);
    if (overflowed()) {
      return false;
    }
    memory_.storeGteXy(packet + kScreen[3], static_cast<int>(psx::gte::kSxy2));
    memory_.mem_w32(packet + kUv[1], texturePage & kTexturePageMask);
    const std::int32_t bucket = depthBucket();
    if (bucket < 0) {
      return false;
    }
    const std::uint32_t uv2 = memory_.mem_r32(record + 8u);
    memory_.mem_w32(packet + kUv[2], uv2);
    memory_.mem_w32(packet + kUv[3], static_cast<std::uint32_t>(static_cast<std::int32_t>(uv2) >> 16));
    for (const std::uint32_t uv : kUv) {
      memory_.mem_w8(packet + uv, static_cast<std::uint8_t>(memory_.mem_r8(packet + uv) + call_.a3));
    }
    shade(quad);
    memory_.mem_w8(packet + 7u, (texturePage & kSemiBit) != 0u ? kGouraudQuadSemi : kGouraudQuad);
    OrderingTable(memory_, call_.ot)
        .link({.packet = packet, .words = kPacketWords, .bucket = static_cast<std::uint32_t>(bucket)});
    return true;
  }

private:
  static constexpr std::uint32_t kScreen[4] = {8u, 0x14u, 0x20u, 0x2Cu};
  static constexpr std::uint32_t kUv[4] = {0xCu, 0x18u, 0x24u, 0x30u};
  static constexpr std::uint32_t kColour[4] = {4u, 0x10u, 0x1Cu, 0x28u};

  psx::cpu::Gte &gte() const {
    return memory_.core().gte();
  }
  // The corner's bytes as s16 << 8 on the stack, where the GTE loads them from.
  void stage(const Quad &quad, std::uint32_t corner) const {
    const std::uint32_t record = quad.record;
    const std::uint32_t at = frame_ + corner * kCornerBytes;
    const CornerBytes &bytes = kCorners[corner];
    memory_.mem_w16(at, static_cast<std::uint16_t>(memory_.mem_r8(record + bytes.x) << 8));
    memory_.mem_w16(at + 2u, static_cast<std::uint16_t>(memory_.mem_r8(record + bytes.y) << 8));
    memory_.mem_w16(at + 4u, static_cast<std::uint16_t>(memory_.mem_r8(record + bytes.z) << 8));
  }
  // Corners 0..2 into V0..V2 for RTPT.
  void loadTriangle() const {
    for (std::uint32_t corner = 0; corner < 3u; ++corner) {
      const std::uint32_t at = frame_ + corner * kCornerBytes;
      gte().writeData(psx::gte::kVxy0 + corner * 2u, memory_.mem_r32(at));
      gte().writeData(psx::gte::kVz0 + corner * 2u, memory_.mem_r32(at + 4u));
    }
  }
  // Corner 3 into V0 for RTPS.
  void loadLastCorner() const {
    const std::uint32_t at = frame_ + 3u * kCornerBytes;
    gte().writeData(psx::gte::kVxy0, memory_.mem_r32(at));
    gte().writeData(psx::gte::kVz0, memory_.mem_r32(at + 4u));
  }
  bool overflowed() const {
    memory_.mem_w32(frame_ + kFlagWord, gte().readControl(psx::gte::kFlag));
    return static_cast<std::int32_t>(memory_.mem_r32(frame_ + kFlagWord)) < 0;
  }
  // AVSZ4 plus the bias, compressed; kNoBucket outside the OT.
  std::int32_t depthBucket() const {
    gte().execute(psx::gte::kAvsz4);
    const auto depth = static_cast<std::int32_t>(gte().readData(psx::gte::kOtz));
    std::int32_t bucket = OrderingTable::compressDepth(depth + static_cast<std::int16_t>(call_.count));
    if (!OrderingTable::inDepthRange(bucket)) {
      bucket = OrderingTable::kNoBucket;
    }
    memory_.mem_w32(frame_ + kBucketWord, static_cast<std::uint32_t>(bucket));
    return bucket;
  }
  // Corners 0..2 by DPCT and corner 3 by DPCS, all toward the far colour by the scratch IR0.
  void shade(const Quad &quad) const {
    const auto [record, packet] = quad;
    gte().writeData(psx::gte::kIr0, memory_.mem_r32(PackedMesh::kDepthCue));
    gte().writeData(psx::gte::kRgb0, memory_.mem_r32(record + 0xCu));
    gte().writeData(psx::gte::kRgb1, memory_.mem_r32(record + 0x10u));
    gte().writeData(psx::gte::kRgb2, memory_.mem_r32(record + 0x14u));
    gte().writeData(psx::gte::kRgbc, memory_.mem_r32(record + 0x14u));
    gte().execute(psx::gte::kDpct);
    memory_.mem_w32(packet + kColour[0], gte().readData(psx::gte::kRgb0));
    memory_.mem_w32(packet + kColour[1], gte().readData(psx::gte::kRgb1));
    memory_.mem_w32(packet + kColour[2], gte().readData(psx::gte::kRgb2));
    gte().writeData(psx::gte::kRgbc, memory_.mem_r32(record + 0x18u));
    gte().execute(psx::gte::kDpcs);
    memory_.mem_w32(packet + kColour[3], gte().readData(psx::gte::kRgb2));
  }

  psx::present::EmitMemory memory_;
  const ListCall &call_;
  std::uint32_t frame_;
};

// The records of the list, through the last.
std::uint32_t recordCount(Core &core, std::uint32_t list) {
  std::uint32_t count = 1;
  while (static_cast<std::int32_t>(core.mem_r32(list + (count - 1u) * PackedMesh::kRecordBytes + 4u)) > 0) {
    ++count;
  }
  return count;
}

class MeshStateProducer final : public ListStateProducer {
public:
  using ListStateProducer::ListStateProducer;

protected:
  void emit(const psx::present::EmitMemory &memory, std::uint32_t, const ListCall &call) const override {
    PackedMesh::emit(memory, call);
  }
};

} // namespace

void PackedMesh::write(Core *core) {
  const ListCall call{.list = core->r[4],
                      .ot = core->mem_r32(OrderingTable::kBasePointer),
                      .count = core->r[6],
                      .a3 = core->r[7],
                      .sp = core->r[29],
                      .a1 = core->r[5]};
  const EmitterObject object(core->emission, kWriter, call.list);
  const std::uint32_t records = recordCount(*core, call.list);
  ListJobWriter job(*core, 0u, call, records * kPacketBytes);
  job.input(*core, call.list, records * kRecordBytes);
  job.input(*core, kDepthCue, sizeof(std::uint32_t));
  emit(*core, call);
  job.save(*core);
}

void PackedMesh::emit(const psx::present::EmitMemory &memory, const ListCall &call) {
  const Mesh mesh(memory, call);
  const PacketPool pool(memory);
  std::uint32_t packet = pool.cursor();
  std::uint32_t record = call.list;
  for (std::uint32_t index = 0;; ++index, record += kRecordBytes) {
    const auto last = static_cast<std::int32_t>(memory.mem_r32(record + 4u)) <= 0;
    {
      const psx::present::ElementScope element(memory, modelElement(ModelList::PackedMesh, index));
      if (mesh.write({.record = record, .packet = packet})) {
        packet += kPacketBytes;
      }
    }
    if (last) {
      break;
    }
  }
  pool.setCursor(packet);
}

void PackedMesh::registerOverrides(native::NativeOverrideCatalog &natives) {
  natives.declare(kWriter, "PackedMesh::write", &PackedMesh::write);
}

void PackedMesh::registerStateRenders(psx::present::StateProducerTable &renders, Core &core) {
  renders.install(kWriter, std::make_unique<MeshStateProducer>(core));
}

} // namespace tomba::render
