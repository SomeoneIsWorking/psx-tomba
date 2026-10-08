// game/render/obj_model_view.h — the shared MODEL-VIEW SETUP leaf every effect-mesh draw runs
// first (guest FUN_800318A0). See obj_model_view.cpp for the identification evidence.
//
// Writes the guest's own GTE and scratchpad state; every write is part of the byte-exact state.
#pragma once
#include "core.h"
#include <cstdint>

class Game;

// The composed model-view lives in the scratchpad: a 3x3 rotation as 16-bit elements from +0,
// followed by the three 32-bit translation components.
constexpr uint32_t kComposeTrans = 20u;

// Lens over that scratchpad compose area, reached through the register that identifies it. r16 holds
// the base for the whole body (callee-saved across both leaf calls), and the lens re-reads it on
// every access exactly as the guest does, so it can never go stale across a call — the same idiom as
// game/world/collision_resolve.cpp's Rec.
//
// IT LIVES IN THE HEADER ON PURPOSE, and the setters are deliberately ONE-LINERS: tools/dynamic differential evidence
// harvests a lens setter's mem_wN widths by regex to count `compose.setElem(...)` as the store it
// performs, and that harvester scans `game/**/*.h` ONLY. A lens defined in the .cpp is invisible to
// it, so every store made through it silently vanishes from the width sequence and the method FAILs
// with a store count far short of the oracle's. A setter that grew a second statement or a nested
// brace would fail the same way. (Both traps were hit while porting this very function.)
struct ComposeArea {
  Core *c;
  uint32_t base() const {
    return c->r[16];
  }
  uint32_t elem(uint32_t off) const {
    return c->mem_r16(base() + off);
  }
  void setElem(uint32_t off, uint32_t v) const {
    c->mem_w16(base() + off, (uint16_t)v);
  }
  uint32_t rotWord(uint32_t i) const {
    return c->mem_r32(base() + 4u * i);
  }
  uint32_t trans(uint32_t axis) const {
    return c->mem_r32(base() + kComposeTrans + 4u * axis);
  }
  void setTrans(uint32_t axis, uint32_t v) const {
    c->mem_w32(base() + kComposeTrans + 4u * axis, v);
  }
};

class ObjModelView {
public:
  // FUN_800318A0(worldPos = a0, scaleBytes = a1, angles = a2) -> void.
  //
  // Places one object in the camera's frame of reference so the mesh writer that follows can just
  // feed model-space corners to the GTE:
  //   a0 -> the object's WORLD POSITION, packed the way the GTE wants it (word0 = VX | VY<<16,
  //         word1 = VZ) — this is the node+0x2C/+0x30 anchor pair the whole effect family uses.
  //   a1 -> THREE SCALE BYTES, one per matrix column (x, y, z). Each is used as byte<<2 in 1.12
  //         fixed point, so 0x400 (byte 256) would be unity — the callers run values well under
  //         that, i.e. these meshes are authored large and scaled down.
  //   a2 -> the object's ROTATION as three PSX angle units (SVECTOR: vx|vy packed at +0, vz at +4).
  //
  // On return the GTE control registers hold the composed model-view transform (R in CR0-4, T in
  // CR5-7) and the scratchpad compose area at 0x1F800000 holds the same thing, which is what the
  // per-record RTPT/RTPS calls downstream consume.
  static void composeIntoGte(Core *c);

  static void registerOverrides(Game *game);
};
