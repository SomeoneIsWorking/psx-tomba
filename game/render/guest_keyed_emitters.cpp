// game/render/guest_keyed_emitters.cpp — GuestKeyedEmitters. See guest_keyed_emitters.h.
#include "guest_keyed_emitters.h"

#include "core.h"
#include "core/overrides/native_override_catalog.h"
#include "entry/session.h"
#include "quad_state.h"

#include <memory>

namespace tomba::render {
namespace {

// The front end's chrome is drawn by one body with no object of its own.
constexpr std::uint32_t kScreenObject = 0u;

std::uint32_t objectOf(Core &core, EmitterKey key) {
  switch (key) {
  case EmitterKey::Node:
    return nodeObject(core);
  case EmitterKey::CallSite:
    return sessionOf(&core).callSites.object(core.r[31]);
  case EmitterKey::Screen:
    break;
  }
  return kScreenObject;
}

template <std::uint32_t Emitter, EmitterKey Key> void drawForGuest(Core *core) {
  runGuestEmitter(*core, Emitter, objectOf(*core, Key), "GuestKeyedEmitters");
}

} // namespace

void GuestKeyedEmitters::registerOverrides(native::NativeOverrideCatalog &natives) {
  natives.declare(0x800328BCu, "GuestKeyedEmitters::nodeStamp", &drawForGuest<0x800328BCu, EmitterKey::Node>);
  natives.declareOverlay(
      "A01", 0x80129114u, "GuestKeyedEmitters::a01AngleLeaf", &drawForGuest<0x80129114u, EmitterKey::Node>);
  natives.declareOverlay(
      "A01", 0x8012D9E8u, "GuestKeyedEmitters::a01RecordLeaf", &drawForGuest<0x8012D9E8u, EmitterKey::Node>);
  natives.declareOverlay(
      "A00", 0x8013E08Cu, "GuestKeyedEmitters::a00LineLeaf", &drawForGuest<0x8013E08Cu, EmitterKey::Node>);
  natives.declareOverlay(
      "A07", 0x80117D4Cu, "GuestKeyedEmitters::a07Rope", &drawForGuest<0x80117D4Cu, EmitterKey::Node>);
  natives.declareOverlay(
      "A0F", 0x80116ABCu, "GuestKeyedEmitters::a0fWire", &drawForGuest<0x80116ABCu, EmitterKey::Node>);
  natives.declareOverlay(
      "A0F", 0x801143C4u, "GuestKeyedEmitters::lensFlare", &drawForGuest<0x801143C4u, EmitterKey::Node>);
  natives.declareOverlay(
      "A0D", 0x80110C14u, "GuestKeyedEmitters::a0dRing", &drawForGuest<0x80110C14u, EmitterKey::Node>);
  natives.declareOverlay(
      "A03", 0x801113B4u, "GuestKeyedEmitters::a03Ribbon", &drawForGuest<0x801113B4u, EmitterKey::Node>);
  natives.declareOverlay(
      "A06", 0x80129AF8u, "GuestKeyedEmitters::a06Line", &drawForGuest<0x80129AF8u, EmitterKey::CallSite>);
  natives.declareOverlay(
      "DEMO", 0x80106690u, "GuestKeyedEmitters::demoEdgeSprites", &drawForGuest<0x80106690u, EmitterKey::Screen>);
  natives.declareOverlay(
      "A03", 0x80109EECu, "GuestKeyedEmitters::a03Hud", &drawForGuest<0x80109EECu, EmitterKey::Screen>);
  natives.declareOverlay(
      "A01", 0x801311FCu, "GuestKeyedEmitters::a01Effect", &drawForGuest<0x801311FCu, EmitterKey::Screen>);
}

void GuestKeyedEmitters::registerStateRenders(psx::present::StateProducerTable &renders) {
  for (const GuestKeyedEmitter &leaf : kEmitters) {
    renders.install(leaf.emitter, std::make_unique<QuadStateRender>());
  }
}

} // namespace tomba::render
