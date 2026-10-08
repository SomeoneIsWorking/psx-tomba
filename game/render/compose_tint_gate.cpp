// game/render/compose_tint_gate.cpp — Render::composeTintGate, guest FUN_8003EF9C.
//
// RE'd 2026-07-22 from guest 0x8003EF9C. One of the two per-type render handlers still unported from
// the render frontier list (the other is FUN_8003F174); every other entry on that list is already
// LIVE, so this closes half the remaining gap.
//
// WHAT IT DOES. It is a small GATE in front of the geometry emitter, and its whole point is the pool
// snapshot:
//
//     poolBefore = *packetPool          // where the emitter is about to start writing
//     compose(node, node[0x0B] == 0x0F) // emit this object's primitives
//     if (mode == 2)                    // ...and then tint exactly those primitives
//         effectColorAdd(node, poolBefore, *packetPool)
//
// `effectColorAdd` (FUN_8003D584, already owned) takes a LO/HI packet range and modulates each
// colour channel over it. Capturing the pool pointer before the emit and passing it with the pointer
// after is how the guest says "apply this to the primitives I just produced, and nothing earlier".
// Read without that framing the two pool reads look incidental; they are the mechanism.
//
// The gate itself is the low nibble of node[+0x0D]: 0 = compose plain, 2 = compose then tint, and any
// other value draws NOTHING AT ALL — an object in an unlisted mode is silently skipped, which is worth
// knowing when something fails to appear.
//
// node[+0x0B] == 0x0F is passed through to the emitter as a bool. It is a sub-type flag; what it
// selects lives inside FUN_8003F07C, which is not ported yet, so it is forwarded rather than named.
#include "core.h"
#include "core/entry/game_ctx.h" // rend(c) — the Render instance
#include "core/overrides/native_override_catalog.h"
#include "guest_call.h"
#include "ordering_table.h"
#include "render.h"

// Render::effectColorAdd, which already owns this address

namespace {

constexpr uint32_t NODE_SUBTYPE = 0x0Bu; // == 0x0F selects the emitter's alternate path
constexpr uint32_t NODE_MODE = 0x0Du;    // low nibble gates this handler
constexpr uint8_t SUBTYPE_MATCH = 0x0F;

constexpr uint32_t MODE_COMPOSE = 0u;      // emit only
constexpr uint32_t MODE_COMPOSE_TINT = 2u; // emit, then colour-add over what was emitted

} // namespace

// ORACLE: guest 0x8003EF9C
void Render::composeTintGate(Core *c) {
  const uint32_t node = c->r[4];

  c->r[29] -= 32;
  const uint32_t frame = c->r[29];
  c->mem_w32(frame + 24, c->r[18]);
  c->mem_w32(frame + 28, c->r[31]);
  c->mem_w32(frame + 20, c->r[17]);
  c->mem_w32(frame + 16, c->r[16]);

  const uint32_t poolBefore = tomba2::render::PacketPool(*c).cursor();
  const uint32_t subtype = (c->mem_r8(node + NODE_SUBTYPE) == SUBTYPE_MATCH) ? 1u : 0u;
  const uint32_t mode = c->mem_r8(node + NODE_MODE) & 0x0Fu;

  // Two separate branches, as the guest has them — mode 0 emits and stops, mode 2 emits and then
  // tints. They are not merged, because the emit call sites differ in the jal-site ra they publish.
  if (mode == MODE_COMPOSE) {
    c->r[4] = node;
    c->r[5] = subtype;
    c->r[31] = 0x8003EFE8u; // jal-site ra
    psx::cpu::dispatchGuestToReturn0(*c, 0x8003F07Cu, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
  } else if (mode == MODE_COMPOSE_TINT) {
    c->r[4] = node;
    c->r[5] = subtype;
    c->r[31] = 0x8003EFFCu; // jal-site ra
    psx::cpu::dispatchGuestToReturn0(*c, 0x8003F07Cu, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);

    // tint precisely the primitives just emitted: [poolBefore, poolNow). Called through the thunk,
    // so the override registry routes it to Render::effectColorAdd rather than the substrate body.
    c->r[4] = node;
    c->r[5] = poolBefore;
    c->r[6] = tomba2::render::PacketPool(*c).cursor();
    c->r[31] = 0x8003F00Cu; // jal-site ra
    psx::cpu::dispatchGuestToReturn0(*c, 0x8003D584u, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
  }

  c->r[31] = c->mem_r32(frame + 28);
  c->r[18] = c->mem_r32(frame + 24);
  c->r[17] = c->mem_r32(frame + 20);
  c->r[16] = c->mem_r32(frame + 16);
  c->r[29] += 32;
}

static void ov_compose_tint_gate(Core *c) {
  Render::composeTintGate(c);
}

void compose_tint_gate_install() {
  tomba::native::declareOverride(0x8003EF9Cu, "Render::composeTintGate", ov_compose_tint_gate);
}
