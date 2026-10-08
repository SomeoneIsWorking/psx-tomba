#include "debug/dev_warp.h"

#include "core.h"
#include "entry/game_ctx.h"
#include "guest_call.h"

#include <cstdint>

namespace tomba {

void applyColdWarp(Core &core, int area, int sub) {
  Core *c = &core;
  const uint32_t dest = static_cast<uint32_t>(area) & 0x1fu;
  const uint32_t wsm = c->mem_r32(0x1f800138u);
  psx::cpu::callGuestNow(*c, __func__, 0x80074E48u); // stop the current song, as every area transition does
  c->mem_w8(wsm + 0x6e, static_cast<uint8_t>(dest));
  c->mem_w8(wsm + 0x6d, 2);
  eng(c).sop.transitionAreaLoad();
  c->mem_w8(0x800bf871u, static_cast<uint8_t>(static_cast<uint32_t>(sub) & 0x3fu));
  c->mem_w8(0x800bf839u, 0); // no pending door transition after a completed cold warp
  c->mem_w16(wsm + 0x48, 2);
  c->mem_w16(wsm + 0x4a, 1);
  c->mem_w16(wsm + 0x4c, c->mem_r8(0x80108f60u + dest));
  c->mem_w16(wsm + 0x4e, 0);
  eng(c).sop.transitionAreaEnter();
}

} // namespace tomba
