// game/render/packed_mesh.cpp — PackedMesh. See packed_mesh.h.
#include "packed_mesh.h"

#include "core.h"
#include "core/overrides/native_override_catalog.h"
#include "entry/session.h"
#include "fx_sprite_publish.h"
#include "gte_registers.h"
#include "guest_ordering_table.h"
#include "list_state_producer.h"
#include "model_element.h"
#include "ordering_table.h"
#include "quad_state.h"

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
constexpr std::uint32_t kColour[4] = {4u, 0x10u, 0x1Cu, 0x28u};
constexpr std::uint32_t kLastWord = 4u; // word 1 not positive ends a list

// Record byte offsets of each corner's x, y and z.
struct CornerBytes {
  std::uint32_t x, y, z;
};
constexpr CornerBytes kCorners[4] = {
    {0x1Cu, 0x1Eu, 0x0Fu}, {0x1Du, 0x1Fu, 0x13u}, {0x20u, 0x22u, 0x17u}, {0x21u, 0x23u, 0x1Bu}};

class Mesh {
public:
  Mesh(const psx::present::EmitMemory &memory, PackedMesh::Variant variant, const ListCall &call)
      : memory_(memory), call_(call), frame_(call.sp - kFrameBytes),
        clutRows_(variant == PackedMesh::Variant::ClutRows ? call.a1 : 0u),
        vOffset_(variant == PackedMesh::Variant::VOffset ? call.a1 : 0u) {}

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
      memory_.storeVertex(packet + kScreen[corner], gte().projected(corner));
    }
    loadLastCorner();
    PackedRecord::uv(memory_, {.record = record, .packet = packet, .corner = 0, .clutRows = clutRows_});
    gte().execute(psx::gte::kRtps);
    if (overflowed()) {
      return false;
    }
    memory_.storeVertex(packet + kScreen[3], gte().projected(2));
    PackedRecord::uv(memory_, {.record = record, .packet = packet, .corner = 1, .clutRows = clutRows_});
    const std::int32_t bucket = depthBucket();
    if (bucket < 0) {
      return false;
    }
    PackedRecord::uv(memory_, {.record = record, .packet = packet, .corner = 2, .clutRows = clutRows_});
    PackedRecord::uv(memory_, {.record = record, .packet = packet, .corner = 3, .clutRows = clutRows_});
    for (const std::uint32_t uv : kUv) {
      memory_.mem_w8(packet + uv, static_cast<std::uint8_t>(memory_.mem_r8(packet + uv) + call_.a3));
      memory_.mem_w8(packet + uv + 1u, static_cast<std::uint8_t>(memory_.mem_r8(packet + uv + 1u) + vOffset_));
    }
    PackedRecord::shade(memory_, record, packet);
    memory_.mem_w8(packet + 7u, (texturePage & kSemiBit) != 0u ? kGouraudQuadSemi : kGouraudQuad);
    OrderingTable(memory_, call_.ot)
        .link({.packet = packet, .words = kPacketWords, .bucket = static_cast<std::uint32_t>(bucket)});
    return true;
  }

private:
  static constexpr const std::uint32_t (&kScreen)[4] = PackedRecord::kPacketScreen;
  static constexpr const std::uint32_t (&kUv)[4] = PackedRecord::kPacketUv;

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

  psx::present::EmitMemory memory_;
  const ListCall &call_;
  std::uint32_t frame_;
  std::uint32_t clutRows_;
  std::uint32_t vOffset_;
};

class MeshStateProducer final : public ListStateProducer {
public:
  using ListStateProducer::ListStateProducer;

protected:
  void emit(const psx::present::EmitMemory &memory, std::uint32_t variant, const ListCall &call) const override {
    PackedMesh::emit(memory, static_cast<PackedMesh::Variant>(variant), call);
  }
};

} // namespace

void PackedRecord::uv(const psx::present::EmitMemory &memory, const Uv &uv) {
  const auto [record, packet, corner, clutRows] = uv;
  std::uint32_t word = 0;
  switch (corner) {
  case 0:
    word = memory.mem_r32(record) + (clutRows << kClutRowShift);
    break;
  case 1:
    word = memory.mem_r32(record + 4u) & kTexturePageMask;
    break;
  case 2:
    word = memory.mem_r32(record + 8u);
    break;
  default:
    word = static_cast<std::uint32_t>(static_cast<std::int32_t>(memory.mem_r32(record + 8u)) >> 16);
    break;
  }
  memory.mem_w32(packet + kPacketUv[corner], word);
}

void PackedRecord::shade(const psx::present::EmitMemory &memory, std::uint32_t record, std::uint32_t packet) {
  psx::cpu::Gte &gte = memory.core().gte();
  gte.writeData(psx::gte::kIr0, memory.mem_r32(PackedMesh::kDepthCue));
  gte.writeData(psx::gte::kRgb0, memory.mem_r32(record + 0xCu));
  gte.writeData(psx::gte::kRgb1, memory.mem_r32(record + 0x10u));
  gte.writeData(psx::gte::kRgb2, memory.mem_r32(record + 0x14u));
  gte.writeData(psx::gte::kRgbc, memory.mem_r32(record + 0x14u));
  gte.execute(psx::gte::kDpct);
  memory.mem_w32(packet + kColour[0], gte.readData(psx::gte::kRgb0));
  memory.mem_w32(packet + kColour[1], gte.readData(psx::gte::kRgb1));
  memory.mem_w32(packet + kColour[2], gte.readData(psx::gte::kRgb2));
  gte.writeData(psx::gte::kRgbc, memory.mem_r32(record + 0x18u));
  gte.execute(psx::gte::kDpcs);
  memory.mem_w32(packet + kColour[3], gte.readData(psx::gte::kRgb2));
}

