// Array8Dispatch::tick — see array8_dispatch.h. Faithful port of guest FUN_80026368.
#include "array8_dispatch.h"
#include "core.h"
#include "entry/tomba_session.h"
#include "game.h"

namespace tomba {

void Array8Dispatch::tick() {
  Core *c = core;
  for (int i = 0; i < 8; i++) {
    uint32_t slot = ARRAY_BASE + (uint32_t)i * SLOT_STRIDE;
    if (c->mem_r8(slot) == 0) {
      continue;
    }
    uint32_t type = c->mem_r8(slot + 2);
    uint32_t h = c->mem_r32(METHOD_TABLE + type * 4u);
    eng(c).behaviors.dispatchObj({.object = slot, .handler = h});
  }
}

} // namespace tomba
