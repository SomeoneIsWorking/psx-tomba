// game/render/dust_field.cpp — A0B FUN_801110BC. See dust_field.h.
#include "dust_field.h"

#include "camera_lattice.h"
#include "core.h"
#include "core/overrides/native_override_catalog.h"
#include "gte_registers.h"
#include "guest_abi.h"
#include "guest_ordering_table.h"
#include "horizontal_visibility_cull.h"
#include "list_job.h"
#include "list_state_producer.h"
#include "object_draw.h"

#include <cstdint>
#include <memory>

namespace tomba::render {
namespace {

constexpr std::uint32_t kEntry = 0x801110BCu;
constexpr GuestFrameSpill kSpills[] = {
    {31, 0x38}, {23, 0x34}, {22, 0x30}, {21, 0x2C}, {20, 0x28}, {19, 0x24}, {18, 0x20}, {17, 0x1C}, {16, 0x18}};
constexpr std::uint32_t kCornerLocal = 0x10u; // s16 x, y, z

constexpr std::uint32_t kMultiplier = 0x8011C030u;
constexpr std::uint32_t kPointVector = 0x1F8000C0u; // SVECTOR handed to RTPS
constexpr std::uint32_t kScratchDepth = 0x1F800080u;
constexpr std::uint32_t kPointCount = 0x201u;
constexpr std::uint32_t kRectWords = 3u;
constexpr std::uint32_t kRectBytes = 0x10u;
constexpr std::uint32_t kWhiteRect = 0x60FFFFFFu;
constexpr std::uint32_t kNearSize = 0x00020002u;
constexpr std::uint32_t kFarSize = 0x00010001u;
constexpr std::int32_t kNearDepth = 0x600;
constexpr std::uint32_t kBucket = 0x100u;

// The next lattice point from the seed, wrapped into the cube.
class PointStream {
public:
  PointStream(const psx::present::EmitMemory &memory, std::uint32_t seed, CameraLattice::Offset offset)
      : mMemory(memory), mSeed(seed), mOffset(offset) {}

  // Draws x, y and z, stepping between them; `lead` steps once before the first.
  void next(bool lead) {
    if (lead) {
      advance();
    }
    store(0u, mOffset.x);
    advance();
    store(2u, mOffset.y);
    advance();
    store(4u, mOffset.z);
  }
  void advance() {
    mSeed = CameraLattice::step(mMemory, mSeed, kMultiplier);
  }

private:
  void store(std::uint32_t lane, std::int32_t offset) const {
    const std::int32_t axis = (static_cast<std::int32_t>(mSeed) >> 16) + offset;
    mMemory.mem_w16(kPointVector + lane,
                    static_cast<std::uint16_t>(axis & static_cast<std::int32_t>(CameraLattice::kExtent)));
  }

  psx::present::EmitMemory mMemory;
  std::uint32_t mSeed;
  CameraLattice::Offset mOffset;
};

void drawPoints(const psx::present::EmitMemory &memory, const ListCall &call) {
  const std::uint32_t node = call.list;
  const CameraLattice::Point corner = CameraLattice::corner(memory);
  memory.mem_w16(call.sp + kCornerLocal, static_cast<std::uint16_t>(corner.x));
  memory.mem_w16(call.sp + kCornerLocal + 2u, static_cast<std::uint16_t>(corner.y));
  memory.mem_w16(call.sp + kCornerLocal + 4u, static_cast<std::uint16_t>(corner.z));
  const CameraLattice::Offset offset = CameraLattice::nodeOffset(memory, node, corner);
  CameraLattice::loadView(memory, corner);

  const horizontal_cull::Visibility visible = horizontal_cull::forDrawWindow(&memory.core());
  const OrderingTable ot(memory, call.ot);
  psx::cpu::Gte &gte = memory.core().gte();
  PointStream points(memory, memory.mem_r32(node + CameraLattice::kNodeSeed), offset);
  points.next(false);
  points.advance();
  const PacketPool pool(memory);
  std::uint32_t packet = pool.cursor();
  for (std::uint32_t point = 0; point < kPointCount; ++point) {
    gte.writeData(psx::gte::kVxy0, memory.mem_r32(kPointVector));
    gte.writeData(psx::gte::kVz0, memory.mem_r32(kPointVector + 4u));
    gte.execute(psx::gte::kRtps);
    points.next(point != 0u);
    memory.mem_w32(kScratchDepth, gte.readControl(psx::gte::kFlag));
    if (static_cast<std::int32_t>(memory.mem_r32(kScratchDepth)) < 0) {
      continue;
    }
    memory.mem_w32(packet + 8u, gte.projected(2).word());
    memory.mem_w32(kScratchDepth, gte.readData(psx::gte::kSz3));
    const std::uint32_t screenX = memory.mem_r16(packet + 8u);
    if (!visible.keepsX(&screenX, 1, horizontal_cull::Domain::Packed16)) {
      continue;
    }
    const bool near = static_cast<std::int32_t>(memory.mem_r32(kScratchDepth)) < kNearDepth;
    memory.mem_w32(packet + 0xCu, near ? kNearSize : kFarSize);
    memory.mem_w32(packet + 4u, kWhiteRect);
    ot.link({.packet = packet, .words = kRectWords, .bucket = kBucket});
    packet += kRectBytes;
  }
  pool.setCursor(packet);
}

class DustFieldRender final : public ListStateProducer {
public:
  using ListStateProducer::ListStateProducer;

protected:
  void emit(const psx::present::EmitMemory &memory, std::uint32_t, const ListCall &call) const override {
    drawPoints(memory, call);
  }
};

} // namespace

void DustField::draw(Core *c) {
  const std::uint32_t node = c->r[4];
  GuestFrame<0x40, 9> frame(c, kSpills);
  const psx::present::EmitMemory memory(*c);
  const std::uint32_t ot = OrderingTable::active(memory).base();
  const ListCall call{.list = node, .ot = ot, .count = 0, .a3 = 0, .sp = c->r[psx::cpu::kSp], .a1 = ot};
  const EmitterObject object(*c, kEntry, OwnObject{node});
  ListJobWriter job(*c, 0u, call, kPointCount * kRectBytes);
  job.input(*c, CameraLattice::kCameraMatrix, CameraLattice::kCameraBytes, InputBlend::Camera);
  for (std::uint32_t axis = 0; axis < 3u; ++axis) {
    job.input(*c, CameraLattice::kCameraPosition + axis * 4u, sizeof(std::int16_t), InputBlend::Lanes16);
  }
  job.input(*c, node + CameraLattice::kNodeOrigin, 3u * sizeof(std::int16_t), InputBlend::Lanes16);
  job.input(*c, node + CameraLattice::kNodeSeed, sizeof(std::uint32_t));
  job.input(*c, kMultiplier, sizeof(std::uint32_t));
  drawPoints(memory, call);
  job.save(*c);
}

void DustField::registerOverrides(native::NativeOverrideCatalog &natives) {
  natives.declareOverlay("A0B", kEntry, "DustField::draw", &DustField::draw);
}

void DustField::registerStateRenders(psx::present::StateProducerTable &renders, Core &core) {
  renders.install(kEntry, std::make_unique<DustFieldRender>(core));
}

} // namespace tomba::render
