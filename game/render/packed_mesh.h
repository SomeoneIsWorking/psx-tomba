// game/render/packed_mesh.h — FUN_80027768, the packed-mesh writer; FUN_8002847C draws its records as screen quads.
// A 36-byte record list, last with word 1 not positive; A08's FUN_8011731C adds a V offset instead of CLUT rows.
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

// The 36-byte record: UV words at +0, +4 and +8, colours at +0xC..+0x18, corner bytes from +0x1C.
struct PackedRecord {
  static constexpr std::uint32_t kPacketScreen[4] = {8u, 0x14u, 0x20u, 0x2Cu};
  static constexpr std::uint32_t kPacketUv[4] = {0xCu, 0x18u, 0x24u, 0x30u};

  // A corner's UV word of a POLY_GT4: with the CLUT (plus `clutRows` << 22) on corner 0, the texture page on 1.
  struct Uv {
    std::uint32_t record;
    std::uint32_t packet;
    std::uint32_t corner;
    std::uint32_t clutRows;
  };
  static void uv(const psx::present::EmitMemory &memory, const Uv &uv);
  // Corners 0..2 by DPCT and corner 3 by DPCS, all toward the far colour by the scratch IR0.
  static void shade(const psx::present::EmitMemory &memory, std::uint32_t record, std::uint32_t packet);
  // The records of the list, through the first whose word 1 is not positive.
  static std::uint32_t count(const psx::present::EmitMemory &memory, std::uint32_t list);
};

class PackedMesh {
public:
  static constexpr std::uint32_t kWriter = 0x80027768u;
  static constexpr std::uint32_t kRecordBytes = 0x24u;
  static constexpr std::uint32_t kPacketBytes = 0x34u;
  // Scratch word whose value DPCT and DPCS take as IR0.
  static constexpr std::uint32_t kDepthCue = 0x1F800090u;

  static constexpr std::uint32_t kVOffsetWriter = 0x8011731Cu; // A08

  // What the writer's a1 is.
  enum class Variant : std::uint32_t {
    ClutRows, // FUN_80027768: CLUT rows added to corner 0's CLUT
    VOffset,  // A08 FUN_8011731C: added to every corner's V
  };

  // The call as the guest makes it: the active OT, and the list's records saved as its state.
  static void write(Core *core);
  // The body over `memory`; `call.ot` is the active OT.
  static void emit(const psx::present::EmitMemory &memory, Variant variant, const ListCall &call);

  static constexpr std::uint32_t kScreenWriter = 0x8002847Cu;
  // FUN_8002847C over `memory`: `list`'s records as screen quads into `ot`, CLUT rows `clutRows`.
  struct ScreenCall {
    std::uint32_t list;
    std::uint32_t clutRows;
    std::uint32_t ot;
  };
  static void emitScreen(const psx::present::EmitMemory &memory, const ScreenCall &call);

  static void registerOverrides(native::NativeOverrideCatalog &natives);
  static void registerStateRenders(psx::present::StateProducerTable &renders, Core &core);

private:
  static void writeSaving(Core &core, Variant variant, const ListCall &call);
};

} // namespace tomba::render
