// game/render/widescreen_margin_quad.h — native port of the widescreen-margin OT.GT4 quad
// emitter (FUN_8013CDD4, a00 overlay). See widescreen_margin_quad.cpp for the RE trace and
// docs/findings/render.md "0x8013CDD4 port — ambiguity SETTLED" for the record-layout proof.
//
// Writes the guest's own GTE, OT and packet-pool state; every write is part of the byte-exact state.
#pragma once

#include "state_producer.h"

struct Core;
class Game;

namespace tomba::native {
class NativeOverrideCatalog;
} // namespace tomba::native

namespace tomba {

class WidescreenMarginQuad {
public:
  static constexpr std::uint32_t kEmitter = 0x8013CDD4u; // A00

  // FUN_8013CDD4(obj=a0) -> void. Walks obj's single margin-node + its 36-byte-stride quad
  // record array, GTE-transforms each into a POLY_GT4 packet, and links it into the OT.
  static void emit(Core *c);

  static void registerOverrides(native::NativeOverrideCatalog &natives);
  // The node keys its own quads; the render moves their screen vertices by t.
  static void registerStateRenders(psx::present::StateProducerTable &renders);
};

} // namespace tomba
