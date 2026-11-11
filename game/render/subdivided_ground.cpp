// game/render/subdivided_ground.cpp — A03 FUN_80110290 with FUN_8010F968 and FUN_8010F1A0. See subdivided_ground.h.
#include "subdivided_ground.h"

#include "core.h"
#include "core/overrides/native_override_catalog.h"
#include "entry/session.h"
#include "gte_registers.h"
#include "guest_ordering_table.h"
#include "horizontal_visibility_cull.h"
#include "list_state_producer.h"
#include "model_packet.h"
#include "object_draw.h"
#include "view_clip.h"
#include "wide_window.h"

#include <lucent/log.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

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

std::uint8_t halfway(std::uint8_t a, std::uint8_t b) {
  return static_cast<std::uint8_t>((a + b) >> 1);
}

std::int16_t halfway(std::int16_t a, std::int16_t b) {
  return static_cast<std::int16_t>((a + b) >> 1);
}

ModelPoint halfway(const ModelPoint &a, const ModelPoint &b) {
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

bool splitBucket(std::int32_t bucket) {
  return bucket >= kFirstSplitBucket && bucket < kSplitBucketEnd;
}

// The split and subdivided leaves' bucket: the farthest corner, compressed with A03's band bias.
std::int32_t splitBucketOf(const std::array<std::int32_t, 4> &depths) {
  return OrderingTable::bucketOf(OrderingTable::extremeDepth(true, depths), kA03BandBias);
}

void count(const psx::present::EmitMemory &memory, std::uint32_t counter) {
  memory.mem_w32(counter, memory.mem_r32(counter) + 1u);
}

// How a path colours a corner by its depth.
enum class Tint : std::uint8_t {
  Plain,
  Fade,  // toward the far colour by kSubdivideDepth - SZ (0x8010F968's fade)
  Black, // black nearer than kBlackDepth (0x8010F1A0's leaves)
  Fog,   // toward the far colour by SZ - kFogDepth (the averaged path past kFogBucket)
};

// Where a path puts a corner its own projection placed without error, so a neighbour sharing it meets the edge.
enum class Edges : std::uint8_t {
  Placed, // where the GTE placed it (the averaged path and the subdivided leaves link the GTE's vertices)
  Split,  // on its word, an edge between two such corners bent at the halfway word (0x8010F968's split)
};

// Packets one call may add for records it draws from their clipped polygons.
constexpr std::uint32_t kClippedPackets = 256u;
// Each fan triangle of a clipped polygon is halved once in model space, so its texture bends with the view.
constexpr std::uint32_t kPiecesPerTriangle = 4u;

// What a call may still add from clipped polygons, and the records it could not.
struct ClipBudget {
  std::uint32_t packets = kClippedPackets;
  std::uint32_t refused = 0;
};

// What every packet of one record shares.
struct Quad {
  const psx::present::EmitMemory &memory;
  std::uint32_t record;
  const OrderingTable &ot;
  bool exact; // no GTE error projecting the record's corners
  ClipBudget &budget;
  wide_window::Window window; // the columns the picture shows

  // The picture shows columns past the guest's 320; at 4:3 every path is the guest's.
  [[nodiscard]] bool widened() const {
    return window.left < 0 || window.right > wide_window::kGuestWidth;
  }

  void link(std::uint32_t packet, std::int32_t bucket) const {
    ot.link({.packet = packet, .words = kModelGt4.packetWords, .bucket = static_cast<std::uint32_t>(bucket)});
  }
  void stampModes(std::uint32_t packet) const {
    memory.mem_w16(packet + ModelPacket::kUv[0] + 2u, memory.mem_r16(record + kClut));
    memory.mem_w8(packet + ModelPacket::kColour[0] + 3u, static_cast<std::uint8_t>(kGt4Code));
    memory.mem_w16(packet + ModelPacket::kUv[1] + 2u, memory.mem_r16(record + kTpage));
  }

  // `corners`' part in `columns`, projected exactly and linked at `bucket` as triangles from `packet`; nullopt,
  // with nothing written, when the call's budget cannot hold it.
  std::optional<std::uint32_t> linkClipped(std::uint32_t packet,
                                           const std::array<ModelPoint, 4> &corners,
                                           ViewClip::Columns columns,
                                           std::int32_t bucket,
                                           Tint tint,
                                           Edges edges) const;
  // A projection with a GTE error put in place: the record drawn from its clipped polygon across the picture, or
  // the guest's packet when the budget is spent.
  std::optional<std::uint32_t> linkErrored(std::uint32_t packet, std::int32_t bucket, Tint tint, Edges edges) const;
  // What the guest skips or culls for a GTE error, in the margins only: inside its 320 columns it drew nothing.
  std::uint32_t linkMargins(std::uint32_t packet,
                            const std::array<ModelPoint, 4> &corners,
                            std::int32_t bucket,
                            Tint tint,
                            Edges edges) const;

private:
  void storePoint(std::uint32_t packet,
                  std::uint32_t corner,
                  const ClipPoint &point,
                  const std::optional<psx::gte::ProjectedVertex> &guest,
                  Tint tint) const;
};

// 0x8010F1A0's leaf: RotTransPers4 of the corners, culled and linked at the farthest corner's bucket.
std::uint32_t projectSubdivided(const Quad &quad,
                                const Visibility &visible,
                                std::uint32_t packet,
                                const std::array<ModelPoint, 4> &c) {
  const psx::present::EmitMemory &memory = quad.memory;
  psx::cpu::Gte &gte = memory.core().gte();
  count(memory, kLeafCount);
  const auto xy = [](const ModelPoint &corner) {
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
  std::array<std::int32_t, 4> depths{};
  for (std::uint32_t corner = 0; corner < 4u; ++corner) {
    depths[corner] = static_cast<std::int32_t>(gte.readData(psx::gte::kSz0 + corner));
  }
  const std::int32_t bucket = splitBucketOf(depths);
  if (static_cast<std::int32_t>(flag) < 0) {
    return splitBucket(bucket) ? quad.linkMargins(packet, c, bucket, Tint::Black, Edges::Placed) : packet;
  }
  const ModelPacket model(memory, kModelGt4, {.record = quad.record, .packet = packet}, kOverlayStage);
  if (!model.onScreen(visible) || !splitBucket(bucket)) {
    return packet;
  }
  quad.stampModes(packet);
  for (std::uint32_t corner = 0; corner < 4u; ++corner) {
    const ModelPoint &from = c[corner];
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
    const Quad &quad, const Visibility &visible, std::uint32_t packet, int levels, const std::array<ModelPoint, 4> &c) {
  if (levels == 0) {
    return projectSubdivided(quad, visible, packet, c);
  }
  const ModelPoint top = halfway(c[0], c[1]);
  const ModelPoint bottom = halfway(c[2], c[3]);
  const ModelPoint left = halfway(c[0], c[2]);
  const ModelPoint right = halfway(c[1], c[3]);
  const ModelPoint centre = halfway(bottom, left);
  packet = subdivide(quad, visible, packet, levels - 1, {c[0], top, left, centre});
  packet = subdivide(quad, visible, packet, levels - 1, {top, c[1], centre, right});
  packet = subdivide(quad, visible, packet, levels - 1, {left, centre, c[2], bottom});
  return subdivide(quad, visible, packet, levels - 1, {centre, right, bottom, c[3]});
}

// The record's corners as 0x80110290 hands them to 0x8010F1A0.
std::array<ModelPoint, 4> modelCorners(const psx::present::EmitMemory &memory, std::uint32_t record) {
  constexpr std::array<std::uint32_t, 4> kUvAt{8u, 0xCu, 0x10u, 0x12u};
  std::array<ModelPoint, 4> corners{};
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

void loadModelPoint(psx::cpu::Gte &gte, const ClipPoint &point) {
  const auto unit = [](double value) {
    return static_cast<std::uint16_t>(static_cast<std::int16_t>(std::lround(value)));
  };
  gte.writeData(psx::gte::kVxy0, (static_cast<std::uint32_t>(unit(point.y)) << 16) | unit(point.x));
  gte.writeData(psx::gte::kVz0, unit(point.z));
  gte.execute(psx::gte::kRtps);
}

std::uint8_t channel(double value) {
  return static_cast<std::uint8_t>(std::clamp(std::lround(value), 0l, 255l));
}

void Quad::storePoint(std::uint32_t packet,
                      std::uint32_t corner,
                      const ClipPoint &point,
                      const std::optional<psx::gte::ProjectedVertex> &guest,
                      Tint tint) const {
  psx::cpu::Gte &gte = memory.core().gte();
  // The GTE gives the point its depth; a cut point's place on screen is the unrounded one.
  loadModelPoint(gte, point);
  GtePlace place = gte.projected(2).place();
  const ViewClip::Screen at = ViewClip::screen(gte, point);
  place.x = static_cast<float>(at.x);
  place.y = static_cast<float>(at.y);
  memory.storeVertex(packet + ModelPacket::kScreen[corner], guest.value_or(psx::gte::ProjectedVertex::nearest(place)));
  const auto depth = static_cast<std::int32_t>(gte.readData(psx::gte::kSz3));
  const bool black = tint == Tint::Black && depth < kBlackDepth;
  const std::uint32_t colour = packet + ModelPacket::kColour[corner];
  memory.mem_w8(colour + 0u, black ? std::uint8_t{0} : channel(point.r));
  memory.mem_w8(colour + 1u, black ? std::uint8_t{0} : channel(point.g));
  memory.mem_w8(colour + 2u, black ? std::uint8_t{0} : channel(point.b));
  memory.mem_w8(packet + ModelPacket::kUv[corner] + 0u, channel(point.u));
  memory.mem_w8(packet + ModelPacket::kUv[corner] + 1u, channel(point.v));
  const ModelPacket model(memory, kModelGt4, {.record = record, .packet = packet}, kOverlayStage);
  if (tint == Tint::Fade) {
    model.shade({.corner = static_cast<int>(corner), .intensity = kSubdivideDepth - depth});
  } else if (tint == Tint::Fog) {
    model.shade({.corner = static_cast<int>(corner), .intensity = depth - kFogDepth});
  }
}

std::optional<std::uint32_t> Quad::linkClipped(std::uint32_t packet,
                                               const std::array<ModelPoint, 4> &corners,
                                               ViewClip::Columns columns,
                                               std::int32_t bucket,
                                               Tint tint,
                                               Edges edges) const {
  lucent::debug("groundclip", "record {:#010x} clipped to columns [{}, {})", record, columns.left, columns.right);
  psx::cpu::Gte &gte = memory.core().gte();
  const std::array<ModelPoint, 4> around{corners[0], corners[1], corners[3], corners[2]};
  const std::vector<ClipPoint> placed = ViewClip::place(gte, around);
  // Each corner as the guest's path links it, where its own projection has no error.
  std::array<std::optional<psx::gte::ProjectedVertex>, 4> guestCorner{};
  for (std::size_t corner = 0; corner < around.size(); ++corner) {
    loadModelPoint(gte, placed[corner]);
    if (static_cast<std::int32_t>(gte.readControl(psx::gte::kFlag)) >= 0) {
      const psx::gte::ProjectedVertex vertex = gte.projected(2);
      guestCorner[corner] = edges == Edges::Split ? psx::gte::ProjectedVertex::whole(vertex.word()) : vertex;
    }
  }
  const std::vector<ClipPoint> polygon = ViewClip::visible(gte, placed, columns);
  if (polygon.size() < 3u) {
    return packet;
  }
  const auto pieces = static_cast<std::uint32_t>(polygon.size() - 2u) * kPiecesPerTriangle;
  if (pieces > budget.packets) {
    ++budget.refused;
    return std::nullopt;
  }
  budget.packets -= pieces;
  // A polygon point the clip kept from a corner takes that corner's guest vertex.
  std::vector<std::optional<psx::gte::ProjectedVertex>> guest(polygon.size());
  for (std::size_t i = 0; i < polygon.size(); ++i) {
    for (std::size_t corner = 0; corner < placed.size(); ++corner) {
      const ClipPoint &c = placed[corner];
      if (polygon[i].x == c.x && polygon[i].y == c.y && polygon[i].z == c.z) {
        guest[i] = guestCorner[corner];
      }
    }
  }
  // The midpoint of polygon edge `from`-`to`: on a split edge between two guest corners, the guest's halfway word.
  const auto middle = [&](std::size_t from, std::size_t to) {
    const bool edge = (from + 1u) % polygon.size() == to || (to + 1u) % polygon.size() == from;
    std::optional<psx::gte::ProjectedVertex> vertex;
    if (edges == Edges::Split && edge && guest[from] && guest[to]) {
      vertex = psx::gte::ProjectedVertex::whole(halfwayScreen(guest[from]->word(), guest[to]->word()));
    }
    return std::pair{polygon[from].toward(polygon[to], 0.5), vertex};
  };
  using Point = std::pair<ClipPoint, std::optional<psx::gte::ProjectedVertex>>;
  const auto triangle = [&](const Point &a, const Point &b, const Point &c) {
    stampModes(packet);
    storePoint(packet, 0u, a.first, a.second, tint);
    storePoint(packet, 1u, b.first, b.second, tint);
    storePoint(packet, 2u, c.first, c.second, tint);
    storePoint(packet, 3u, c.first, c.second, tint);
    link(packet, bucket);
    packet += kModelGt4.packetBytes;
  };
  for (std::size_t i = 1; i + 1 < polygon.size(); ++i) {
    const Point a{polygon[0], guest[0]};
    const Point b{polygon[i], guest[i]};
    const Point c{polygon[i + 1], guest[i + 1]};
    const Point ab = middle(0, i);
    const Point bc = middle(i, i + 1);
    const Point ca = middle(i + 1, 0);
    triangle(a, ab, ca);
    triangle(ab, b, bc);
    triangle(ca, bc, c);
    triangle(ab, bc, ca);
  }
  return packet;
}

std::optional<std::uint32_t>
Quad::linkErrored(std::uint32_t packet, std::int32_t bucket, Tint tint, Edges edges) const {
  return linkClipped(packet,
                     modelCorners(memory, record),
                     {.left = static_cast<double>(window.left), .right = static_cast<double>(window.right)},
                     bucket,
                     tint,
                     edges);
}

std::uint32_t Quad::linkMargins(
    std::uint32_t packet, const std::array<ModelPoint, 4> &corners, std::int32_t bucket, Tint tint, Edges edges) const {
  constexpr auto kGuestRight = static_cast<double>(wide_window::kGuestWidth);
  for (const ViewClip::Columns margin :
       {ViewClip::Columns{.left = static_cast<double>(window.left), .right = 0.0},
        ViewClip::Columns{.left = kGuestRight, .right = static_cast<double>(window.right)}}) {
    if (margin.left < margin.right) {
      packet = linkClipped(packet, corners, margin, bucket, tint, edges).value_or(packet);
    }
  }
  return packet;
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
  const std::int32_t bucket = splitBucketOf(depths);
  if (!splitBucket(bucket)) {
    return packet;
  }
  if (!quad.exact && quad.widened()) {
    if (const std::optional<std::uint32_t> end =
            quad.linkErrored(packet, bucket, fade ? Tint::Fade : Tint::Plain, Edges::Split)) {
      return *end;
    }
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
  if (!quad.exact && quad.widened()) {
    const Tint tint = bucket > kFogBucket ? Tint::Fog : Tint::Plain;
    if (const std::optional<std::uint32_t> end = quad.linkErrored(model.packet(), bucket, tint, Edges::Placed)) {
      return *end;
    }
  }
  if (bucket > kFogBucket) {
    for (int corner = 0; corner < 4; ++corner) {
      model.shade({.corner = corner, .intensity = depths[static_cast<std::size_t>(corner)] - kFogDepth});
    }
  }
  // An out-of-range bucket links one slot before the table, which no walk reaches.
  quad.link(model.packet(), bucket);
  return model.packet() + kModelGt4.packetBytes;
}

// Which of 0x80110290's paths a projected record takes, by its flag and its corners' depths.
struct Path {
  enum class Kind : std::uint8_t { None, SplitFaded, Split, Subdivide, Averaged };
  Kind kind = Kind::None;
  std::int32_t bucket = 0; // the averaged path's
};

Path pathOf(psx::cpu::Gte &gte, std::uint32_t flags, const std::array<std::int32_t, 4> &depths) {
  const std::int32_t far = std::ranges::max(depths);
  if (far < kNearestDepth) {
    return {};
  }
  const bool subdivided = (flags & 1u) != 0u;
  if (far < kSubdivideDepth) {
    return {.kind = subdivided ? Path::Kind::Subdivide : Path::Kind::SplitFaded};
  }
  if (subdivided && far < kSplitDepth) {
    return {.kind = Path::Kind::Split};
  }
  gte.execute(psx::gte::kAvsz4);
  std::int32_t bucket = OrderingTable::bucketOf(static_cast<std::int32_t>(gte.readData(psx::gte::kOtz)));
  if (!subdivided) {
    if (bucket < 5) {
      return {};
    }
    bucket = std::min(bucket + kFarPush, OrderingTable::kFarthestBucket);
  }
  return {.kind = Path::Kind::Averaged, .bucket = bucket};
}

// A record the guest culls on its wrong corners still reaches a margin: its part there, as its path would draw it.
std::uint32_t
culledMargins(const Quad &quad, std::uint32_t packet, const Path &path, const std::array<std::int32_t, 4> &depths) {
  const std::array<ModelPoint, 4> corners = modelCorners(quad.memory, quad.record);
  switch (path.kind) {
  case Path::Kind::None:
    return packet;
  case Path::Kind::Averaged:
    return quad.linkMargins(
        packet, corners, path.bucket, path.bucket > kFogBucket ? Tint::Fog : Tint::Plain, Edges::Placed);
  case Path::Kind::SplitFaded:
  case Path::Kind::Split:
  case Path::Kind::Subdivide: {
    const std::int32_t bucket = splitBucketOf(depths);
    if (!splitBucket(bucket)) {
      return packet;
    }
    const Tint tint = path.kind == Path::Kind::SplitFaded ? Tint::Fade
                      : path.kind == Path::Kind::Split    ? Tint::Plain
                                                          : Tint::Black;
    return quad.linkMargins(packet, corners, bucket, tint, tint == Tint::Black ? Edges::Placed : Edges::Split);
  }
  }
  return packet;
}

std::uint32_t emitRecord(const psx::present::EmitMemory &memory,
                         const OrderingTable &ot,
                         const Visibility &visible,
                         const ModelPacket &model,
                         ClipBudget &budget) {
  psx::cpu::Gte &gte = memory.core().gte();
  const std::uint32_t record = model.record();
  const std::uint32_t packet = model.packet();
  model.loadTriangle();
  const std::uint32_t colour01 = memory.mem_r32(record);
  memory.mem_w32(packet + ModelPacket::kColour[0], colour01 & kColourCodeMask);
  gte.execute(psx::gte::kRtpt);
  memory.mem_w32(packet + ModelPacket::kColour[1], (colour01 << 4) & kColourMask);
  const std::uint32_t triangleFlag = gte.readControl(psx::gte::kFlag);
  memory.mem_w32(kOverlayStage.gte, triangleFlag);
  model.storeTriangle();
  const std::uint32_t colour23 = memory.mem_r32(record + 4u);
  memory.mem_w32(packet + ModelPacket::kColour[2], colour23 & kColourMask);
  model.loadCornerThree();
  memory.mem_w32(packet + ModelPacket::kColour[3], (colour23 << 4) & kColourMask);
  gte.execute(psx::gte::kRtps);
  memory.mem_w32(packet + ModelPacket::kUv[0], memory.mem_r32(record + 8u));
  const std::uint32_t cornerFlag = gte.readControl(psx::gte::kFlag);
  memory.mem_w32(kOverlayStage.gte, cornerFlag);
  memory.storeVertex(packet + ModelPacket::kScreen[3], gte.projected(2));
  std::array<std::int32_t, 4> depths{};
  for (std::uint32_t corner = 0; corner < 4u; ++corner) {
    depths[corner] = static_cast<std::int32_t>(gte.readData(psx::gte::kSz0 + corner));
  }
  const Quad quad{.memory = memory,
                  .record = record,
                  .ot = ot,
                  .exact = static_cast<std::int32_t>(triangleFlag | cornerFlag) >= 0,
                  .budget = budget,
                  .window = wide_window::drawWindow(&memory.core())};
  if (!model.onScreen(visible)) {
    return quad.exact || !quad.widened() ? packet
                                         : culledMargins(quad, packet, pathOf(gte, model.flags(), depths), depths);
  }
  memory.mem_w32(packet + ModelPacket::kUv[1], memory.mem_r32(record + 0xCu));
  model.storeLastUv();
  const Path path = pathOf(gte, model.flags(), depths);
  switch (path.kind) {
  case Path::Kind::None:
    return packet;
  case Path::Kind::SplitFaded:
    return splitOnScreen(quad, packet, depths, true);
  case Path::Kind::Split:
    return splitOnScreen(quad, packet, depths, false);
  case Path::Kind::Subdivide:
    return subdivide(quad, visible, packet, kSubdivideLevels, modelCorners(memory, record));
  case Path::Kind::Averaged:
    return linkAveraged(quad, model, depths, path.bucket);
  }
  return packet;
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
  ClipBudget budget;
  const std::uint32_t end = emitModelList(memory, call, kModelGt4, kOverlayStage, [&](const ModelPacket &model) {
    return emitRecord(memory, ot, visible, model, budget);
  });
  sessionOf(&memory.core()).groundClip.add(budget.refused);
  return end;
}

void GroundClipTally::report() const {
  lucent::info("ground",
               "run-end: {} A03 ground record(s) in {} call(s) drawn as the guest drew them, the {}-packet clip "
               "budget spent",
               records_,
               calls_,
               kClippedPackets);
}

void SubdividedGround::draw(Core *c) {
  const ListCall call = ListCall::fromRegisters(*c);
  const EmitterObject object(*c, kEntry, call.list);
  ListJobWriter job(*c, 0u, call, (call.count * kMostPacketsPerRecord + kClippedPackets) * kModelGt4.packetBytes);
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
