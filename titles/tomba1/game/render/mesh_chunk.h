// One chunk of a mesh: a count, then that many records of one primitive kind, projected, culled at the
// screen edges, linked into the ordering table and written as GPU packets (0x80024CFC..0x800263F0).
#pragma once

#include "screen_cull.h"

#include <cstdint>

class Core;

namespace tomba1 {

enum class MeshKind : std::uint8_t {
  TexturedTriangle,   // 0x80024CFC
  TexturedQuad,       // 0x80024EEC
  GouraudTexTriangle, // 0x800251C0
  GouraudTexQuad,     // 0x800253C8
  LitFlatTriangle,    // 0x8002564C
  LitFlatQuad,        // 0x80025810
  LitGouraudTriangle, // 0x80025A38
  LitGouraudQuad,     // 0x80025C14
  FlatTriangle,       // 0x80025E74
  FlatQuad,           // 0x8002601C
  GouraudTriangle,    // 0x80026228
  GouraudQuad,        // 0x800263F0
};

// Where a chunk's records are: attributes at `attributes`, positions at `vertices` with the same stride (the
// same records for a rigid mesh, a posed copy for a skinned one). `entity` is the drawn object or 0.
struct MeshChunkCall {
  std::uint32_t count = 0;
  std::uint32_t attributes = 0;
  std::uint32_t vertices = 0;
  std::uint32_t orderingTable = 0;
  std::uint32_t entity = 0;
};

struct MeshChunkEnd {
  std::uint32_t attributes = 0;
  std::uint32_t vertices = 0;
};

class MeshChunk {
public:
  // The guest's rows: 0x80016C4C builds 320x224 display and draw rectangles.
  static constexpr int kGuestWidth = 320;
  static constexpr int kGuestRows = 224;
  // Where each chunk leaves its vertex cursor for the next.
  static constexpr std::uint32_t kVertexCursor = 0x1F80008Cu;

  // The cull for the columns the record canvas shows.
  static psx::present::ScreenCull cullFor(const Core &core);

  // Emits every record and stores the end vertex pointer at kVertexCursor, as the guest does.
  static MeshChunkEnd emit(Core &core, MeshKind kind, const MeshChunkCall &call, const psx::present::ScreenCull &cull);
};

} // namespace tomba1
