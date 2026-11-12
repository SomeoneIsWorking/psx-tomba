#include "mesh_chunk.h"

#include "core.h"
#include "emit_memory.h"
#include "game.h"
#include "gte_registers.h"
#include "packet_pool.h"

#include <array>

namespace tomba1 {
namespace {

using psx::present::CornerDomain;
using psx::present::EmitMemory;
using psx::present::ScreenCull;

// The GTE result word each submitter parks before testing it.
constexpr std::uint32_t kGteScratch = 0x1F800070u;
// The ordering table's first entry; a slot is linked only within 0xCA0 bytes of it.
constexpr std::uint32_t kOrderingTableBase = 0x1F8001E0u;
constexpr std::uint32_t kOrderingTableBytes = 0xCA0u;
// The front slot an entity's face is linked into (0x800250F8).
constexpr std::uint32_t kEntityFaceSlot = 0xCu;
// Per-face colours for an entity's tinted gouraud quads (0x800265F8), indexed by 0x12 less the faces left.
constexpr std::uint32_t kTintColours = 0x8007C160u;
constexpr std::uint32_t kTintFaces = 0x12u;

struct Layout {
  std::uint32_t stride;
  std::uint32_t triangle; // VXY0, VZ0 .. VXY2, VZ2 at the vertex record + this
  std::uint32_t corner3;  // VXY3, VZ3 at the vertex record + this; 0 for a triangle
  std::uint32_t tagWords;
  std::array<std::uint32_t, 4> screen; // packet offsets of the corners' SXY
};

constexpr Layout layoutOf(MeshKind kind) {
  switch (kind) {
  case MeshKind::TexturedTriangle:
    return {0x20u, 0x08u, 0u, 7u, {0x08u, 0x10u, 0x18u, 0u}};
  case MeshKind::TexturedQuad:
    return {0x28u, 0x08u, 0x20u, 9u, {0x08u, 0x10u, 0x18u, 0x20u}};
  case MeshKind::GouraudTexTriangle:
    return {0x28u, 0x10u, 0u, 9u, {0x08u, 0x14u, 0x20u, 0u}};
  case MeshKind::GouraudTexQuad:
    return {0x34u, 0x14u, 0x2Cu, 12u, {0x08u, 0x14u, 0x20u, 0x2Cu}};
  case MeshKind::LitFlatTriangle:
  case MeshKind::FlatTriangle:
    return {0x28u, 0x04u, 0u, 4u, {0x08u, 0x0Cu, 0x10u, 0u}};
  case MeshKind::LitFlatQuad:
  case MeshKind::FlatQuad:
    return {0x30u, 0x04u, 0x1Cu, 5u, {0x08u, 0x0Cu, 0x10u, 0x14u}};
  case MeshKind::LitGouraudTriangle:
  case MeshKind::GouraudTriangle:
    return {0x40u, 0x0Cu, 0u, 6u, {0x08u, 0x10u, 0x18u, 0u}};
  case MeshKind::LitGouraudQuad:
  case MeshKind::GouraudQuad:
    return {0x54u, 0x10u, 0x28u, 8u, {0x08u, 0x10u, 0x18u, 0x20u}};
  }
  return {};
}

class FaceWriter {
public:
  FaceWriter(Core &core, std::uint32_t attributes, std::uint32_t packet)
      : mCore(core), mAttributes(attributes), mPacket(packet) {}

