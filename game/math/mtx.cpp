#include "mtx.h"
#include "core.h"
#include "core/entry/game_ctx.h"                    // mtxOf(c) — the per-Core Mtx instance
#include "core/overrides/native_override_catalog.h" // tomba::native::declareOverride — the one native-override registry

void Mtx::identity(uint32_t addr) {
  Core *c = this->core;
  // m[0][0]=0x1000 m[0][1]=0 | m[0][2]=0 m[1][0]=0 | m[1][1]=0x1000 m[1][2]=0 |
  // m[2][0]=0 m[2][1]=0    | m[2][2]=0x1000 (pad)   | t[0]=0 t[1]=0 t[2]=0
  c->mem_w32(addr + 0, 0x00001000u);
  c->mem_w32(addr + 4, 0);
  c->mem_w32(addr + 8, 0x00001000u);
  c->mem_w32(addr + 12, 0);
  c->mem_w32(addr + 16, 0x00001000u);
  c->mem_w32(addr + 20, 0);
  c->mem_w32(addr + 24, 0);
  c->mem_w32(addr + 28, 0);
}

// FUN_800517BC (diagonal-scale matrix write) is owned by `NodeXform::seedBlock`
// (game/render/node_xform.cpp), which is the real wired dispatch target for that address
// (registered via NodeXform::registerOverrides into the global override registry). This class used to carry an
// unused, uncalled duplicate implementation (`Mtx::diagonal`) — deleted 2026-07-08 (dual-ownership
// found via codemap; dead code, zero callers).

// ─────────────────────────────────────────────────────────────────────────────────────────────────
// Wiring. The trampoline is the whole guest ABI for this leaf: matrix address in a0, and v0 = 4096.
//
// v0 is NOT incidental. The guest body computes the 0x1000 diagonal constant into r2 and stores
// it from there, so the guest function returns 4096 as a side effect of how it was compiled. The
// native writes the constant directly and would otherwise leave v0 holding whatever the previous
// call left — a difference no RAM compare can see, but one that a caller reading the return value
// would. Set it explicitly rather than relying on a leftover.
// EXPECT AN OVHIT COUNT MISMATCH ON THIS ADDRESS, and do not read it as a divergence. A 1500-frame
// SBS run reports native=20269 oracle=26738 alongside a 50/50 byte-identical compare. The gap is the
// ~6.5k calls that already-ported callers make as `mtxOf(c).identity(...)` — a direct C++ method call
// that never passes through the guest function, so the registry cannot count it. Core B, being pure
// substrate, reaches every one of them through guest 0x80051794 and counts them all. The mismatch is
// therefore a measure of how much of this leaf's caller set is already native, and it should GROW as
// more callers are ported. `ovhit`'s "control-flow divergence" label is right for a target whose
// callers are all still guest code and wrong for one like this.
static void eov_identity(Core *c) {
  mtxOf(c).identity(c->r[4]);
  c->r[2] = 4096;
}

void Mtx::registerOverrides(Game *) {
  tomba::native::declareOverride(0x80051794u, "&eov_identity", &eov_identity);
}