std::uint32_t PackedRecord::count(const psx::present::EmitMemory &memory, std::uint32_t list) {
  std::uint32_t count = 1;
  while (static_cast<std::int32_t>(memory.mem_r32(list + (count - 1u) * PackedMesh::kRecordBytes + kLastWord)) > 0) {
    ++count;
  }
  return count;
}

void PackedMesh::write(Core *core) {
  const ListCall call{.list = core->r[4],
                      .ot = core->mem_r32(OrderingTable::kBasePointer),
                      .count = core->r[6],
                      .a3 = core->r[7],
                      .sp = core->r[29],
                      .a1 = core->r[5]};
  // Unscoped, the call is its own object: SOP 0x8010BF54 draws one list twice a frame.
  const EmitterObject object(
      *core, kWriter, core->emission.isOpen() ? call.list : sessionOf(core).callSites.object(core->r[31], call.list));
  writeSaving(*core, Variant::ClutRows, call);
}

void PackedMesh::writeSaving(Core &core, Variant variant, const ListCall &call) {
  const std::uint32_t records = PackedRecord::count(core, call.list);
  ListJobWriter job(core, static_cast<std::uint32_t>(variant), call, records * kPacketBytes);
  job.input(core, call.list, records * kRecordBytes);
  job.input(core, kDepthCue, sizeof(std::uint32_t));
  emit(core, variant, call);
  job.save(core);
}

void PackedMesh::emit(const psx::present::EmitMemory &memory, Variant variant, const ListCall &call) {
  const Mesh mesh(memory, variant, call);
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

void PackedMesh::emitScreen(const psx::present::EmitMemory &memory, const ScreenCall &call) {
  const PacketPool pool(memory);
  const FxSpritePublish publish{memory};
  const psx::gte::ProjectedVertex anchor = memory.loadVertex(fxpublish::kScreenXY);
  const auto scaleX = static_cast<std::int32_t>(memory.mem_r32(fxpublish::kScaleX));
  const auto scaleY = static_cast<std::int32_t>(memory.mem_r32(fxpublish::kScaleY));
  const auto bucket = static_cast<std::uint32_t>(memory.mem_r32(fxpublish::kOtKey));
  std::uint32_t packet = pool.cursor();
  for (std::uint32_t record = call.list;; record += kRecordBytes, packet += kPacketBytes) {
    for (std::uint32_t corner = 0; corner < 4u; ++corner) {
      memory.storeVertex(packet + PackedRecord::kPacketScreen[corner],
                         publish.scaledCorner({.anchor = anchor,
                                               .x = memory.mem_r8s(record + kCorners[corner].x),
                                               .y = memory.mem_r8s(record + kCorners[corner].y),
                                               .scaleX = scaleX,
                                               .scaleY = scaleY}));
    }
    for (std::uint32_t corner = 0; corner < 4u; ++corner) {
      PackedRecord::uv(memory, {.record = record, .packet = packet, .corner = corner, .clutRows = call.clutRows});
    }
    PackedRecord::shade(memory, record, packet);
    memory.mem_w8(packet + 7u, kGouraudQuadSemi);
    OrderingTable(memory, call.ot).link({.packet = packet, .words = kPacketWords, .bucket = bucket});
    if (static_cast<std::int32_t>(memory.mem_r32(record + kLastWord)) <= 0) {
      break;
    }
  }
  pool.setCursor(packet + kPacketBytes);
}

void PackedMesh::registerOverrides(native::NativeOverrideCatalog &natives) {
  natives.declare(kWriter, "PackedMesh::write", &PackedMesh::write);
  natives.declareOverlay("A08", kVOffsetWriter, "PackedMesh::writeVOffset", [](Core *core) {
    const EmitterObject object(*core, kWriter, core->r[4]);
    writeSaving(*core,
                Variant::VOffset,
                {.list = core->r[4],
                 .ot = core->mem_r32(OrderingTable::kBasePointer),
                 .count = core->r[6],
                 .a3 = core->r[7],
                 .sp = core->r[29],
                 .a1 = core->r[5]});
  });
  natives.declare(kScreenWriter, "PackedMesh::writeScreen", [](Core *core) {
    const std::uint32_t object =
        core->emission.isOpen() ? core->r[4] : sessionOf(core).callSites.object(core->r[31], core->r[4]);
    const psx::present::EmissionScope::Guard scope(core->emission, kScreenWriter, object, 0);
    const std::uint32_t before = PacketPool(*core).cursor();
    emitScreen(*core, {.list = core->r[4], .clutRows = core->r[5], .ot = core->mem_r32(OrderingTable::kBasePointer)});
    saveQuads(*core,
              {.emitter = kScreenWriter, .object = object, .before = before, .after = PacketPool(*core).cursor()});
  });
}

void PackedMesh::registerStateRenders(psx::present::StateProducerTable &renders, Core &core) {
  renders.install(kWriter, std::make_unique<MeshStateProducer>(core));
  renders.install(kScreenWriter, std::make_unique<QuadStateRender>());
}

} // namespace tomba::render
