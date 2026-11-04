// game/render/subdivided_ground.cpp — A03 FUN_80110290 with FUN_8010F968 and FUN_8010F1A0. See subdivided_ground.h.
#include "subdivided_ground.h"

#include "core.h"
#include "core/overrides/native_override_catalog.h"
#include "gte_registers.h"
#include "guest_ordering_table.h"
#include "horizontal_visibility_cull.h"
#include "list_state_producer.h"
#include "model_packet.h"
#include "object_draw.h"

#include <algorithm>
#include <array>
#include <memory>

namespace tomba::render {
namespace {

using horizontal_cull::Visibility;

constexpr std::uint32_t kEntry = 0x80110290u;
constexpr std::uint32_t kSplitCount = 0x8011C0E0u; // u32, quads split on screen this frame
constexpr std::uint32_t kLeafCount = 0x8011C0E4u;  // u32, model quads projected after subdividing

constexpr std::int32_t kNearestDepth = 0x384;
constexpr std::int32_t kSubdivideDepth = 0xDAC;
constexpr std::int32_t kSplitDepth = 0x157C;
constexpr std::int32_t kFogDepth = 0x1D4C;
constexpr std::int32_t kFogBucket = 0x600;
constexpr std::int32_t kBlackDepth = 0x514;
constexpr std::int32_t kFarPush = 0x5A;
constexpr std::int32_t kFirstSplitBucket = 6;
constexpr std::int32_t kSplitBucketEnd = 0x7D0;
constexpr int kSubdivideLevels = 2;

constexpr std::uint32_t kGt4Code = 0x3Cu;
constexpr std::uint32_t kSplitColourMask = 0xFEFEFEu;
constexpr std::uint32_t kClut = 0xAu;  // record u16, to packet +0xE
constexpr std::uint32_t kTpage = 0xEu; // record u16, to packet +0x1A

// A model corner as 0x8010F1A0 carries it.
struct Corner {
  std::int16_t x, y, z;
  std::uint8_t u, v, r, g, b;
};

std::uint8_t halfway(std::uint8_t a, std::uint8_t b) {
  return static_cast<std::uint8_t>((a + b) >> 1);
}

std::int16_t halfway(std::int16_t a, std::int16_t b) {
  return static_cast<std::int16_t>((a + b) >> 1);
}

Corner halfway(const Corner &a, const Corner &b) {
  return {halfway(a.x, b.x),
          halfway(a.y, b.y),
          halfway(a.z, b.z),
          halfway(a.u, b.u),
          halfway(a.v, b.v),
          halfway(a.r, b.r),
          halfway(a.g, b.g),
          halfway(a.b, b.b)};
}

std::uint32_t halfwayColour(std::uint32_t a, std::uint32_t b) {
  return ((a & kSplitColourMask) + (b & kSplitColourMask)) >> 1;
}

std::uint32_t halfwayScreen(std::uint32_t a, std::uint32_t b) {
  const auto x = halfway(static_cast<std::int16_t>(a), static_cast<std::int16_t>(b));
  const auto y = halfway(static_cast<std::int16_t>(a >> 16), static_cast<std::int16_t>(b >> 16));
  return (static_cast<std::uint32_t>(static_cast<std::uint16_t>(y)) << 16) | static_cast<std::uint16_t>(x);
}

std::uint16_t halfwayUv(std::uint16_t a, std::uint16_t b) {
  const auto u = halfway(static_cast<std::uint8_t>(a), static_cast<std::uint8_t>(b));
  const auto v = halfway(static_cast<std::uint8_t>(a >> 8), static_cast<std::uint8_t>(b >> 8));
  return static_cast<std::uint16_t>((v << 8) | u);
}

// The guest's bucket of a depth, kNoBucket outside [4, 0x7FF].
std::int32_t bucketOf(std::int32_t depth) {
  const std::int32_t bucket = OrderingTable::compressDepth(depth);
  return OrderingTable::inDepthRange(bucket) ? bucket : OrderingTable::kNoBucket;
}

bool splitBucket(std::int32_t bucket) {
  return bucket >= kFirstSplitBucket && bucket < kSplitBucketEnd;
}

std::int32_t farthest(const std::array<std::int32_t, 4> &depths) {
  return std::ranges::max(depths);
}

void count(const psx::present::EmitMemory &memory, std::uint32_t counter) {
  memory.mem_w32(counter, memory.mem_r32(counter) + 1u);
}

// What every packet of one record shares.
struct Quad {
  const psx::present::EmitMemory &memory;
  std::uint32_t record;
  const OrderingTable &ot;

