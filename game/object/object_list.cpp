// ObjectList::walkAll / walkAux — see object_list.h.
//
// Faithful ports of guest FUN_8007A904 / FUN_80069B28. Both use BehaviorDispatch::dispatchObj
// (game/object/behavior_dispatch.h), the same native-or-substrate handler dispatch used by
// TransitionState3::walkOnce. The `behhist` diagnostic (from the pre-restructure call_handler)
// is preserved in the main walk since it feeds top-down ownership decisions.
#include "object_list.h"
#include "core.h"
#include "engine_types.h" // T2_OBJLIST_HEAD_1/2, T2OBJ_HANDLER/NEXT/RENDER_FLAG
#include "entry/session.h"
#include "game.h"
#include "render.h" // rend(c)->margin.flush
#include <lucent/log.h>
#include <stdint.h>
#include <stdio.h>

namespace tomba {

namespace {

// Per-node handler dispatch: native-or-substrate route via
// BehaviorDispatch (was static call_handler + walk_list helpers). Local to this TU. The `behhist`
// diagnostic feeds top-down ownership decisions, so it lives here on the main walk path.
inline void call_handler(Core *c, uint32_t node) {
  uint32_t h = c->mem_r32(node + T2OBJ_HANDLER);
  if (lucent::channel_on("behhist")) {
    ObjectList &ol = eng(c).objectList;
    uint32_t *addr = ol.mBehAddr;
    long *cnt = ol.mBehCnt;
    int &nh = ol.mBehN;
    long &w = ol.mBehW;
    int i = 0;
    for (; i < nh; i++) {
      if (addr[i] == h) {
        break;
      }
    }
    if (i == nh && nh < 64) {
      addr[nh] = h;
      cnt[nh] = 0;
      nh++;
    }
    if (i < 64) {
      cnt[i]++;
    }
    if ((++w % 300) == 0) {
      lucent::info("behhist", "distinct={} handlers:", nh);
      for (int j = 0; j < nh; j++) {
        lucent::info("behhist", "   {:08X}  x{}", addr[j], cnt[j]);
      }
    }
  }
  eng(c).behaviors.dispatchObj({.object = node, .handler = h});
}

inline void walk_list(Core *c, uint32_t head, long *count) {
  for (uint32_t n = head; n;) {
    uint32_t next = c->mem_r32(n + T2OBJ_NEXT);
    c->mem_w8(n + T2OBJ_RENDER_FLAG, 0);
    call_handler(c, n);
    n = next;
    (*count)++;
  }
}

} // namespace

void ObjectList::walkAll() {
  Core *c = core;
  long nodes = 0;
  walk_list(c, c->mem_r32(T2_OBJLIST_HEAD_1), &nodes);
  walk_list(c, c->mem_r32(T2_OBJLIST_HEAD_2), &nodes);
  rend(c)->margin.flush(c);

  if ((mWalksAll % 300) == 0) {
    lucent::debug("engine", "objwalk #{}: {} nodes", mWalksAll, nodes);
  }
  mWalksAll++;
}

void ObjectList::walkList2() {
  Core *c = core;
  long nodes = 0;
  walk_list(c, c->mem_r32(T2_OBJLIST_HEAD_2), &nodes);
  // NB: no MarginRenderer::flush here — that belongs to walkAll (walkList2 is a distinct call site
  // from Sop::fieldUpdate, not a replacement of the whole entity walk).

  if ((mWalksL2 % 300) == 0) {
    lucent::debug("engine", "objwalk_l2 #{}: {} nodes", mWalksL2, nodes);
  }
  mWalksL2++;
}

void ObjectList::walkAux() {
  Core *c = core;
  // FUN_80069B28: does NOT clear the render flag; dispatches per handler ptr via the shared path.
  for (uint32_t n = c->mem_r32(AUX_LIST_HEAD); n;) {
    uint32_t h = c->mem_r32(n + 0x1Cu);
    uint32_t next = c->mem_r32(n + 0x24u);
    eng(c).behaviors.dispatchObj({.object = n, .handler = h});
    n = next;
  }
}

} // namespace tomba
