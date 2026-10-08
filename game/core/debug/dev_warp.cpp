#include "debug/dev_warp.h"

#include "core.h"
#include "engine/engine.h"
#include "entry/game_ctx.h"
#include "guest_call.h"

#include <cstdint>
#include <cstdio>
#include <lucent/log.h>

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

std::string DevWarp::arm(Core &core, const char *line) {
  unsigned area = 0;
  unsigned sub = 0;
  const int parsed = std::sscanf(line, "%*s %u %u", &area, &sub);
  if (parsed < 1) {
    return "usage: warp <area> [sub]";
  }
  if (!Engine::devWarpAllowed(&core)) {
    return "refused: a warp is legal only once the game stage is running";
  }
  const int count = Engine::devAreaCount();
  if (area >= static_cast<unsigned>(count)) {
    return lucent::format("refused: area {} is out of range, this game has {} areas (0..{})", area, count, count - 1);
  }
  area_ = static_cast<int>(area);
  sub_ = parsed == 2 ? static_cast<int>(sub) : 0;
  armed_ = true;
  return lucent::format("ok: warp armed for area {} sub {}", area_, sub_);
}

void DevWarp::applyArmed(Core &core, uint32_t frame) {
  if (!armed_) {
    return;
  }
  armed_ = false;
  applyColdWarp(core, area_, sub_ & 0x3f);
  lucent::info("warp", "cold area {} sub {} loaded at f{}", area_ & 0x1f, sub_ & 0x3f, frame);
}

} // namespace tomba
