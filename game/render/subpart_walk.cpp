// game/render/subpart_walk.cpp — Render::subPartWalk, guest FUN_8003F174.
//
// RE'd 2026-07-22 from guest 0x8003F174. The LAST unported entry on the render frontier's per-type
// handler list (0x8003EF9C was the other, ported the same day as Render::composeTintGate).
//
// WHAT IT DOES. Draws a node that is built from SUB-PARTS, each with its own transform. The node
// carries an array of sub-part pointers at +0xC0 (stride 4), and for each one this:
//
//   1. loads that sub-part's TRANSFORM into the GTE — eight control words from `sub+0x18`, which is
//      the standard 3x3 rotation (regs 0..4, packed two shorts per word) plus translation (regs 5..7);
//   2. submits the sub-part's geometry block (`sub+0x40`) through FUN_8003F698, along with the
//      ordering-table base and the caller's second argument passed straight through.
//
// So each sub-part is positioned independently: the transform is re-loaded per iteration rather than
// once for the whole node. That is what makes this a sub-part walker rather than a plain emitter.
//
// TWO SEPARATE COUNTS, and they are not interchangeable:
//   node[+8] — checked at the TOP of every iteration; the walk returns the moment the index reaches it
//   node[+9] — the do/while bound at the BOTTOM
// The guest tests both, and it tests [+8] first, so a node with [+8] == 0 draws nothing even if [+9]
// is large. Collapsing them into one loop bound would change behaviour for any node where they differ.
#include "core.h"
#include "core/entry/game_ctx.h" // eng()
#include "core/overrides/native_override_catalog.h"
#include "game.h"
#include "guest_call.h"
#include "ordering_table.h"
#include "render.h"

// gte_write_ctrl is declared in core.h (uint32_t, uint32_t) — included above

namespace {

constexpr uint32_t SUBPART_ARRAY = 0xC0u; // node+0xC0: array of sub-part pointers, stride 4
constexpr uint32_t SUB_TRANSFORM = 0x18u; // sub+0x18: 8 GTE control words
constexpr uint32_t SUB_GEOMBLK = 0x40u;   // sub+0x40: geometry block to submit
constexpr uint32_t NODE_LIMIT = 8u;       // checked at the top of each iteration
constexpr uint32_t NODE_COUNT = 9u;       // do/while bound at the bottom

} // namespace

// ORACLE: guest 0x8003F174
void Render::subPartWalk(Core *c) {
  const uint32_t node = c->r[4];
  const uint32_t passThrough = c->r[5];

  c->r[29] -= 80;
  const uint32_t frame = c->r[29];
  c->mem_w32(frame + 64, c->r[18]);
  c->mem_w32(frame + 76, c->r[31]);
  c->mem_w32(frame + 72, c->r[20]);
  c->mem_w32(frame + 68, c->r[19]);
  c->mem_w32(frame + 60, c->r[17]);
  c->mem_w32(frame + 56, c->r[16]);

  // LIVE-REGISTER LAW (docs/findings/sbs.md) — why the loop state lives in the GUEST registers and
  // not in C++ locals. The callee chain guest 0x8003F698 -> guest 0x800803DC is still substrate, and
  // guest 0x800803DC's prologue SPILLS its incoming s0/s1 into its own guest frame (sp+0x10 / sp+0x14 —
  // 0x801FE808/0x801FE80C on the field leg) before reusing them. guest 0x8003F174 keeps its whole
  // walk state in the callee-saved set for the entire loop — s0=index, s1=cursor, s2=node,
  // s3=OT-pointer page, s4=the pass-through arg — so those registers are genuine guest-stack-visible
  // state, not dead scratch. Holding them only in C++ locals left guest 0x800803DC spilling whatever
  // STALE values the previous native code had parked there: the f169 SBS-full divergence at
  // 0x801FE808 (kanban #61 — A spilled a leftover pointer pair, B spilled index/cursor). Same class,
  // same fix as the r16..r23 mirror in Render::cmdListDispatch (game/render/perobj_dispatch.cpp).
  uint32_t &index = c->r[16];
  uint32_t &cursor = c->r[17]; // walks the +0xC0 array, 4 bytes per sub-part
  c->r[18] = node;
  c->r[20] = passThrough;

  if (c->mem_r8(node + NODE_LIMIT) != 0 && c->mem_r8(node + NODE_COUNT) != 0) {
    index = 0;
    cursor = node;
    c->r[19] = tomba2::render::OrderingTable::kBasePointerPage; // gen addresses the OT base as s3-10040

    do {
      if ((int32_t)index >= (int32_t)(uint32_t)c->mem_r8(node + NODE_LIMIT)) {
        break; // top-of-loop limit
      }

      const uint32_t sub = c->mem_r32(cursor + SUBPART_ARRAY);
      const uint32_t xf = sub + SUB_TRANSFORM;

      // this sub-part's transform: rotation (0..4) then translation (5..7)
      gte_write_ctrl(0, c->mem_r32(xf + 0));
      gte_write_ctrl(1, c->mem_r32(xf + 4));
      gte_write_ctrl(2, c->mem_r32(xf + 8));
      gte_write_ctrl(3, c->mem_r32(xf + 12));
      gte_write_ctrl(4, c->mem_r32(xf + 16));
      gte_write_ctrl(5, c->mem_r32(xf + 20));
      gte_write_ctrl(6, c->mem_r32(xf + 24));
      gte_write_ctrl(7, c->mem_r32(xf + 28));

      cursor += 4;
      c->r[4] = c->mem_r32(sub + SUB_GEOMBLK);
      c->r[5] = tomba2::render::OrderingTable::active(*c).base();
      c->r[31] = 0x8003F230u; // jal-site ra
      c->r[6] = passThrough;
      const auto part = c->emission.instance(eng(c).graphicsBind.records.object(sub));
      psx::cpu::dispatchGuestToReturn0(*c, 0x8003F698u, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);

      index += 1;
    } while ((int32_t)index < (int32_t)(uint32_t)c->mem_r8(node + NODE_COUNT));
  }

  c->r[31] = c->mem_r32(frame + 76);
  c->r[20] = c->mem_r32(frame + 72);
  c->r[19] = c->mem_r32(frame + 68);
  c->r[18] = c->mem_r32(frame + 64);
  c->r[17] = c->mem_r32(frame + 60);
  c->r[16] = c->mem_r32(frame + 56);
  c->r[29] += 80;
}

static void ov_subpart_walk(Core *c) {
  Render::subPartWalk(c);
}

void subpart_walk_install() {
  tomba::native::declareOverride(
      0x8003F174u, "Render::subPartWalk", ov_subpart_walk, psx::present::Producer{psx::present::Arg::A0});
}
