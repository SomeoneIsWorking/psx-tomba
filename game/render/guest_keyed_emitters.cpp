// game/render/guest_keyed_emitters.cpp — GuestKeyedEmitters. See guest_keyed_emitters.h.
#include "guest_keyed_emitters.h"

#include "core.h"
#include "core/overrides/native_override_catalog.h"
#include "quad_state.h"

#include <memory>

namespace tomba::render {
namespace {

// The front end's chrome is drawn by one body with no object of its own.
constexpr std::uint32_t kScreenObject = 0u;

template <std::uint32_t Emitter, bool NodeOwned> void drawForGuest(Core *core) {
  const std::uint32_t object = NodeOwned ? nodeObject(*core) : kScreenObject;
  runGuestEmitter(*core, Emitter, object, "GuestKeyedEmitters");
}

} // namespace

void GuestKeyedEmitters::registerOverrides(native::NativeOverrideCatalog &natives) {
  natives.declareOverlay("A01", 0x80129114u, "GuestKeyedEmitters::a01AngleLeaf", &drawForGuest<0x80129114u, true>);
  natives.declareOverlay("A01", 0x8012D9E8u, "GuestKeyedEmitters::a01RecordLeaf", &drawForGuest<0x8012D9E8u, true>);
  natives.declareOverlay("A00", 0x8013E08Cu, "GuestKeyedEmitters::a00LineLeaf", &drawForGuest<0x8013E08Cu, true>);
  natives.declareOverlay("DEMO", 0x80106690u, "GuestKeyedEmitters::demoEdgeSprites", &drawForGuest<0x80106690u, false>);
  natives.declareOverlay("A01", 0x801311FCu, "GuestKeyedEmitters::a01Effect", &drawForGuest<0x801311FCu, false>);
}

void GuestKeyedEmitters::registerStateRenders(psx::present::StateProducerTable &renders) {
  for (const GuestKeyedEmitter &leaf : kEmitters) {
    renders.install(leaf.emitter, std::make_unique<QuadStateRender>());
  }
}

} // namespace tomba::render
