// Tomba2Engine — widescreen-margin re-include set. See margin_render.h.
#include "margin_render.h"
#include "game.h"
#include <lucent/log.h>

namespace tomba {

// Entity type (node+0xc) of the static world-geometry objects the wide frustum re-includes.
// later-133: exactly these account for the +24 widescreen-margin commands at the field.
#define T2_WORLDGEO_TYPE 0x03

// Record a re-include-eligible node (deduped within the frame). Only type-0x03 nodes render in the
// real +1 path (through guest 0x8003CDD8(node, 0)); other types render elsewhere or not at all.
void MarginRenderer::collect(Core *c, uint32_t node) {
  if (node == 0) {
    return;
  }
  if (c->mem_r8(node + 0xc) != T2_WORLDGEO_TYPE) {
    return;
  }
  if (!seen_.insert(node).second) {
    return;
  }
  nodes_.push_back(node);
}

// The collected set has no drawer until the widescreen margin producer exists; reset per frame.
void MarginRenderer::flush(Core *c) {
  lucent::debug("margin", "f{} collected {} margin nodes", c->game->timing.logicFrame, nodes_.size());
  nodes_.clear();
  seen_.clear();
}

} // namespace tomba
