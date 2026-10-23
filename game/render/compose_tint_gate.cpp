// game/render/compose_tint_gate.cpp — Render::composeTintGate, guest FUN_8003EF9C.
// The low nibble of node[+0x0D] gates: 0 composes, 2 composes then tints the just-emitted pool span.
#include "core.h"
#include "core/overrides/native_override_catalog.h"
#include "entry/session.h" // rend(c) — the Render instance
#include "guest_call.h"
#include "guest_ordering_table.h"
#include "render.h"

#include <lucent/log.h>

#include <cstdlib>

namespace tomba {

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

  const uint32_t poolBefore = tomba::render::PacketPool(*c).cursor();
  const uint32_t subtype = (c->mem_r8(node + NODE_SUBTYPE) == SUBTYPE_MATCH) ? 1u : 0u;
  const uint32_t mode = c->mem_r8(node + NODE_MODE) & 0x0Fu;

  // Two branches, as the guest has them; not merged, the emit sites publish different jal-site ra.
  if (mode == MODE_COMPOSE) {
    c->r[4] = node;
    c->r[5] = subtype;
    c->r[31] = 0x8003EFE8u; // jal-site ra
    psx::cpu::callGuestNow(*c, __func__, 0x8003F07Cu);
  } else if (mode == MODE_COMPOSE_TINT) {
    // The tint is read after the emission; what the emitters saved was read before it.
    const render::ObjectEffect tint = render::ObjectEffect::colorAdd(*c, node);
    c->r[4] = node;
    c->r[5] = subtype;
    c->r[31] = 0x8003EFFCu; // jal-site ra
    {
      const render::ObjectEffectScope underTint(rend(c)->objectEffect, tint);
      psx::cpu::callGuestNow(*c, __func__, 0x8003F07Cu);
    }
    if (render::ObjectEffect::colorAdd(*c, node) != tint) {
      lucent::error("render", "node 0x{:08X}: its tint changed while FUN_8003F07C emitted it", node);
      std::abort();
    }

    // FUN_8003D584 over precisely the primitives just emitted: [poolBefore, poolNow).
    c->r[4] = node;
    c->r[5] = poolBefore;
    c->r[6] = tomba::render::PacketPool(*c).cursor();
    c->r[31] = 0x8003F00Cu; // jal-site ra
    tint.apply(psx::present::EmitMemory(*c), poolBefore, c->r[6]);
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

void compose_tint_gate_install(native::NativeOverrideCatalog &natives) {
  natives.declare(0x8003EF9Cu, "Render::composeTintGate", ov_compose_tint_gate);
}

} // namespace tomba
