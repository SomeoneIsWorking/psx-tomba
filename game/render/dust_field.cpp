// game/render/dust_field.cpp — A0B FUN_801110BC and A04 FUN_8013AD90. See dust_field.h.
#include "dust_field.h"

#include "camera_lattice.h"
#include "core.h"
#include "core/overrides/native_override_catalog.h"
#include "gte_registers.h"
#include "guest_abi.h"
#include "guest_ordering_table.h"
#include "horizontal_visibility_cull.h"
#include "list_job.h"
#include "object_draw.h"

#include <cstdint>

namespace tomba::render {
namespace {

constexpr std::uint32_t kCornerLocal = 0x10u;       // s16 x, y, z
constexpr std::uint32_t kPointVector = 0x1F8000C0u; // SVECTOR handed to RTPS
constexpr std::uint32_t kScratchDepth = 0x1F800080u;
constexpr std::uint32_t kRectWords = 3u;
constexpr std::uint32_t kRectBytes = 0x10u;
constexpr std::uint32_t kWhiteRect = 0x60FFFFFFu;
constexpr std::uint32_t kNearSize = 0x00020002u;
constexpr std::uint32_t kFarSize = 0x00010001u;
constexpr std::int32_t kNearDepth = 0x600;
constexpr std::uint32_t kBucket = 0x100u;

constexpr std::uint32_t kDustEntry = 0x801110BCu;
constexpr GuestFrameSpill kDustSpills[] = {
    {31, 0x38}, {23, 0x34}, {22, 0x30}, {21, 0x2C}, {20, 0x28}, {19, 0x24}, {18, 0x20}, {17, 0x1C}, {16, 0x18}};
constexpr std::uint32_t kDustMultiplier = 0x8011C030u;
constexpr std::uint32_t kDustCount = 0x201u;

constexpr std::uint32_t kSnowEntry = 0x8013AD90u;
constexpr GuestFrameSpill kSnowSpills[] = {{31, 0x44},
                                           {30, 0x40},
                                           {23, 0x3C},
                                           {22, 0x38},
                                           {21, 0x34},
                                           {20, 0x30},
                                           {19, 0x2C},
                                           {18, 0x28},
                                           {17, 0x24},
                                           {16, 0x20}};
constexpr std::uint32_t kSnowSeedLocal = 0x18u; // node + 0x50
constexpr std::uint32_t kSnowSeed = 0x0012D687u;
constexpr std::uint32_t kSnowMultiplier = 0x7D2B89DDu;
constexpr std::uint32_t kSnowCount = 0x52u; // s16 at node: last point index
constexpr std::uint32_t kSnowPoolReserve = 0x8000u;
// A snow point is kept only inside the cave mouth: near in z, or low and not far.
constexpr std::int32_t kSnowNearZ = 0x2711;
constexpr std::int32_t kSnowLowY = -0x2327;
constexpr std::int32_t kSnowFarZ = 0x2CED;

enum Variant : std::uint8_t { kDust = 0, kSnow = 1 };

// The next lattice point from the seed, wrapped into the cube.
class PointStream {
public:
  PointStream(std::uint32_t seed, std::uint32_t multiplier, CameraLattice::Offset offset)
      : mSeed(seed), mMultiplier(multiplier), mOffset(offset) {}

  // Draws x, y and z, stepping between them; `lead` steps once before the first.
  CameraLattice::Point next(bool lead) {
    if (lead) {
      advance();
    }
    const std::int16_t x = wrap(mOffset.x);
    advance();
    const std::int16_t y = wrap(mOffset.y);
    advance();
    return {x, y, wrap(mOffset.z)};
  }
  void advance() {
    mSeed = CameraLattice::step(mSeed, mMultiplier);
  }

private:
  std::int16_t wrap(std::int32_t offset) const {
    return static_cast<std::int16_t>(((static_cast<std::int32_t>(mSeed) >> 16) + offset) &
                                     static_cast<std::int32_t>(CameraLattice::kExtent));
  }

