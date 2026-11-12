#include "mesh_draw.h"

#include "core.h"
#include "mesh_chunk.h"
#include "r3000.h"

#include <array>

namespace tomba1 {
namespace {

using psx::cpu::kA0;
using psx::cpu::kA1;
using psx::cpu::kA2;
using psx::cpu::kA3;
using psx::cpu::kSp;
using psx::cpu::kV0;

constexpr std::array kRigidOrder{
    MeshKind::TexturedTriangle, MeshKind::TexturedQuad, MeshKind::GouraudTexTriangle, MeshKind::GouraudTexQuad};
constexpr std::array kLitOrder{
    MeshKind::LitFlatTriangle, MeshKind::LitFlatQuad, MeshKind::LitGouraudTriangle, MeshKind::LitGouraudQuad};
constexpr std::array kUnlitOrder{
    MeshKind::FlatTriangle, MeshKind::FlatQuad, MeshKind::GouraudTriangle, MeshKind::GouraudQuad};

// A chunk at `list` (its count) whose positions are at `vertices`.
MeshChunkEnd emitChunk(Core &core,
                       MeshKind kind,
                       std::uint32_t list,
                       std::uint32_t vertices,
                       std::uint32_t orderingTable,
                       std::uint32_t entity,
                       const psx::present::ScreenCull &cull) {
  return MeshChunk::emit(core,
                         kind,
                         {.count = core.mem_r32(list),
                          .attributes = list + 4u,
                          .vertices = vertices,
                          .orderingTable = orderingTable,
                          .entity = entity},
                         cull);
}

// The chunk entries: (list, vertices, ot), or (entity, list, vertices, ot) for the two that read an entity.
template <MeshKind Kind> void chunkEntry(Core *core) {
  const MeshChunkEnd end =
      emitChunk(*core, Kind, core->r[kA0], core->r[kA1], core->r[kA2], 0u, MeshChunk::cullFor(*core));
  core->r[kV0] = end.attributes;
}

template <MeshKind Kind> void entityChunkEntry(Core *core) {
  const MeshChunkEnd end =
      emitChunk(*core, Kind, core->r[kA1], core->r[kA2], core->r[kA3], core->r[kA0], MeshChunk::cullFor(*core));
  core->r[kV0] = end.attributes;
}

// Every kind of one mesh in its stored order. A rigid mesh's positions are its records; a posed one's
// follow on from `vertices`, each chunk resuming at the vertex cursor the last one left.
std::uint32_t drawMesh(Core &core,
                       std::uint32_t list,
                       std::uint32_t vertices,
                       bool posed,
                       std::uint32_t orderingTable,
                       std::uint32_t entity,
                       bool lit) {
  const psx::present::ScreenCull cull = MeshChunk::cullFor(core);
  const auto &shaded = lit ? kLitOrder : kUnlitOrder;
  std::uint32_t next = list;
  std::uint32_t position = posed ? vertices : list + 4u;
  for (const auto &order : {kRigidOrder, shaded}) {
    for (const MeshKind kind : order) {
      const MeshChunkEnd end = emitChunk(core, kind, next, position, orderingTable, entity, cull);
      next = end.attributes;
      position = posed ? end.vertices : next + 4u;
    }
  }
  return next;
}

// 0x80026694 (list, ot, entity, lit)
void drawRigidMesh(Core *core) {
  core->r[kV0] = drawMesh(*core, core->r[kA0], 0u, false, core->r[kA1], core->r[kA2], (core->r[kA3] & 0xFFu) != 0u);
}

// 0x80026E48 (list, ot, entity, vertices, lit at 16(sp))
void drawPosedMesh(Core *core) {
  const bool lit = core->mem_r8(core->r[kSp] + 0x10u) != 0u;
  core->r[kV0] = drawMesh(*core, core->r[kA0], core->r[kA3], true, core->r[kA1], core->r[kA2], lit);
}

// 0x80027600 (list, ot): unlit, no entity.
void drawPlainMesh(Core *core) {
  core->r[kV0] = drawMesh(*core, core->r[kA0], 0u, false, core->r[kA1], 0u, false);
}

// 0x800279E8 (list, ot, entity): a textured-quad chunk after a skipped word, then lit gouraud quads after
// four empty chunk counts.
void drawFaceMesh(Core *core) {
  const psx::present::ScreenCull cull = MeshChunk::cullFor(*core);
  const std::uint32_t list = core->r[kA0];
  const std::uint32_t orderingTable = core->r[kA1];
  const MeshChunkEnd quads =
      emitChunk(*core, MeshKind::TexturedQuad, list + 4u, list + 8u, orderingTable, core->r[kA2], cull);
  const MeshChunkEnd lit = emitChunk(
      *core, MeshKind::LitGouraudQuad, quads.attributes + 0x14u, quads.attributes + 0x18u, orderingTable, 0u, cull);
  core->r[kV0] = lit.attributes;
}

struct Entry {
  std::uint32_t address;
  std::string_view name;
  psx::cpu::NativeFunction function;
};

constexpr std::array kEntries{
    Entry{0x80024CFCu, "MeshChunk::TexturedTriangle", chunkEntry<MeshKind::TexturedTriangle>},
    Entry{0x80024EECu, "MeshChunk::TexturedQuad", entityChunkEntry<MeshKind::TexturedQuad>},
    Entry{0x800251C0u, "MeshChunk::GouraudTexTriangle", chunkEntry<MeshKind::GouraudTexTriangle>},
    Entry{0x800253C8u, "MeshChunk::GouraudTexQuad", chunkEntry<MeshKind::GouraudTexQuad>},
    Entry{0x8002564Cu, "MeshChunk::LitFlatTriangle", chunkEntry<MeshKind::LitFlatTriangle>},
    Entry{0x80025810u, "MeshChunk::LitFlatQuad", chunkEntry<MeshKind::LitFlatQuad>},
    Entry{0x80025A38u, "MeshChunk::LitGouraudTriangle", chunkEntry<MeshKind::LitGouraudTriangle>},
    Entry{0x80025C14u, "MeshChunk::LitGouraudQuad", chunkEntry<MeshKind::LitGouraudQuad>},
    Entry{0x80025E74u, "MeshChunk::FlatTriangle", chunkEntry<MeshKind::FlatTriangle>},
    Entry{0x8002601Cu, "MeshChunk::FlatQuad", chunkEntry<MeshKind::FlatQuad>},
    Entry{0x80026228u, "MeshChunk::GouraudTriangle", chunkEntry<MeshKind::GouraudTriangle>},
    Entry{0x800263F0u, "MeshChunk::GouraudQuad", entityChunkEntry<MeshKind::GouraudQuad>},
    Entry{0x80026694u, "drawRigidMesh", drawRigidMesh},
    Entry{0x80026E48u, "drawPosedMesh", drawPosedMesh},
    Entry{0x80027600u, "drawPlainMesh", drawPlainMesh},
    Entry{0x800279E8u, "drawFaceMesh", drawFaceMesh},
};

} // namespace

bool installMeshDraw(const InstallOverride &install) {
  for (const Entry &entry : kEntries) {
    if (!install(entry.address, entry.name, entry.function)) {
      return false;
    }
  }
  return true;
}

} // namespace tomba1
