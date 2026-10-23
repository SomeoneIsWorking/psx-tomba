// game/render/anchor_quads.cpp — AnchorQuads. See anchor_quads.h.
#include "anchor_quads.h"

#include "core.h"
#include "core/overrides/native_override_catalog.h"
#include "entry/session.h"
#include "guest_call.h"
#include "guest_ordering_table.h"
#include "native_dispatch.h"
#include "quad_state.h"

#include <cstdint>
#include <memory>

namespace tomba::render {
namespace {

template <std::uint32_t Emitter> void drawForGuest(Core *core) {
  runGuestEmitter(*core, Emitter, nodeObject(*core), "AnchorQuads");
}

} // namespace

void AnchorQuads::registerOverrides(native::NativeOverrideCatalog &natives) {
  natives.declareOverlay("A00", kEmitters[0], "AnchorQuads::rope", &drawForGuest<kEmitters[0]>);
  natives.declareOverlay("A00", kEmitters[1], "AnchorQuads::chain", &drawForGuest<kEmitters[1]>);
}

void AnchorQuads::registerStateRenders(psx::present::StateProducerTable &renders, Core &core) {
  (void)core;
  renders.install(kEmitters[0], std::make_unique<QuadStateRender>());
  renders.install(kEmitters[1], std::make_unique<QuadStateRender>());
}

} // namespace tomba::render
