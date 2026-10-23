// game/render/area_map.h — A02 FUN_80113628, the area's map with the player's marker.
// Area type 13's draw: a 64x64 map sprite and an 8x8 marker at the player (0x1F800160 x, 0x1F800164 z), into bucket 4.
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

class AreaMap {
public:
  static constexpr std::uint32_t kEmitter = 0x80113628u; // A02

  // The body over `memory`; `call.ot` is the active OT and `call.sp` the caller's stack.
  static void emit(const psx::present::EmitMemory &memory, const ListCall &call);

  static void registerOverrides(native::NativeOverrideCatalog &natives);
  static void registerStateRenders(psx::present::StateProducerTable &renders, Core &core);
};

} // namespace tomba::render
