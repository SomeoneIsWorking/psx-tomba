// game/render/subpart_walk_shared.cpp — Render::sharedTransformWalk, guest FUN_8003F07C.
//
// RE'd 2026-07-22 from guest 0x8003F07C. The SIBLING of Render::subPartWalk (FUN_8003F174), and the
// pair is easiest understood together:
//
//   subPartWalk (F174)          per sub-part: load THAT PART'S transform (sub+0x18), then submit it
//   sharedTransformWalk (F07C)  load ONE transform ONCE, then submit every sub-part under it
//
// So F174 draws an ARTICULATED node whose parts move independently, and this draws a RIGID one whose
// parts are fixed relative to each other. Same node layout, same two counts, same submit call — the
// only difference is where the transform comes from and how often it is loaded.
//
// The shared transform is read from SCRATCHPAD at 0x1F8000F8 (eight control words: rotation in GTE
// regs 0..4, translation in 5..7). That is the current view/camera matrix the frame set up, which is
// why a rigid node needs nothing of its own.
//
// The two counts behave exactly as in the sibling, and the same warning applies: `node[+8]` is tested
// at the TOP of each iteration and ends the walk immediately, `node[+9]` is the do/while bound at the
// bottom, and `node[+8] == 0` draws nothing however large `node[+9]` is. Note also that this function
// checks `node[+8]` BEFORE loading the transform at all, so an empty node costs nothing.
#include "core.h"
#include "core/entry/game_ctx.h" // eng()
#include "core/overrides/native_override_catalog.h"
#include "guest_call.h"
#include "ordering_table.h"
#include "render.h"

namespace {

constexpr uint32_t VIEW_TRANSFORM = 0x1F8000F8u; // scratchpad: 8 GTE control words (the frame's view)
constexpr uint32_t SUBPART_ARRAY = 0xC0u;        // node+0xC0: sub-part pointers, stride 4
constexpr uint32_t SUB_GEOMBLK = 0x40u;
constexpr uint32_t NODE_LIMIT = 8u;
constexpr uint32_t NODE_COUNT = 9u;

} // namespace

// ORACLE: guest 0x8003F07C
void Render::sharedTransformWalk(Core *c) {
  const uint32_t node = c->r[4];
  const uint32_t passThrough = c->r[5];

  c->r[29] -= 40;
  const uint32_t frame = c->r[29];
  c->mem_w32(frame + 24, c->r[18]);
  c->mem_w32(frame + 36, c->r[31]);
  c->mem_w32(frame + 32, c->r[20]);
  c->mem_w32(frame + 28, c->r[19]);
  c->mem_w32(frame + 20, c->r[17]);
  c->mem_w32(frame + 16, c->r[16]);

  if (c->mem_r8(node + NODE_LIMIT) != 0) {
    // one shared transform for the whole node — rotation (0..4) then translation (5..7)
    gte_write_ctrl(0, c->mem_r32(VIEW_TRANSFORM + 0));
    gte_write_ctrl(1, c->mem_r32(VIEW_TRANSFORM + 4));
    gte_write_ctrl(2, c->mem_r32(VIEW_TRANSFORM + 8));
    gte_write_ctrl(3, c->mem_r32(VIEW_TRANSFORM + 12));
    gte_write_ctrl(4, c->mem_r32(VIEW_TRANSFORM + 16));
    gte_write_ctrl(5, c->mem_r32(VIEW_TRANSFORM + 20));
    gte_write_ctrl(6, c->mem_r32(VIEW_TRANSFORM + 24));
    gte_write_ctrl(7, c->mem_r32(VIEW_TRANSFORM + 28));

    if (c->mem_r8(node + NODE_COUNT) != 0) {
      int32_t index = 0;
      uint32_t cursor = node;

      do {
        if (index >= (int32_t)(uint32_t)c->mem_r8(node + NODE_LIMIT)) {
          break;
        }

        const uint32_t sub = c->mem_r32(cursor + SUBPART_ARRAY);
        c->r[4] = c->mem_r32(sub + SUB_GEOMBLK);
        c->r[5] = tomba2::render::OrderingTable::active(*c).base();
        c->r[31] = 0x8003F140u; // jal-site ra
        c->r[6] = passThrough;
        const auto part = c->emission.instance(eng(c).graphicsBind.records.object(sub));
        psx::cpu::dispatchGuestToReturn0(*c, 0x8003F698u, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);

        index += 1;
        cursor += 4;
      } while (index < (int32_t)(uint32_t)c->mem_r8(node + NODE_COUNT));
    }
  }

  c->r[31] = c->mem_r32(frame + 36);
  c->r[20] = c->mem_r32(frame + 32);
  c->r[19] = c->mem_r32(frame + 28);
  c->r[18] = c->mem_r32(frame + 24);
  c->r[17] = c->mem_r32(frame + 20);
  c->r[16] = c->mem_r32(frame + 16);
  c->r[29] += 40;
}

static void ov_shared_transform_walk(Core *c) {
  Render::sharedTransformWalk(c);
}

void shared_transform_walk_install() {
  tomba::native::declareOverride(0x8003F07Cu,
                                 "Render::sharedTransformWalk",
                                 ov_shared_transform_walk,
                                 psx::present::Producer{psx::present::Arg::A0});
}