  void link(std::uint32_t packet, std::int32_t bucket) const {
    ot.link({.packet = packet, .words = kModelGt4.packetWords, .bucket = static_cast<std::uint32_t>(bucket)});
  }
  void stampModes(std::uint32_t packet) const {
    memory.mem_w16(packet + ModelPacket::kUv[0] + 2u, memory.mem_r16(record + kClut));
    memory.mem_w8(packet + ModelPacket::kColour[0] + 3u, static_cast<std::uint8_t>(kGt4Code));
    memory.mem_w16(packet + ModelPacket::kUv[1] + 2u, memory.mem_r16(record + kTpage));
  }
};

// 0x8010F1A0's leaf: RotTransPers4 of the corners, culled and linked at the farthest corner's bucket.
std::uint32_t
projectSubdivided(const Quad &quad, const Visibility &visible, std::uint32_t packet, const std::array<Corner, 4> &c) {
  const psx::present::EmitMemory &memory = quad.memory;
  psx::cpu::Gte &gte = memory.core().gte();
  count(memory, kLeafCount);
  const auto xy = [](const Corner &corner) {
    return (static_cast<std::uint32_t>(static_cast<std::uint16_t>(corner.y)) << 16) |
           static_cast<std::uint16_t>(corner.x);
  };
  gte.writeData(psx::gte::kVxy0, xy(c[0]));
  gte.writeData(psx::gte::kVz0, static_cast<std::uint16_t>(c[0].z));
  gte.writeData(psx::gte::kVxy1, xy(c[1]));
  gte.writeData(psx::gte::kVz1, static_cast<std::uint16_t>(c[1].z));
  gte.writeData(psx::gte::kVxy2, xy(c[2]));
  gte.writeData(psx::gte::kVz2, static_cast<std::uint16_t>(c[2].z));
  gte.execute(psx::gte::kRtpt);
  for (std::uint32_t corner = 0; corner < 3u; ++corner) {
    memory.storeVertex(packet + ModelPacket::kScreen[corner], gte.projected(corner));
  }
  std::uint32_t flag = gte.readControl(psx::gte::kFlag);
  gte.writeData(psx::gte::kVxy0, xy(c[3]));
  gte.writeData(psx::gte::kVz0, static_cast<std::uint16_t>(c[3].z));
  gte.execute(psx::gte::kRtps);
  memory.storeVertex(packet + ModelPacket::kScreen[3], gte.projected(2));
  flag |= gte.readControl(psx::gte::kFlag);
  const ModelPacket model(memory, kModelGt4, {.record = quad.record, .packet = packet}, kOverlayStage);
  if (!model.onScreen(visible)) {
    return packet;
  }
  std::array<std::int32_t, 4> depths{};
  for (std::uint32_t corner = 0; corner < 4u; ++corner) {
    depths[corner] = static_cast<std::int32_t>(gte.readData(psx::gte::kSz0 + corner));
  }
  const std::int32_t bucket = bucketOf(farthest(depths) >> 2);
  if (static_cast<std::int32_t>(flag) < 0 || !splitBucket(bucket)) {
    return packet;
  }
  quad.stampModes(packet);
  for (std::uint32_t corner = 0; corner < 4u; ++corner) {
    const Corner &from = c[corner];
    const bool black = depths[corner] < kBlackDepth;
    const std::uint32_t colour = packet + ModelPacket::kColour[corner];
    memory.mem_w8(colour + 0u, black ? 0u : from.r);
    memory.mem_w8(colour + 1u, black ? 0u : from.g);
    memory.mem_w8(colour + 2u, black ? 0u : from.b);
    memory.mem_w8(packet + ModelPacket::kUv[corner] + 0u, from.u);
    memory.mem_w8(packet + ModelPacket::kUv[corner] + 1u, from.v);
  }
  quad.link(packet, bucket);
  return packet + kModelGt4.packetBytes;
}

// 0x8010F1A0: halves the model quad `levels` times; the centre is taken between the 0-2 and 2-3 edge midpoints.
std::uint32_t subdivide(
    const Quad &quad, const Visibility &visible, std::uint32_t packet, int levels, const std::array<Corner, 4> &c) {
  if (levels == 0) {
    return projectSubdivided(quad, visible, packet, c);
  }
  const Corner top = halfway(c[0], c[1]);
  const Corner bottom = halfway(c[2], c[3]);
  const Corner left = halfway(c[0], c[2]);
  const Corner right = halfway(c[1], c[3]);
  const Corner centre = halfway(bottom, left);
  packet = subdivide(quad, visible, packet, levels - 1, {c[0], top, left, centre});
  packet = subdivide(quad, visible, packet, levels - 1, {top, c[1], centre, right});
  packet = subdivide(quad, visible, packet, levels - 1, {left, centre, c[2], bottom});
  return subdivide(quad, visible, packet, levels - 1, {centre, right, bottom, c[3]});
}

// The record's corners as 0x80110290 hands them to 0x8010F1A0.
std::array<Corner, 4> modelCorners(const psx::present::EmitMemory &memory, std::uint32_t record) {
  constexpr std::array<std::uint32_t, 4> kUvAt{8u, 0xCu, 0x10u, 0x12u};
  std::array<Corner, 4> corners{};
  for (std::size_t corner = 0; corner < corners.size(); ++corner) {
    const ModelCorner &at = kModelGt4.corners[corner];
    const std::uint32_t rgb = record + (corner < 2u ? 0u : 4u);
    const auto channel = [&](std::uint32_t lane) {
      const std::uint32_t value = memory.mem_r8(rgb + lane);
      return static_cast<std::uint8_t>(corner % 2u == 0u ? value & 0xF0u : value << 4);
    };
    corners[corner] = {static_cast<std::int16_t>(memory.mem_r16(record + at.x)),
                       static_cast<std::int16_t>(memory.mem_r16(record + at.y)),
                       static_cast<std::int16_t>(memory.mem_r16(record + at.z)),
                       memory.mem_r8(record + kUvAt[corner]),
                       memory.mem_r8(record + kUvAt[corner] + 1u),
                       channel(0u),
                       channel(1u),
                       channel(2u)};
  }
  return corners;
}

// 0x8010F968: the projected quad, faded by 0xDAC - SZ when `fade`, split into four on screen.
std::uint32_t
splitOnScreen(const Quad &quad, std::uint32_t packet, const std::array<std::int32_t, 4> &depths, bool fade) {
  const psx::present::EmitMemory &memory = quad.memory;
  const ModelPacket model(memory, kModelGt4, {.record = quad.record, .packet = packet}, kOverlayStage);
  std::array<std::uint32_t, 4> screen{};
  std::array<std::uint16_t, 4> uv{};
  for (std::size_t corner = 0; corner < 4u; ++corner) {
    screen[corner] = memory.mem_r32(packet + ModelPacket::kScreen[corner]);
    uv[corner] = memory.mem_r16(packet + ModelPacket::kUv[corner]);
  }
  if (fade) {
    for (int corner = 0; corner < 4; ++corner) {
      model.shade({.corner = corner, .intensity = kSubdivideDepth - depths[static_cast<std::size_t>(corner)]});
    }
  }
  std::array<std::uint32_t, 4> colour{};
  for (std::size_t corner = 0; corner < 4u; ++corner) {
    colour[corner] = memory.mem_r32(packet + ModelPacket::kColour[corner]);
  }
  count(memory, kSplitCount);
  const std::int32_t bucket = bucketOf(farthest(depths) >> 2);
  if (!splitBucket(bucket)) {
    return packet;
  }
  const std::uint32_t top = halfwayScreen(screen[0], screen[1]);
  const std::uint32_t bottom = halfwayScreen(screen[2], screen[3]);
  const std::uint32_t left = halfwayScreen(screen[0], screen[2]);
  const std::uint32_t right = halfwayScreen(screen[1], screen[3]);
  const std::uint32_t centre = halfwayScreen(top, bottom);
  const std::uint16_t uvTop = halfwayUv(uv[0], uv[1]);
  const std::uint16_t uvBottom = halfwayUv(uv[2], uv[3]);
  const std::uint16_t uvLeft = halfwayUv(uv[0], uv[2]);
  const std::uint16_t uvRight = halfwayUv(uv[1], uv[3]);
  const std::uint16_t uvCentre = halfwayUv(uvTop, uvBottom);
  const std::uint32_t colourTop = halfwayColour(colour[0], colour[1]);
  const std::uint32_t colourBottom = halfwayColour(colour[2], colour[3]);
  const std::uint32_t colourLeft = halfwayColour(colour[0], colour[2]);
  const std::uint32_t colourRight = halfwayColour(colour[1], colour[3]);
  const std::uint32_t colourCentre = halfwayColour(colourTop, colourBottom);
  struct Point {
    std::uint32_t screen;
    std::uint16_t uv;
    std::uint32_t colour;
  };
  const std::array<std::array<Point, 4>, 4> quads{{
      {{{screen[0], uv[0], colour[0]},
        {top, uvTop, colourTop},
        {left, uvLeft, colourLeft},
        {centre, uvCentre, colourCentre}}},
      {{{screen[1], uv[1], colour[1]},
        {top, uvTop, colourTop},
        {right, uvRight, colourRight},
        {centre, uvCentre, colourCentre}}},
      {{{screen[3], uv[3], colour[3]},
        {bottom, uvBottom, colourBottom},
        {right, uvRight, colourRight},
        {centre, uvCentre, colourCentre}}},
      {{{screen[2], uv[2], colour[2]},
        {bottom, uvBottom, colourBottom},
        {left, uvLeft, colourLeft},
        {centre, uvCentre, colourCentre}}},
  }};
  for (const auto &points : quads) {
    for (std::size_t corner = 0; corner < 4u; ++corner) {
      memory.mem_w32(packet + ModelPacket::kScreen[corner], points[corner].screen);
      memory.mem_w16(packet + ModelPacket::kUv[corner], points[corner].uv);
      memory.mem_w32(packet + ModelPacket::kColour[corner], points[corner].colour);
    }
    quad.stampModes(packet);
    quad.link(packet, bucket);
    packet += kModelGt4.packetBytes;
  }
  return packet;
}

// The averaged path: the bucket stage, fogged past kFogBucket, linked.
std::uint32_t linkAveraged(const Quad &quad,
                           const ModelPacket &model,
                           const std::array<std::int32_t, 4> &depths,
                           std::int32_t bucket) {
  quad.memory.mem_w32(kOverlayStage.bucket, static_cast<std::uint32_t>(bucket));
  if (bucket > kFogBucket) {
    for (int corner = 0; corner < 4; ++corner) {
      model.shade({.corner = corner, .intensity = depths[static_cast<std::size_t>(corner)] - kFogDepth});
    }
  }
  // An out-of-range bucket links one slot before the table, which no walk reaches; a render has no such slot.
  if (bucket != OrderingTable::kNoBucket || !quad.memory.hosted()) {
    quad.link(model.packet(), bucket);
  }
  return model.packet() + kModelGt4.packetBytes;
}

std::uint32_t emitRecord(const psx::present::EmitMemory &memory,
                         const OrderingTable &ot,
                         const Visibility &visible,
                         const ModelPacket &model) {
  psx::cpu::Gte &gte = memory.core().gte();
  const std::uint32_t record = model.record();
  const std::uint32_t packet = model.packet();
  model.loadTriangle();
  const std::uint32_t colour01 = memory.mem_r32(record);
  memory.mem_w32(packet + ModelPacket::kColour[0], colour01 & kColourCodeMask);
  gte.execute(psx::gte::kRtpt);
  memory.mem_w32(packet + ModelPacket::kColour[1], (colour01 << 4) & kColourMask);
  memory.mem_w32(kOverlayStage.gte, gte.readControl(psx::gte::kFlag));
  model.storeTriangle();
  const std::uint32_t colour23 = memory.mem_r32(record + 4u);
  memory.mem_w32(packet + ModelPacket::kColour[2], colour23 & kColourMask);
  model.loadCornerThree();
  memory.mem_w32(packet + ModelPacket::kColour[3], (colour23 << 4) & kColourMask);
  gte.execute(psx::gte::kRtps);
  memory.mem_w32(packet + ModelPacket::kUv[0], memory.mem_r32(record + 8u));
  memory.mem_w32(kOverlayStage.gte, gte.readControl(psx::gte::kFlag));
  memory.storeVertex(packet + ModelPacket::kScreen[3], gte.projected(2));
  if (!model.onScreen(visible)) {
    return packet;
  }
  memory.mem_w32(packet + ModelPacket::kUv[1], memory.mem_r32(record + 0xCu));
  std::array<std::int32_t, 4> depths{};
  for (std::uint32_t corner = 0; corner < 4u; ++corner) {
    depths[corner] = static_cast<std::int32_t>(gte.readData(psx::gte::kSz0 + corner));
  }
  model.storeLastUv();
  const std::int32_t far = farthest(depths);
  if (far < kNearestDepth) {
    return packet;
  }
  const Quad quad{memory, record, ot};
  const bool subdivided = (model.flags() & 1u) != 0u;
  if (!subdivided) {
    if (far < kSubdivideDepth) {
      return splitOnScreen(quad, packet, depths, true);
    }
    gte.execute(psx::gte::kAvsz4);
    std::int32_t bucket = bucketOf(static_cast<std::int32_t>(gte.readData(psx::gte::kOtz)));
    if (bucket < 5) {
      return packet;
    }
    bucket = std::min(bucket + kFarPush, OrderingTable::kFarthestBucket);
    return linkAveraged(quad, model, depths, bucket);
  }
  if (far < kSubdivideDepth) {
    return subdivide(quad, visible, packet, kSubdivideLevels, modelCorners(memory, record));
  }
  if (far < kSplitDepth) {
    return splitOnScreen(quad, packet, depths, false);
  }
  gte.execute(psx::gte::kAvsz4);
  return linkAveraged(quad, model, depths, bucketOf(static_cast<std::int32_t>(gte.readData(psx::gte::kOtz))));
}

class SubdividedGroundRender final : public ListStateProducer {
public:
  using ListStateProducer::ListStateProducer;

protected:
  void emit(const psx::present::EmitMemory &memory, std::uint32_t, const ListCall &call) const override {
    SubdividedGround::emit(memory, call, horizontal_cull::forDrawWindow(&core()));
  }
};

// Sixteen packets for a record subdivided twice.
constexpr std::uint32_t kMostPacketsPerRecord = 16u;

} // namespace

std::uint32_t
SubdividedGround::emit(const psx::present::EmitMemory &memory, const ListCall &call, const Visibility &visible) {
  const OrderingTable ot(memory, call.ot);
  return emitModelList(memory, call, kModelGt4, kOverlayStage, [&](const ModelPacket &model) {
    return emitRecord(memory, ot, visible, model);
  });
}

void SubdividedGround::draw(Core *c) {
  const ListCall call = ListCall::fromRegisters(*c);
  const EmitterObject object(*c, kEntry, call.list);
  ListJobWriter job(*c, 0u, call, call.count * kModelGt4.packetBytes * kMostPacketsPerRecord);
  job.input(*c, call.list, call.count * kModelGt4.recordBytes);
  job.input(*c, kSplitCount, sizeof(std::uint32_t));
  job.input(*c, kLeafCount, sizeof(std::uint32_t));
  c->r[2] = emit(*c, call, horizontal_cull::forDrawWindow(c));
  job.save(*c);
}

void SubdividedGround::registerOverrides(native::NativeOverrideCatalog &natives) {
  natives.declareOverlay("A03", kEntry, "SubdividedGround::draw", &SubdividedGround::draw);
}

void SubdividedGround::registerStateRenders(psx::present::StateProducerTable &renders, Core &core) {
  renders.install(kEntry, std::make_unique<SubdividedGroundRender>(core));
}

} // namespace tomba::render
