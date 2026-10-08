// game/render/widescreen_margin_quad.h — native port of the widescreen-margin OT.GT4 quad
// emitter (FUN_8013CDD4, a00 overlay). See widescreen_margin_quad.cpp for the RE trace and
// docs/findings/render.md "0x8013CDD4 port — ambiguity SETTLED" for the record-layout proof.
//
// Writes the guest's own GTE, OT and packet-pool state; every write is part of the byte-exact state.
#pragma once
struct Core;
class Game;

class WidescreenMarginQuad {
public:
  // FUN_8013CDD4(obj=a0) -> void. Walks obj's single margin-node + its 36-byte-stride quad
  // record array, GTE-transforms each into a POLY_GT4 packet, and links it into the OT.
  static void emit(Core *c);

  static void registerOverrides(Game *game);
};
