// game/render/fx_sprite_anchored.h — the single-anchor sprite emitters of type-0x20 render nodes.
// Each projects the node's anchor through the scene camera, publishes its point/depth/scales, and stamps its records.
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

class FxSpriteAnchored {
public:
  enum class Variant : uint32_t { Uniform, ByteScale, ScalePair, Cued, AnimatedQuads };
  static constexpr uint32_t kEmitters[5] = {0x80027CB4u, 0x80027E5Cu, 0x8002801Cu, 0x8002DBE4u, 0x800286CCu};

  enum class Drawn { Nothing, Culled, Stamped };
  // The emitter's body over `memory`: `call.list` is the node, `call.ot` the active OT.
  static Drawn draw(const psx::present::EmitMemory &memory, Variant variant, const render::ListCall &call);

  static void registerOverrides(native::NativeOverrideCatalog &natives);
  static void registerStateRenders(psx::present::StateProducerTable &renders, Core &core);
};

} // namespace tomba
