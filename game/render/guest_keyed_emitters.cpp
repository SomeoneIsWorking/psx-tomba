// game/render/guest_keyed_emitters.cpp — GuestKeyedEmitters. See guest_keyed_emitters.h.
#include "guest_keyed_emitters.h"

#include "core.h"
#include "core/overrides/native_override_catalog.h"
#include "entry/session.h"
#include "quad_state.h"

#include <iterator>
#include <memory>
#include <utility>

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

template <std::size_t Index> void drawForGuest(Core *core) {
  constexpr GuestKeyedEmitter kLeaf = GuestKeyedEmitters::kEmitters[Index];
  runGuestEmitter(*core, kLeaf.emitter, objectOf(*core, kLeaf.key), "GuestKeyedEmitters");
}

void declare(native::NativeOverrideCatalog &natives, const GuestKeyedEmitter &leaf, psx::cpu::NativeFunction draw) {
  if (leaf.image == kResidentImage) {
    natives.declare(leaf.emitter, leaf.name, draw);
  } else {
    natives.declareOverlay(leaf.image, leaf.emitter, leaf.name, draw);
  }
}

template <std::size_t... Index>
void declareAll(native::NativeOverrideCatalog &natives, std::index_sequence<Index...>) {
  (declare(natives, GuestKeyedEmitters::kEmitters[Index], &drawForGuest<Index>), ...);
}

} // namespace

void GuestKeyedEmitters::registerOverrides(native::NativeOverrideCatalog &natives) {
  declareAll(natives, std::make_index_sequence<std::size(kEmitters)>{});
}

void GuestKeyedEmitters::registerStateRenders(psx::present::StateProducerTable &renders) {
  for (const GuestKeyedEmitter &leaf : kEmitters) {
    renders.install(leaf.emitter, std::make_unique<QuadStateRender>());
  }
}

} // namespace tomba::render