  void word(std::uint32_t from, std::uint32_t to) const {
    mCore.mem_w32(mPacket + to, mCore.mem_r32(mAttributes + from));
  }
  void half(std::uint32_t from, std::uint32_t to) const {
    mCore.mem_w16(mPacket + to, mCore.mem_r16(mAttributes + from));
  }
  void light(std::uint32_t command, std::uint32_t colour, std::uint32_t normals, std::uint32_t normalWords) const {
    mCore.gte().writeData(psx::gte::kRgbc, mCore.mem_r32(mAttributes + colour));
    for (std::uint32_t index = 0; index < normalWords; ++index) {
      mCore.gte().writeData(psx::gte::kVxy0 + index, mCore.mem_r32(mAttributes + normals + index * 4u));
    }
    mCore.gte().execute(command);
  }
  void lightColour(std::uint32_t reg, std::uint32_t to) const {
    mCore.mem_w32(mPacket + to, mCore.gte().readData(reg));
  }

private:
  Core &mCore;
  std::uint32_t mAttributes;
  std::uint32_t mPacket;
};

// 0x8002509C: the entity's own face texture when its type nibble is 8 and its state byte 1.
bool entityFaceOverride(Core &core, std::uint32_t entity) {
  return entity != 0u && (core.mem_r8(entity + 0x1Cu) & 0x0Fu) == 8u && core.mem_r8(entity + 2u) == 1u;
}

// Everything after the link: the attributes each kind copies or lights into its packet.
void writeFace(Core &core,
               MeshKind kind,
               const FaceWriter &face,
               std::uint32_t entity,
               std::uint32_t facesLeft,
               std::uint32_t packet) {
  switch (kind) {
  case MeshKind::TexturedTriangle:
    face.word(0x04u, 0x04u);
    face.half(0x00u, 0x0Eu);
    face.half(0x02u, 0x16u);
    face.half(0x0Eu, 0x0Cu);
    face.half(0x16u, 0x14u);
    face.half(0x1Eu, 0x1Cu);
    return;
  case MeshKind::TexturedQuad:
    face.word(0x04u, 0x04u);
    face.half(0x00u, 0x0Eu);
    face.half(0x02u, 0x16u);
    if (entityFaceOverride(core, entity)) {
      for (std::uint32_t corner = 0; corner < 4u; ++corner) {
        core.mem_w16(packet + 0x0Cu + corner * 8u, core.mem_r16(entity + 0x6Cu + corner * 2u));
      }
      return;
    }
    face.half(0x0Eu, 0x0Cu);
    face.half(0x16u, 0x14u);
    face.half(0x1Eu, 0x1Cu);
    face.half(0x26u, 0x24u);
    return;
  case MeshKind::GouraudTexTriangle:
    face.word(0x04u, 0x04u);
    face.word(0x08u, 0x10u);
    face.word(0x0Cu, 0x1Cu);
    face.half(0x00u, 0x0Eu);
    face.half(0x02u, 0x1Au);
    face.half(0x16u, 0x0Cu);
    face.half(0x1Eu, 0x18u);
    face.half(0x26u, 0x24u);
    return;
  case MeshKind::GouraudTexQuad:
    face.word(0x04u, 0x04u);
    face.word(0x08u, 0x10u);
    face.word(0x0Cu, 0x1Cu);
    face.word(0x10u, 0x28u);
    face.half(0x00u, 0x0Eu);
    face.half(0x02u, 0x1Au);
    face.half(0x1Au, 0x0Cu);
    face.half(0x22u, 0x18u);
    face.half(0x2Au, 0x24u);
    face.half(0x32u, 0x30u);
    return;
  case MeshKind::LitFlatTriangle:
    face.light(psx::gte::kNccs, 0x24u, 0x1Cu, 2u);
    face.lightColour(psx::gte::kRgb2, 0x04u);
    return;
  case MeshKind::LitFlatQuad:
    face.light(psx::gte::kNccs, 0x2Cu, 0x24u, 2u);
    face.lightColour(psx::gte::kRgb2, 0x04u);
    return;
  case MeshKind::LitGouraudTriangle:
    face.light(psx::gte::kNcct, 0x3Cu, 0x24u, 6u);
    face.lightColour(psx::gte::kRgb0, 0x04u);
    face.lightColour(psx::gte::kRgb1, 0x0Cu);
    face.lightColour(psx::gte::kRgb2, 0x14u);
    return;
  case MeshKind::LitGouraudQuad:
    face.light(psx::gte::kNccs, 0x50u, 0x48u, 2u);
    face.lightColour(psx::gte::kRgb2, 0x1Cu);
    face.light(psx::gte::kNcct, 0x50u, 0x30u, 6u);
    face.lightColour(psx::gte::kRgb0, 0x04u);
    face.lightColour(psx::gte::kRgb1, 0x0Cu);
    face.lightColour(psx::gte::kRgb2, 0x14u);
    return;
  case MeshKind::FlatTriangle:
  case MeshKind::FlatQuad:
    face.word(0x00u, 0x04u);
    return;
  case MeshKind::GouraudTriangle:
    face.word(0x00u, 0x04u);
    face.word(0x04u, 0x0Cu);
    face.word(0x08u, 0x14u);
    return;
  case MeshKind::GouraudQuad:
    if (entity != 0u && core.mem_r8(entity + 0x0Eu) != 0u) {
      const std::uint32_t tint = kTintColours + ((kTintFaces - facesLeft) << 4u);
      const FaceWriter tinted(core, tint, packet);
      tinted.word(0x00u, 0x04u);
      tinted.word(0x04u, 0x0Cu);
      tinted.word(0x08u, 0x14u);
      tinted.word(0x0Cu, 0x1Cu);
      return;
    }
    face.word(0x00u, 0x04u);
    face.word(0x04u, 0x0Cu);
    face.word(0x08u, 0x14u);
    face.word(0x0Cu, 0x1Cu);
    return;
  }
}

bool parkedNegative(Core &core, std::uint32_t value) {
  core.mem_w32(kGteScratch, value);
  return static_cast<std::int32_t>(value) < 0;
}

} // namespace

ScreenCull MeshChunk::cullFor(const Core &core) {
  return {psx::present::recordCanvasColumns(core.game->guestDisplay.plan(), kGuestWidth), kGuestRows};
}

MeshChunkEnd MeshChunk::emit(Core &core, MeshKind kind, const MeshChunkCall &call, const ScreenCull &cull) {
  const Layout layout = layoutOf(kind);
  const EmitMemory memory(core);
  psx::cpu::Gte &gte = core.gte();
  const int corners = layout.corner3 != 0u ? 4 : 3;
  const std::uint32_t packetBytes = (layout.tagWords + 1u) * 4u;
  std::uint32_t attributes = call.attributes;
  std::uint32_t vertices = call.vertices;
  for (std::uint32_t facesLeft = call.count; facesLeft != 0u;
       --facesLeft, attributes += layout.stride, vertices += layout.stride) {
    for (std::uint32_t reg = 0; reg < 6u; ++reg) {
      gte.writeData(psx::gte::kVxy0 + reg, core.mem_r32(vertices + layout.triangle + reg * 4u));
    }
    gte.execute(psx::gte::kRtpt);
    if (parkedNegative(core, gte.readControl(psx::gte::kFlag))) {
      continue;
    }
    gte.execute(psx::gte::kNclip);
    const std::uint32_t area = gte.readData(psx::gte::kMac0);
    core.mem_w32(kGteScratch, area);
    if (static_cast<std::int32_t>(area) <= 0) {
      continue;
    }
    const std::uint32_t packet = PacketPool::reserve(core, packetBytes);
    for (std::uint32_t corner = 0; corner < 3u; ++corner) {
      memory.storeVertex(packet + layout.screen[corner], gte.projected(corner));
    }
    if (corners == 4) {
      gte.writeData(psx::gte::kVxy0, core.mem_r32(vertices + layout.corner3));
      gte.writeData(psx::gte::kVz0, core.mem_r32(vertices + layout.corner3 + 4u));
      gte.execute(psx::gte::kRtps);
      if (parkedNegative(core, gte.readControl(psx::gte::kFlag))) {
        continue;
      }
      memory.storeVertex(packet + layout.screen[3], gte.projected(2));
    }
    std::array<std::uint32_t, 4> xs{};
    std::array<std::uint32_t, 4> ys{};
    for (int corner = 0; corner < corners; ++corner) {
      xs[corner] = core.mem_r16(packet + layout.screen[corner]);
      ys[corner] = core.mem_r16(packet + layout.screen[corner] + 2u);
    }
    if (!cull.keepsY(ys.data(), corners, CornerDomain::Packed16) ||
        !cull.keepsX(xs.data(), corners, CornerDomain::Packed16)) {
      continue;
    }
    gte.execute(corners == 4 ? psx::gte::kAvsz4 : psx::gte::kAvsz3);
    const std::uint32_t depth = gte.readData(psx::gte::kOtz);
    std::uint32_t slot = depth * 4u + call.orderingTable;
    const bool entityFace = kind == MeshKind::TexturedQuad && entityFaceOverride(core, call.entity);
    if (entityFace) {
      slot = core.mem_r32(kOrderingTableBase) + kEntityFaceSlot;
    } else if (slot - core.mem_r32(kOrderingTableBase) >= kOrderingTableBytes) {
      continue;
    }
    const std::uint32_t next = core.mem_r32(slot);
    core.mem_w32(slot, packet);
    core.mem_w32(packet, next | (layout.tagWords << 24u));
    writeFace(core, kind, FaceWriter(core, attributes, packet), call.entity, facesLeft, packet);
    core.mem_w32(PacketPool::kCursor, core.mem_r32(PacketPool::kCursor) + packetBytes);
  }
  core.mem_w32(kVertexCursor, vertices);
  return {attributes, vertices};
}

} // namespace tomba1
