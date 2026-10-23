// game/render/fx_sprite_swarm.h — FUN_800281EC, the per-particle emitter of the scaled-sprite family.
// Each particle at node+0x50 (world VX|VY, VZ|8.8 size) is projected and its records stamped around it.
#pragma once
#include "core.h"
#include "emit_memory.h"
#include "list_job.h"
#include "state_producer.h"
#include <stdint.h>

namespace tomba::native {
class NativeOverrideCatalog;
} // namespace tomba::native

namespace tomba {

class FxSpriteSwarm {
public:
  static constexpr uint32_t kEmitter = 0x800281ECu;

  // The camera and depth cue the particles share; false for a node with no records. `call.list` is the node.
  static bool begin(const psx::present::EmitMemory &memory, const render::ListCall &call);
  // Every particle, stamping `call.a3`'s records into `call.ot`.
  static void particles(const psx::present::EmitMemory &memory, const render::ListCall &call);

  static void registerOverrides(native::NativeOverrideCatalog &natives);
  static void registerStateRenders(psx::present::StateProducerTable &renders, Core &core);
};

} // namespace tomba
