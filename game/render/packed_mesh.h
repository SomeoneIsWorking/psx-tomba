// game/render/packed_mesh.h — FUN_80027768, the packed-mesh writer shadows and effect models draw through.
//
// A list of 36-byte records, the last with word 1 not positive. Each record packs its four corners as bytes
// (x, y, z each << 8 as s16), its UVs, CLUT and texture page, and four colours. Each becomes one POLY_GT4
// projected through the transform the caller left in GTE CR0-7, depth-cued by the IR0 in scratch 0x1F800090
// and linked into the active OT. Arguments: a0 = list, a1 = CLUT rows added (<< 22 into word 0), a2 = s16
// depth bias, a3 = U offset added to every corner.
#pragma once

#include "emit_memory.h"
#include "list_job.h"
#include "state_producer.h"

#include <cstdint>

class Core;

namespace tomba::native {
class NativeOverrideCatalog;
} // namespace tomba::native

namespace tomba::render {

class PackedMesh {
public:
  static constexpr std::uint32_t kWriter = 0x80027768u;
  static constexpr std::uint32_t kRecordBytes = 0x24u;
  static constexpr std::uint32_t kPacketBytes = 0x34u;
  // Scratch word whose value DPCT and DPCS take as IR0.
  static constexpr std::uint32_t kDepthCue = 0x1F800090u;

  // The call as the guest makes it: the active OT, and the list's records saved as its state.
  static void write(Core *core);
  // The body over `memory`; `call.ot` is the active OT, `call.a1` the CLUT rows.
  static void emit(const psx::present::EmitMemory &memory, const ListCall &call);

  static void registerOverrides(native::NativeOverrideCatalog &natives);
  static void registerStateRenders(psx::present::StateProducerTable &renders, Core &core);
};

} // namespace tomba::render
