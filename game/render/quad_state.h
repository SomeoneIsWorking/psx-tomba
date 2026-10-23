// game/render/quad_state.h — the state a guest quad emitter's packets are saved as, and its render.
//
// A guest render fn that draws textured quads through a shared emitter (FUN_8013DD34, FUN_80039F4C's
// glyphs) has no native body to re-run, so its state is the packets it linked: keyed to its node, and the
// render moves each quad's screen vertices by t.
#pragma once

#include "state_producer.h"

#include <cstdint>

class Core;

namespace tomba::render {

struct QuadRange {
  std::uint32_t emitter = 0;
  std::uint32_t object = 0;
  std::uint32_t before = 0;
  std::uint32_t after = 0;
};

// The packets `emitter` linked into the OT between `before` and `after`, keyed to `object`.
void saveQuads(Core &core, const QuadRange &range);

// Every OT-linked packet carrying `object`'s key, with no address range: for a body that restores the
// pool cursor after linking (the HUD gauge), its packets are not inside any before/after span.
void saveKeyedQuads(Core &core, std::uint32_t emitter, std::uint32_t object);

// Runs a guest render body (the original at `emitter`) with its own packets keyed to `object`, and saves
// them as that object's state. One guest leaf's packets, re-emitted at t by QuadStateRender.
void runGuestEmitter(Core &core, std::uint32_t emitter, std::uint32_t object, const char *owner);

// The object of the node in a0, folded with the generation of the record it names.
std::uint32_t nodeObject(Core &core);

class QuadStateRender final : public psx::present::StateProducer {
public:
  void render(const psx::present::StateInterval &states, psx::present::PrimitiveSink &sink) const override;
};

} // namespace tomba::render
