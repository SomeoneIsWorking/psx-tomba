// game/render/line_strip.h — A08 FUN_80117E84, a node's strip of nine points drawn as thick lines.
// The node's scale (+0x50 << 4) and world point (+0x2C) compose with the scene camera; the strip is at 0x80148668.
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

class LineStrip {
public:
  static constexpr std::uint32_t kEmitter = 0x80117E84u; // A08

  // The body over `memory`; `call.list` is the node, `call.ot` the active OT.
  static void emit(const psx::present::EmitMemory &memory, const ListCall &call);

  static void registerOverrides(native::NativeOverrideCatalog &natives);
  static void registerStateRenders(psx::present::StateProducerTable &renders, Core &core);
};

} // namespace tomba::render