  std::uint32_t mSeed;
  std::uint32_t mMultiplier;
  CameraLattice::Offset mOffset;
};

void storePoint(const psx::present::EmitMemory &memory, CameraLattice::Point point) {
  memory.mem_w16(kPointVector + 0u, static_cast<std::uint16_t>(point.x));
  memory.mem_w16(kPointVector + 2u, static_cast<std::uint16_t>(point.y));
  memory.mem_w16(kPointVector + 4u, static_cast<std::uint16_t>(point.z));
}

bool insideCaveMouth(CameraLattice::Point corner, CameraLattice::Point point) {
  const std::int32_t z = corner.z + point.z;
  return z < kSnowNearZ || (corner.y + point.y < kSnowLowY && z < kSnowFarZ);
}

void drawPoints(const psx::present::EmitMemory &memory, Variant variant, const ListCall &call) {
  const std::uint32_t node = call.list;
  const CameraLattice::Point corner = CameraLattice::corner(memory);
  memory.mem_w16(call.sp + kCornerLocal, static_cast<std::uint16_t>(corner.x));
  memory.mem_w16(call.sp + kCornerLocal + 2u, static_cast<std::uint16_t>(corner.y));
  memory.mem_w16(call.sp + kCornerLocal + 4u, static_cast<std::uint16_t>(corner.z));
  const CameraLattice::Offset offset = CameraLattice::nodeOffset(memory, node, corner);
  CameraLattice::loadView(memory, corner);

  const bool snow = variant == kSnow;
  PointStream points =
      snow ? PointStream(kSnowSeed, kSnowMultiplier, offset)
           : PointStream(memory.mem_r32(node + CameraLattice::kNodeSeed), memory.mem_r32(kDustMultiplier), offset);
  storePoint(memory, points.next(false));
  points.advance();
  const std::int32_t count = snow ? memory.mem_r16s(node + kSnowCount) + 1 : static_cast<std::int32_t>(kDustCount);

  const horizontal_cull::Visibility visible = horizontal_cull::forDrawWindow(&memory.core());
  const OrderingTable ot(memory, call.ot);
  psx::cpu::Gte &gte = memory.core().gte();
  const PacketPool pool(memory);
  std::uint32_t packet = pool.cursor();
  for (std::int32_t point = 0; point < count; ++point) {
    gte.writeData(psx::gte::kVxy0, memory.mem_r32(kPointVector));
    gte.writeData(psx::gte::kVz0, memory.mem_r32(kPointVector + 4u));
    gte.execute(psx::gte::kRtps);
    const CameraLattice::Point next = points.next(snow || point != 0);
    if (snow && !insideCaveMouth(corner, next)) {
      continue;
    }
    storePoint(memory, next);
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

ListCall callOf(Core &core) {
  const std::uint32_t ot = OrderingTable::active(psx::present::EmitMemory(core)).base();
  return {.list = core.r[4], .ot = ot, .count = 0, .a3 = 0, .sp = core.r[psx::cpu::kSp], .a1 = ot};
}

} // namespace

void DustField::draw(Core *c) {
  GuestFrame<0x40, 9> frame(c, kDustSpills);
  const ListCall call = callOf(*c);
  const EmitterObject object(*c, kDustEntry, OwnObject{call.list});
  drawPoints(psx::present::EmitMemory(*c), kDust, call);
}

void DustField::drawSnow(Core *c) {
  GuestFrame<0x48, 10> frame(c, kSnowSpills);
  const ListCall call = callOf(*c);
  c->mem_w32(call.sp + kSnowSeedLocal, call.list + CameraLattice::kNodeSeed);
  const psx::present::EmitMemory memory(*c);
  if (!PacketPool(memory).hasRoom(kSnowPoolReserve)) {
    return;
  }
  const EmitterObject object(*c, kSnowEntry, OwnObject{call.list});
  drawPoints(memory, kSnow, call);
}

void DustField::registerOverrides(native::NativeOverrideCatalog &natives) {
  natives.declareOverlay("A0B", kDustEntry, "DustField::draw", &DustField::draw);
  natives.declareOverlay("A04", kSnowEntry, "DustField::drawSnow", &DustField::drawSnow);
}

} // namespace tomba::render
