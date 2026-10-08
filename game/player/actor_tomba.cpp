// game/player/actor_tomba.cpp — the Tomba actor: his per-frame G-block driver, the two item walks,
// the growth and motion steps, and the outer-transition/asset gating.
//
// G IS TOMBA'S NODE — the master block at 0x800E7E80 (see game/player/tomba_state.h for the lens
// and the address vocabulary). What touching an item MEANS is ActorInteraction
// (game/player/actor_interaction.{h,cpp}); this class owns the walks that choose which items to hand
// it, and everything else Tomba does in a frame. Addresses, field layouts and the per-cluster call
// graph are in docs/engine_re.md's "ActorTomba G-block" section and docs/codemap.md.
#include "actor_tomba.h"

#include "core.h"
#include "core/entry/game_ctx.h"
#include "core/overrides/guest_jal.h"
#include "core/overrides/native_override_catalog.h"
#include "game.h"
#include "guest_abi.h"
#include "guest_call.h"
#include "player/tomba_state.h"

#include <cstdint>

namespace tomba::player {

// =================================================================================
// Per-frame interaction walk (FUN_80022760)
// =================================================================================
void ActorTomba::interactWalk() {
  Core *c = core;
  const uint32_t G = G_ADDR;
  TombaState tomba{c, G};

  // Early-outs.
  if (tomba.turnCurrent() == 0) {
    return;
  }
  if (c->mem_r8(kGateBF80C) != 0) {
    return;
  }
  if (tomba.growthFlags() & 0x200u) {
    return;
  }

  const uint32_t listBase = c->mem_r32(kAuxListHead);
  const uint8_t count0 = c->mem_r8(kAuxListCount);
  c->mem_w8(kGateBF80C, 0); // guest instruction path's initial `uVar2=0` write
  c->mem_w8(kAuxWalkCounter, count0);

  uint32_t cursor = listBase;
  while (c->mem_r8(kAuxWalkCounter) != 0) {
    const uint32_t item = c->mem_r32(cursor);
    c->mem_w8(kAuxWalkCounter, (uint8_t)(c->mem_r8(kAuxWalkCounter) - 1));
    cursor += 4;
    if (c->mem_r8(item) != 1) {
      continue; // item[0]!=1 → skip
    }

    const uint8_t typ = c->mem_r8(item + 2u);
    switch (typ) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 7:
      interaction_.proximityCheck(item);
      break;
    case 4:
      if (c->mem_r8(item + 0x5Eu) == 2) {
        interaction_.type4GuardedCheck(item);
      } else {
        interaction_.proximityCheck(item);
      }
      break;
    case 6:
      interaction_.subHitboxCheck(item);
      break;
    default:
      break;
    }
  }
}

// =================================================================================
// Post-interact walk (FUN_801130C4) — the default-mode "post-tick" that runs after interactWalk
// =================================================================================
void ActorTomba::postInteractWalk() {
  Core *c = core;
  const uint32_t G = G_ADDR;
  TombaState tomba{c, G};

  // This walker uses a DIFFERENT aux list than interactWalk: the render/interaction queue at
  // *0x1F80013C with count *0x1F800144 (vs 0x1F800154 / 0x1F80015C for interactWalk).
  constexpr uint32_t LIST_HEAD_SPAD = 0x1F80013Cu;
  constexpr uint32_t LIST_COUNT_SPAD = 0x1F800144u;

  constexpr uint32_t LEAF_TYPE_9_SPECIAL = 0x80111304u; // item[0xC]==9 guarded handler
  constexpr uint32_t LEAF_TYPE_3 = 0x8010E258u;
  constexpr uint32_t LEAF_TYPE_4_PROX_STEP = 0x8001F40Cu; // case-4 proximity + return code
  constexpr uint32_t LEAF_TYPE_4_TAG_SET = 0x8001FDB4u;   // case-4 alt-tag write
  constexpr uint32_t LEAF_TYPE_7 = 0x800235A0u;
  constexpr uint32_t LEAF_TYPE_8 = 0x800205CCu;
  constexpr uint32_t LEAF_TYPE_0F_14_56 = 0x80020364u;
  constexpr uint32_t LEAF_TYPE_13 = 0x8010EA80u;

  uint32_t cursor = c->mem_r32(LIST_HEAD_SPAD);
  c->mem_w8(kAuxWalkCounter, c->mem_r8(LIST_COUNT_SPAD));

  while (c->mem_r8(kAuxWalkCounter) != 0) {
    const uint32_t item = c->mem_r32(cursor);
    c->mem_w8(kAuxWalkCounter, (uint8_t)(c->mem_r8(kAuxWalkCounter) - 1));
    cursor += 4;
    if ((c->mem_r8(item) & 1) == 0) {
      continue; // active-flag gate
    }
    if (c->mem_r8(item + 0xCu) == 9) { // special-type items
      if ((tomba.growthFlags() & 0x8200u) == 0) {
        tomba::guest::dispatchLeafToReturn(*c, LEAF_TYPE_9_SPECIAL, G, item);
      }
      continue; // keep walking
    }
    const uint8_t typ = c->mem_r8(item + 2u);
    switch (typ) {
    case 3:
      tomba::guest::dispatchLeafToReturn(*c, LEAF_TYPE_3, G, item);
      break;
    case 4: {
      // Detailed guarded state-transition (case 4). Faithful to the guest instruction path:
      //   * dispatch the proximity/step leaf with a2=1; if v0 < 0 → skip (no interaction).
      //   * stop the walk (WALK_COUNTER = 0) unconditionally after we enter case 4.
      //   * bonus-tag path: DAT_800BF9E5 == 6 && G+0x144 == 1 && v0 < 2 →
      //         FUN_8001FDB4(item, 0xFFFF8001, 0x10, 0x20); continue walking.
      //   * silence path: skip if 0x1F800137 != 0 OR G[0] & 6 OR G+0x144 > 1 OR G+0x164 != 0.
      //   * else: DAT_800BF9E5 != 6 → announcer cue 0x2A/0x41; stamp G/item state as the
      //     "type-4 hit" transition (see writes below).
      const int32_t v0 = (int32_t)tomba::guest::dispatchLeafToReturn(*c, LEAF_TYPE_4_PROX_STEP, G, item, 1u);
      if (v0 < 0) {
        break; // no hit
      }
      c->mem_w8(kAuxWalkCounter, 0); // stop the walk
      const uint8_t bf9e5 = c->mem_r8(0x800BF9E5u);
      const uint8_t g144 = tomba.justTransitioned();
      if (bf9e5 == 6 && g144 == 1 && v0 < 2) {
        tomba::guest::dispatchLeafToReturn(*c, LEAF_TYPE_4_TAG_SET, item, 0xFFFF8001u, 0x10u, 0x20u);
        break; // continue at loop top (via while)
      }
      if (c->mem_r8(0x1F800137u) != 0) {
        break;
      }
      if ((tomba.statusFlags() & 6) != 0) {
        break;
      }
      if (g144 > 1) {
        break;
      }
      if (tomba.transitionSlot() != 0) {
        break;
      }
      if (bf9e5 != 6) {
        eng(c).announcerCue(0x2A, 0x41); // native FUN_8004ED94
      }
      // Type-4 hit state transition on G + item.
      c->mem_w8(G + 4, 2);
      c->mem_w8(G + 5, 2);
      tomba.setStatusFlags(3);
      c->mem_w8(G + 6, 0);
      c->mem_w8(G + 0x172u, 0x78);
      c->mem_w8(G + 0x173u, 0);
      c->mem_w8(G + 0x2Bu, (uint8_t)((int32_t)c->mem_r32(kOutHeadingSlot) >> 4));
      break;
    }
    case 7:
      tomba::guest::dispatchLeafToReturn(*c, LEAF_TYPE_7, G, item);
      break;
    case 8:
      tomba::guest::dispatchLeafToReturn(*c, LEAF_TYPE_8, G, item);
      break;
    case 0x0F:
    case 0x14:
    case 0x56:
      tomba::guest::dispatchLeafToReturn(*c, LEAF_TYPE_0F_14_56, G, item, 0u);
      break;
    case 0x13:
      tomba::guest::dispatchLeafToReturn(*c, LEAF_TYPE_13, G, item);
      break;
    case 0x2F:
      tomba::guest::dispatchLeafToReturn(*c, LEAF_TYPE_0F_14_56, G, item, 2u);
      break;
    default:
      break; // no interaction for other types
    }
  }
}

// =================================================================================
// Growth / shrink transformation (FUN_80057DC0)
// =================================================================================
void ActorTomba::growthStep(int32_t mode) {
  Core *c = core;
  const uint32_t G = G_ADDR;

  const uint16_t f17E = c->mem_r16(G + 0x17Eu);
  const int16_t posY = (int16_t)c->mem_r16(G + 0x32u);
  uint16_t newFlag;
  if (mode == 0) {
    if (f17E & 0x8000) {
      c->mem_w16(G + 0x32u, (uint16_t)(posY - 0x46)); // shrink → drop feet
    }
    newFlag = (uint16_t)(f17E & 0x7FFF);
  } else {
    if ((f17E & 0x8000) == 0) {
      c->mem_w16(G + 0x32u, (uint16_t)(posY + 0x46)); // grow → raise feet
    }
    newFlag = (uint16_t)(f17E | 0x8000);
  }
  c->mem_w16(G + 0x17Eu, newFlag);

  const int32_t divisor = mode + 1;
  const int16_t s1000 = (int16_t)(0x1000 / divisor);
  const int16_t s32_ = (int16_t)(0x32 / divisor);
  const int16_t s100 = (int16_t)(100 / divisor);
  const int16_t s8C = (int16_t)(0x8C / divisor);
  const int16_t s10E = (int16_t)(0x10E / divisor);
  const int16_t s1E = (int16_t)(0x1E / divisor);
  const int16_t sF0 = (int16_t)(0xF0 / divisor);
  c->mem_w16(G + 0xB8u, (uint16_t)s1000);
  c->mem_w16(G + 0xBAu, (uint16_t)s1000);
  c->mem_w16(G + 0xBCu, (uint16_t)s1000);
  c->mem_w16(G + 0x80u, (uint16_t)s32_);
  c->mem_w16(G + 0x82u, (uint16_t)s100);
  c->mem_w16(G + 0x84u, (uint16_t)s8C);
  c->mem_w16(G + 0x86u, (uint16_t)s10E);
  c->mem_w16(G + 0x62u, (uint16_t)s8C);
  c->mem_w16(G + 0x64u, (uint16_t)s8C);
  c->mem_w16(G + 0x66u, (uint16_t)s100);
  c->mem_w16(G + 0x68u, (uint16_t)s1E);
  c->mem_w16(kGrowthMirrorHalfword, (uint16_t)sF0);
}

// =================================================================================
// Post-frame water/sea check (FUN_8010E904 — final call in area_seaside_perframe)
// =================================================================================
void ActorTomba::postFrameWaterCheck() {
  Core *c = core;
  const uint32_t G = G_ADDR;
  TombaState tomba{c, G};

  constexpr uint32_t WATER_MODE_BYTE = 0x800BF816u;
  constexpr uint32_t WATER_LEVEL_S16 = 0x800BF812u; // water surface Y (s16)
  constexpr uint32_t WATER_STATE_BYTE = 0x800BF817u;
  constexpr uint32_t PAUSE_FLAG_SPAD = 0x1F800137u;
  constexpr uint32_t LEAF_DRY_TICK = 0x8010E408u;     // per-frame Tomba tick when not in water
  constexpr uint32_t LEAF_WATER_SPLASH = 0x80022C78u; // Y-snap tail (particle spawn?)

  const int16_t waterLevel = (int16_t)c->mem_r16(WATER_LEVEL_S16);
  const uint8_t waterMode = c->mem_r8(WATER_MODE_BYTE);
  const uint8_t waterState = c->mem_r8(WATER_STATE_BYTE);

  if (waterMode == 0) {
    // Dry land: run the per-frame Tomba tick if not paused.
    if (c->mem_r8(PAUSE_FLAG_SPAD) == 0) {
      tomba::guest::dispatchLeafToReturn(*c, LEAF_DRY_TICK, G);
    }
  } else {
    // Water/sea mode. When water-state is 2 with a specific 800E7FEB (== 8) config, clamp
    // Tomba's Z to the water-region edge — matches the guest instruction path's `< 0x1a05 → 0x1a04` snap.
    bool skipYSnap = false;
    if (waterState > 1) {
      if (waterState == 2 && c->mem_r8(0x800E7FEBu) == 8) {
        if (tomba.posZ() < 0x1A05) {
          tomba.setPosZ(0x1A04);
        }
      } else {
        skipYSnap = true; // guest instruction path: `goto LAB_8010E9D4;` skips the Y block
      }
    }
    if (!skipYSnap) {
      if ((tomba.groundedGate() & 1) == 0 &&
          (int32_t)waterLevel - (int32_t)tomba.physOffsetY() <= (int32_t)tomba.posY()) {
        tomba.setPosY((int16_t)(waterLevel - tomba.physOffsetY())); // Y = waterLevel - G+0x62
        tomba::guest::dispatchLeafToReturn(*c, LEAF_WATER_SPLASH, G);
      }
    }
  }

  // Area-exit trigger — fires only in water-mode 2 when Tomba is off-map.
  if (tomba.committing() != 0) {
    return;
  }
  if (c->mem_r8(0x800BF80Du) != 0) {
    return;
  }
  if (c->mem_r8(0x800BF839u) != 0) {
    return;
  }
  if (waterMode == 0) {
    return;
  }
  if (waterState != 2) {
    return;
  }
  if (tomba.posY() >= -0xE74) {
    return;
  }
  if (tomba.posZ() >= 0x1451) {
    return;
  }

  c->mem_w8(PAUSE_FLAG_SPAD, waterState);
  c->mem_w8(0x800BF80Fu, waterState);
  c->mem_w16(0x800BF83Au, 0x100);
  c->mem_w8(0x800BF839u, 1);
  c->mem_w8(0x1F800236u, 1);
}

// =================================================================================
// postInteractWalk sub-handlers — band 0x80020000-0x8002FFFF. RE'd + drafted 2026-07-08 from
// Ghidra headless (scratch/decomp/region_8002.c) cross-checked against authenticated executable/overlay evidence
// (ground truth for the guest-stack frame + jal-site `ra` constants). UNWIRED: postInteractWalk
// above still reaches these via typed runtime address dispatch(c, LEAF_TYPE_*) — wiring these methods in requires
// adding override-registry entries, deliberately left for the next frontier
// pass so this draft compiles as dead code only.
// =================================================================================
namespace {
constexpr uint32_t LEAF_PROX_STEP =
    0x8001F40Cu; // FUN_8001F40C — shared proximity+step (== postInteractWalk's LEAF_TYPE_4_PROX_STEP)
constexpr uint32_t LEAF_ALT_TAG_SET =
    0x8001FDB4u; // FUN_8001FDB4 — alt-tag stamp (== postInteractWalk's LEAF_TYPE_4_TAG_SET)
constexpr uint32_t LEAF_GROWN_PUSH =
    0x8001F054u; // FUN_8001F054 — grown-state push (stepModeInteract's 0x8000-set/mode&3 branch)
constexpr uint32_t LEAF_NILADIC_CUE = 0x8001F830u; // FUN_8001F830 — niladic cue (type8Interact's item[0]==5 branch)
constexpr uint32_t LEAF_GROWN_DELEGATE =
    0x8001EC3Cu; // FUN_8001EC3C — whole-hog grown-state delegate (type8Interact's 0x8000-set branch)
constexpr uint32_t LEAF_STEP_MODE_FLAG = 0x8001FF7Cu; // FUN_8001FF7C — type7Interact's mode/flag call
} // namespace

// FUN_80022C78 — leaf, no guest-stack frame. Operates on G (postFrameWaterCheck's
// LEAF_WATER_SPLASH call site).
void ActorTomba::growthYSnap() {
  Core *c = core;
  const uint32_t G = G_ADDR;

  c->mem_w8(G + 0x29u, 1);
  c->mem_w8(G + 0x145u, 0);
  c->mem_w16(G + 0x4Au, 0);
  c->mem_w16(G + 0x50u, 0);
  c->mem_w8(G + 0x148u, 0);

  if (c->mem_r8(G + 0x78u) != 0) {
    return;
  }
  if (c->mem_r8(0x800BF816u) != 0) {
    return;
  }

  // BUG FIX (RE cross-check against authenticated executable/overlay evidence guest 0x80022C78): the ground
  // truth's `if (g17E<0) goto L_80022CD8` branch jumps to a block that explicitly re-sets r3=70
  // (0x46) for the comparison/subtraction constant; the FALLTHROUGH (g17E>=0) keeps r3=140
  // (0x8C) from the branch's own delay-slot preset. So g17E<0 -> 0x46, g17E>=0 -> 0x8C — the
  // original draft had this backwards. Same polarity bug was inlined at type8Interact's "just
  // left growth" tail (which reuses this constant indirectly via growthYSnap()), so this one fix
  // corrects both call sites.
  const int16_t g17E = (int16_t)c->mem_r16(G + 0x17Eu);
  const int16_t k = (g17E < 0) ? 0x46 : 0x8C;
  const int16_t g84 = (int16_t)c->mem_r16(G + 0x84u);
  if (g84 == k) {
    return; // no-op — already at the snap point
  }
  c->mem_w16(G + 0x32u, (uint16_t)(g84 + ((int16_t)c->mem_r16(G + 0x32u) - k)));
}

// =================================================================================
// Settle helper — velocityIntegrate's tail dispatch (FUN_80054650)
// =================================================================================
uint32_t ActorTomba::settleStep(int32_t mode) {
  Core *c = core;
  const uint32_t G = G_ADDR;

  c->mem_w8(0x1F800258u, 0);                                    // clear sink-mark
  c->mem_w8(G + 0x5Fu, (uint8_t)(c->mem_r8(G + 0x5Fu) & 0xFB)); // flag95 &= ~0x04

  if (c->mem_r8(G + 0x16Bu) != 0) {
    c->r[2] = 0;
    return 0;
  } // flag363 gate

  // Probe offset selector: mode==0 default 0x1E / 0x3C (when G+0x17E has high bit clear),
  // mode!=0 always 0.
  uint32_t probeOffset = 0;
  if (mode == 0) {
    probeOffset = ((int16_t)c->mem_r16(G + 0x17Eu) >= 0) ? 0x3Cu : 0x1Eu;
  }

  // Probe base: G+0x62 (u16) unless G+0x78 (state) != 0, in which case pull the "hooked item" at
  // G+0x10 and compute `(item[+0x86] - item[+0x84]) - (G+0x32 - item[+0x32])`.
  int32_t base;
  if (c->mem_r8(G + 0x78u) == 0) {
    base = (int32_t)c->mem_r16s(G + 0x62u);
  } else {
    const uint32_t item = c->mem_r32(G + 0x10u);
    base = ((int32_t)c->mem_r16s(item + 0x86u) - (int32_t)c->mem_r16s(item + 0x84u)) -
           ((int32_t)c->mem_r16s(G + 0x32u) - (int32_t)c->mem_r16s(item + 0x32u));
  }
  const int16_t half = (int16_t)(base / 2);

  auto probe = [&](int16_t offset) -> int32_t {
    c->r[4] = G;
    c->r[5] = probeOffset;
    c->r[6] = (uint32_t)(int32_t)offset;
    psx::cpu::dispatchGuestToReturn0(
        *c, 0x8004954Cu, psx::cpu::ExecutionBudget::currentTurn(*c), __func__); // grid probe
    return (int32_t)c->r[2];
  };

  if (probe(half) != 0 || probe((int16_t)(-half)) != 0) {
    uint8_t bV = (uint8_t)(c->mem_r8(G + 0x149u) & 1);
    if ((c->mem_r8(G + 0x149u) & 4) == 0) {
      bV = c->mem_r8(G + 0x147u);
    }
    c->mem_w8(G + 0x60u, 1);
    c->mem_w8(G + 0x5Fu, (uint8_t)(bV + 4));
    c->r[2] = 1;
    return 1;
  }

  // No probe hit — check the sink-mark and fall through with 0.
  if (c->mem_r8(0x1F800258u) != 0) {
    const int8_t v = (int8_t)(5 - (int8_t)c->mem_r8(G + 0x147u));
    c->mem_w8(G + 0x5Fu, (uint8_t)v);
  }
  c->r[2] = 0;
  return 0;
}

// =================================================================================
// Movement — velocity integrate (FUN_80056B48)
// =================================================================================
void ActorTomba::velocityIntegrate(bool suppressY) {
  Core *c = core;
  const uint32_t G = G_ADDR;

  const int32_t speed = c->mem_r16s(G + 0x44u);
  const int32_t dirX = c->mem_r16s(G + 0x48u);
  const int32_t dirZ = c->mem_r16s(G + 0x4Cu);
  c->mem_w32(G + 0x2Cu, c->mem_r32(G + 0x2Cu) + (uint32_t)(dirX * speed)); // posX
  c->mem_w32(G + 0x34u, c->mem_r32(G + 0x34u) + (uint32_t)(dirZ * speed)); // posZ

  if (!suppressY) {
    const int32_t dirY = c->mem_r16s(G + 0x4Au);
    c->mem_w32(G + 0x30u, c->mem_r32(G + 0x30u) + (uint32_t)(dirY * speed)); // posY
  }

  // Tail: settle-helper dispatch OR flag95 &= ~0x04.
  if (c->mem_r8(G + 0x16Bu) == 0 && c->mem_r8(G + 0x61u) == 0) {
    settleStep(0); // native FUN_80054650
  } else {
    const uint8_t f = (uint8_t)(c->mem_r8(G + 0x5Fu) & 0xFB);
    c->mem_w8(G + 0x5Fu, f);
    c->r[2] = f;
  }
}

void ActorTomba::mode0ActionGate() {
  Core *c = core;
  const uint32_t G = G_ADDR;
  static constexpr GuestFrameSpill kSpills[] = {{31, 16}};
  GuestFrame<24, 1> frame(c, kSpills);
  bool pathA = c->mem_r8(0x800BF816u) != 0                // water mode on
               || c->mem_r8(G + 0x17Cu) == 0              // action-enable byte clear
               || (c->mem_r16(G + 0x17Eu) & 0x640u) != 0; // a suppress bit set
  if (pathA) {
    tomba::guest::dispatchJalToReturn(*c, 0x8005A970u, 0x8005A950u); // normal handler (direct same-shard in gen)
  } else {
    tomba::guest::dispatchJalToReturn(*c, 0x80112B50u, 0x8005A960u); // swim/water-interaction handler
  }
}

void ActorTomba::ov_stepModeInteract(Core *c) {
  const uint32_t item = c->r[5];
  const uint32_t mode = c->r[6];
  c->r[2] = eng(c).actorTomba.interaction_.stepModeInteract(item, mode);
}

void ActorTomba::ov_type8Interact(Core *c) {
  const uint32_t item = c->r[5];
  eng(c).actorTomba.interaction_.type8Interact(item);
}

void ActorTomba::ov_type7Interact(Core *c) {
  const uint32_t item = c->r[5];
  c->r[2] = eng(c).actorTomba.interaction_.type7Interact(item);
}

void ActorTomba::ov_growthYSnap(Core *c) {
  eng(c).actorTomba.growthYSnap();
}

void ActorTomba::ov_frameTick(Core *c) {
  eng(c).actorTomba.frameTick();
}

// ov_turnBiasCompute/ov_outerTransitionGate/ov_outerTransitionCommit/ov_assetReady — guest ABI
// trampolines for the frameTick sub-callee cluster (§9 re-verified + wired 2026-07-10). Guest ABI
// per the cited guest instructions (see the definitions above for the cited call sites): turnBiasCompute takes
// facing in a1 (a0=G is unused — the guest body never reads r4 in this leaf); outerTransitionGate/
// outerTransitionCommit always operate on Tomba's single fixed G block (a0=G is always G_ADDR, so
// the instance methods read G_ADDR directly rather than c->r[4]); outerTransitionCommit takes mode
// in a1; assetReady takes slot in a0 (NOT a1 — guest 0x80045580 uses r4<<3 directly).
void ActorTomba::ov_turnBiasCompute(Core *c) {
  turnBiasCompute(c, (int16_t)c->r[5]);
}

void ActorTomba::ov_outerTransitionGate(Core *c) {
  c->r[2] = eng(c).actorTomba.outerTransitionGate() ? 1u : 0u;
}

void ActorTomba::ov_outerTransitionCommit(Core *c) {
  eng(c).actorTomba.outerTransitionCommit((int32_t)c->r[5]);
}

void ActorTomba::ov_assetReady(Core *c) {
  c->r[2] = assetReady(c, (int32_t)c->r[4]) ? 1u : 0u;
}

void ActorTomba::gov_turnBiasCompute(Core *c) {
  ov_turnBiasCompute(c);
}

void ActorTomba::gov_outerTransitionGate(Core *c) {
  ov_outerTransitionGate(c);
}

void ActorTomba::gov_outerTransitionCommit(Core *c) {
  ov_outerTransitionCommit(c);
}

void ActorTomba::gov_assetReady(Core *c) {
  ov_assetReady(c);
}

void ActorTomba::gov_mode0ActionGate(Core *c) {
  eng(c).actorTomba.mode0ActionGate();
}

void ActorTomba::registerOverrides(Game * /*game*/) {
  // typed runtime address dispatch-only postInteractWalk sub-handlers + frameTick (no direct same-module caller ->
  // setter omitted). turnBiasCompute/outerTransitionGate/outerTransitionCommit/assetReady are
  // dual-wired via tomba::native::declareOverride below (direct callers exist).
  tomba::native::declareOverride(0x80020364u, "ActorTomba::stepModeInteract", ov_stepModeInteract);
  tomba::native::declareOverride(0x800205CCu, "ActorTomba::type8Interact", ov_type8Interact);
  tomba::native::declareOverride(0x800235A0u, "ActorTomba::type7Interact", ov_type7Interact);
  tomba::native::declareOverride(0x80022C78u, "ActorTomba::growthYSnap", ov_growthYSnap);
  tomba::native::declareOverride(0x8005950Cu, "ActorTomba::frameTick", ov_frameTick);

  tomba::native::declareOverride(0x80055C9Cu, "gov_turnBiasCompute", gov_turnBiasCompute);
  tomba::native::declareOverride(0x80053E50u, "gov_outerTransitionGate", gov_outerTransitionGate);
  tomba::native::declareOverride(0x80053FDCu, "gov_outerTransitionCommit", gov_outerTransitionCommit);
  tomba::native::declareOverride(0x80045580u, "gov_assetReady", gov_assetReady);
  tomba::native::declareOverride(0x8005A910u, "gov_mode0ActionGate", gov_mode0ActionGate);
}

// turnBiasCompute — guest FUN_80055C9C. See actor_tomba.h for the full RE writeup. Frameless leaf
// (frame_size=0 per abi_extract) — no stack, purely fixed-address reads + a bias-pair write.
namespace {
constexpr uint32_t UI_MODE_BYTE = 0x800E806Cu;      // ==5 selects the wide/menu delta formula
constexpr uint32_t VIEW_HEADING_SPAD = 0x1F8000F2u; // cached view heading, subtracted from facing
constexpr uint32_t CLOSE_MASK_WORD = 0x800E805Au;   // bit 0x800 widens the "close" threshold
constexpr uint32_t TURN_BIAS_IN_SPAD = 0x1F80016Cu;
constexpr uint32_t TURN_BIAS_OUT_SPAD = 0x1F80016Eu;
} // namespace

void ActorTomba::turnBiasCompute(Core *c, int16_t facing) {
  bool closeIn;
  if (c->mem_r8(UI_MODE_BYTE) == 5) {
    // Wide/menu variant: delta from a fixed 0xC00(3072) reference minus the cached view heading
    // and facing.
    const uint32_t d = (3072u - c->mem_r16(VIEW_HEADING_SPAD) - (uint32_t)(int32_t)facing) & 4095u;
    closeIn = (int32_t)d < 2048;
  } else {
    uint32_t r3 = (3072u - (uint32_t)(int32_t)facing) & 4095u;
    const uint32_t r2m = c->mem_r16(VIEW_HEADING_SPAD) & 4095u;
    r3 = r3 - r2m;
    const uint32_t r4 = ((int16_t)r3 < 0) ? r3 : (r3 - 512u);
    const uint32_t d = r4 & 4095u;
    if (c->mem_r16(CLOSE_MASK_WORD) & 0x800u) {
      closeIn = (int32_t)d < 2560;
    } else {
      closeIn = (int32_t)d < 1536;
    }
  }
  if (closeIn) {
    c->mem_w16(TURN_BIAS_IN_SPAD, 128);
    c->mem_w16(TURN_BIAS_OUT_SPAD, 32);
  } else {
    c->mem_w16(TURN_BIAS_IN_SPAD, 32);
    c->mem_w16(TURN_BIAS_OUT_SPAD, 128);
  }
}

// resetLoadGate — guest FUN_80042310. See actor_tomba.h for the full RE writeup. Guest frame
// (abi_extract --contract): 24 B, ra@sp+16 only.
void ActorTomba::resetLoadGate(Core *c) {
  static constexpr GuestFrameSpill kSpills[] = {{31, 16}};
  GuestFrame<24, 1> frameGuard(c, kSpills);

  tomba::guest::dispatchLeafToReturn(*c, 0x8001CF78u);                            // niladic cue
  tomba::guest::dispatchJalToReturn(*c, 0x80074590u, 0x80042320u, 0x7Fu, 0u, 0u); // FUN_80074590(0x7F, 0, 0)
  const uint8_t areaMode = c->mem_r8(0x800BF870u);                                // read before the unpause write below
  c->mem_w8(0x1F800137u, 0);                                                      // unpause
  tomba::guest::dispatchJalToReturn(*c, 0x80074F24u, 0x80042320u, areaMode);      // FUN_80074F24(DAT_800BF870)
}

// assetReady — guest FUN_80045580. See actor_tomba.h for the full RE writeup. Guest frame: 24 B,
// ra@sp+16 only.
bool ActorTomba::assetReady(Core *c, int32_t slot) {
  static constexpr GuestFrameSpill kSpills[] = {{31, 16}};
  GuestFrame<24, 1> frameGuard(c, kSpills);

  constexpr uint32_t TABLE_8018A000 = 0x8018A000u;
  constexpr uint32_t DAT_800A3EC8 = 0x800A3EC8u;
  constexpr uint32_t SLOT_TABLE = 0x800BE118u; // &DAT_800be11c - 4 (slot*8 + 4 == +0xC)
  const uint32_t rec = c->mem_r32(SLOT_TABLE + (uint32_t)slot * 8u + 4u);
  const bool ready = tomba::guest::dispatchJalToReturn(
                         *c, 0x80044CD4u, 0x800455B0u, TABLE_8018A000, c->mem_r32(DAT_800A3EC8), rec) > 0;
  return ready;
}

// outerTransitionGate — guest FUN_80053E50(G). See actor_tomba.h for the full RE writeup. Guest
// frame (abi_extract --contract): 32 B; s0/s1/s2/ra spilled but never written by this body — pure
// passthrough preservation of the caller's values, which GuestFrame reproduces for free.
namespace {
constexpr uint32_t BUSY_LATCH_HI = 0x800BF81Eu;
constexpr uint32_t BUSY_LATCH = 0x800BF80Du;        // global "outer transition busy" latch
constexpr uint32_t LEAF_CUE_800521F4 = 0x800521F4u; // transition-cue dispatch (4 call sites)
constexpr uint32_t LEAF_WALK_RESET = 0x80053D90u;   // walk-state reset leaf
constexpr uint32_t LEAF_STOPMOTION = 0x800312D4u;   // stop-motion task spawn (dest ptr, magnitude)
} // namespace

bool ActorTomba::outerTransitionGate() {
  Core *c = core;
  const uint32_t G = G_ADDR;
  static constexpr GuestFrameSpill kSpills[] = {{16, 16}, {17, 20}, {18, 24}, {31, 28}};
  GuestFrame<32, 4> frameGuard(c, kSpills);
  TombaState tomba{c, G};

  if (tomba.turnCurrent() > 0) {
    return false; // still mid-turn — nothing to do yet
  }

  c->mem_w8(BUSY_LATCH_HI, 0);
  eng(c).gStateMutate(G, 0xB);

  if (tomba.transitionSlot() == 1) {
    if ((tomba.statusFlags() & 4u) == 0) {
      if ((tomba.latchFlags() & 0x80u) == 0) {
        tomba::guest::dispatchJalToReturn(*c, LEAF_CUE_800521F4, 0x80053F14u, 0u, 0x81u, 0x81u, 0x0Fu);
      }
      tomba.setLatchFlags(0);
      tomba.setStopMotionAux(0);
      tomba::guest::dispatchJalToReturn(*c, LEAF_WALK_RESET, 0x80053F24u, G);
      tomba.setStatusFlags(3);
      tomba.setTurnCurrent(0);
      tomba.setTurnTarget(0);
      tomba.setTurnSuppressGate(0);
      tomba.setOuterState(2);
      tomba.setLoadStep(1);
      tomba.setLoadSub(0);
      c->mem_w8(BUSY_LATCH, 1);
      tomba::guest::dispatchJalToReturn(*c, LEAF_STOPMOTION, 0x80053FC0u, 6u, tomba.posAddr(), (uint32_t)-80);
      return true;
    }
    if ((tomba.latchFlags() & 0x80u) != 0) {
      return true;
    }
    tomba::guest::dispatchJalToReturn(*c, LEAF_CUE_800521F4, 0x80053ED8u, 0u, 0x81u, 0x81u, 0x0Fu);
    tomba.setLatchFlags((uint8_t)(tomba.latchFlags() | 0x82u));
    return true;
  }

  if (c->mem_r8s(BUSY_LATCH) < 1) {
    tomba::guest::dispatchJalToReturn(*c, LEAF_CUE_800521F4, 0x80053F74u, 0u, 0x81u, 0x81u, 0x0Fu);
    tomba.setLatchFlags(0);
    tomba.setStopMotionAux(0);
    tomba::guest::dispatchJalToReturn(*c, LEAF_WALK_RESET, 0x80053F84u, G);
    tomba.setStatusFlags(3);
    tomba.setTurnCurrent(0);
    tomba.setTurnTarget(0);
    tomba.setTurnSuppressGate(0);
    tomba.setExtraClear(0); // extra clear only on this path (verified vs shard)
    tomba.setOuterState(2);
    tomba.setLoadStep(1);
    tomba.setLoadSub(0);
    c->mem_w8(BUSY_LATCH, 1);
    tomba::guest::dispatchJalToReturn(*c, LEAF_STOPMOTION, 0x80053FC0u, 6u, tomba.posAddr(), (uint32_t)-80);
  }
  // else (busy latch already set): nothing to do.
  return true;
}

// outerTransitionCommit — guest FUN_80053FDC(G, mode). See actor_tomba.h for the full RE writeup
// (incl. the Ghidra-vs-ground-truth correction in the decrement/settle tail below). Guest frame:
// 32 B; s0(r16)/s1(r17)/ra spilled but, like outerTransitionGate, never written by this body —
// pure passthrough (no s2 slot here, a smaller frame than outerTransitionGate's).
void ActorTomba::outerTransitionCommit(int32_t mode) {
  Core *c = core;
  const uint32_t G = G_ADDR;
  static constexpr GuestFrameSpill kSpills[] = {{16, 16}, {17, 20}, {31, 24}};
  GuestFrame<32, 3> frameGuard(c, kSpills);
  TombaState tomba{c, G};

  c->r[31] = 0x80053FF8u;
  if (outerTransitionGate()) {
    return;
  }

  if (tomba.turnCurrent() != tomba.turnTarget()) {
    // "reset to new target" — cue + gStateMutate(0xB) + conditional stop-motion clear.
    tomba::guest::dispatchJalToReturn(*c, LEAF_CUE_800521F4, 0x80054024u, 0u, 0x81u, 0x81u, 0x0Fu);
    eng(c).gStateMutate(G, 0xB);
    c->mem_w8(BUSY_LATCH_HI, 0);
    if ((tomba.statusFlags() & 4u) == 0) {
      tomba::guest::dispatchJalToReturn(*c, LEAF_WALK_RESET, 0x80054054u, G);
      tomba.setStopMotionAux(0);
    }
    if (mode != 1 && tomba.outerState() == 2) {
      return; // already committing — nothing to do
    }
    // Commit a new target (shared by mode==1 and the mode!=1-but-not-yet-committing path).
    tomba.setSettleCounter(0x5A);
    tomba.setTurnTarget((uint16_t)tomba.turnCurrent());
    tomba.setLatchFlags((uint8_t)(tomba.latchFlags() | 0x82u));
    if ((tomba.statusFlags() & 0xCu) != 0) {
      tomba::guest::dispatchJalToReturn(*c, 0x80074590u, 0x80054108u, 0x23u, 0u, 0u);
      tomba::guest::dispatchJalToReturn(*c, LEAF_STOPMOTION, 0x80054118u, 6u, tomba.posAddr(), (uint32_t)-80);
      return;
    }
    tomba.setStatusFlags(3);
    tomba.setTurnSuppressGate(0);
    c->mem_w8(G + 0x145u, 0);
    tomba.setOuterState(2);
    tomba.setLoadStep(0);
    tomba.setLoadSub(0);
    return;
  }

  // Pending counter already at target — decrement-and-settle path.
  const int16_t remaining = tomba.settleCounter();
  if (remaining == 0) {
    return;
  }
  const int16_t newRemaining = (int16_t)(remaining - 1);
  tomba.setSettleCounter((uint16_t)newRemaining);
  if (newRemaining != 0) {
    return;
  }

  const uint8_t g0 = tomba.statusFlags();
  if (g0 != 2 && (g0 & 4u) == 0) {
    // "unobstructed" — commit to walk-state 1, clearing/masking the stop-motion latch bits.
    tomba.setStatusFlags(1);
    if ((tomba.latchFlags() & 0x50u) != 0) {
      tomba.setLatchFlags((uint8_t)(tomba.latchFlags() & 0x7Fu));
    } else {
      tomba.setLatchFlags(0);
    }
  } else {
    // g0==2 OR (g0&4)!=0 — re-arm the settle counter instead of committing.
    tomba.setSettleCounter(1);
  }
}

namespace {
constexpr uint32_t TURN_SUPPRESS_A = 0x800E7E68u; // "turn-suppress mask" pair, also
constexpr uint32_t TURN_SUPPRESS_B = 0x800ECF54u; // written by the enemy-engage tables
constexpr uint32_t TURN_SUPPRESS_CLEAR_GATE = 0x1F800230u;
constexpr uint32_t TURN_SUPPRESS_MASK_SPAD = 0x1F800174u;
constexpr uint32_t CUTSCENE_FLAG = 0x800BF80Fu;
constexpr uint32_t FRAME_PAUSE_FLAG = 0x1F800137u;
constexpr uint32_t CASE4_MASK_SRC_A = 0x1F800166u; // case-4's alt source for TURN_SUPPRESS_A
constexpr uint32_t CASE4_MASK_SRC_B = 0x1F800190u; // case-4's alt source for TURN_SUPPRESS_B
constexpr uint32_t TURN_SUPPRESS_ACTIVE_OUT = 0x1F800232u;
constexpr uint32_t LOAD_KICK_GATE_SPAD = 0x1F80019Bu;
constexpr uint32_t LOAD_KICK_MODE_BYTE = 0x800BF89Cu;
constexpr uint32_t ANIM_PTR_SPAD = 0x1F800138u;
} // namespace

void ActorTomba::frameTick() {
  Core *c = core;
  const uint32_t G = c->r[4]; // a0 (== G_ADDR from both callers; matches gen's r16=r4+r0)
  static constexpr GuestFrameSpill kSpills[] = {{16, 16}, {31, 28}, {18, 24}, {17, 20}};
  GuestFrame<32, 4> frameGuard(c, kSpills);
  GuestReg<16> gReg(c);
  gReg = G;
  TombaState tomba{c, G};

  const uint8_t outerState = tomba.outerState();
  if (outerState < 8) {
    switch (outerState) {
    case 0: {
      tomba::guest::dispatchJalToReturn(*c, 0x80058648u, 0x80059560u, G, 0u); // enterOuterState0
      break;
    }
    case 1: {
      const uint16_t savedA = c->mem_r16(TURN_SUPPRESS_A);
      const uint16_t savedB = c->mem_r16(TURN_SUPPRESS_B);
      GuestReg<18> r18Reg(c);
      r18Reg = savedA;
      GuestReg<17> r17Reg(c);
      r17Reg = savedB;
      if (c->mem_r8(TURN_SUPPRESS_CLEAR_GATE) != 0) {
        const uint16_t mask = c->mem_r16(TURN_SUPPRESS_MASK_SPAD);
        c->mem_w16(TURN_SUPPRESS_B, (uint16_t)(savedB & ~mask));
        c->mem_w16(TURN_SUPPRESS_A, (uint16_t)(savedA & ~mask));
      }
      // GT clears the turn-suppress pair unless BOTH CUTSCENE_FLAG==0 AND FRAME_PAUSE_FLAG==0
      // (the wide-RE draft inverted the CUTSCENE_FLAG condition — fixed).
      const bool skipClear = (c->mem_r8(CUTSCENE_FLAG) == 0) && (c->mem_r8(FRAME_PAUSE_FLAG) == 0);
      if (!skipClear) {
        c->mem_w16(TURN_SUPPRESS_A, 0);
        c->mem_w16(TURN_SUPPRESS_B, 0);
      }

      tomba::guest::dispatchJalToReturn(
          *c, 0x80055C9Cu, 0x800595DCu, G, (uint32_t)(int32_t)tomba.facing()); // turnBiasCompute
      tomba::guest::dispatchJalToReturn(*c, 0x80058918u, 0x800595E4u, G);      // mode-N dispatch table A
      if (tomba.turnSuppressGate() == 0) {
        c->mem_w8(TURN_SUPPRESS_ACTIVE_OUT, 1);
      }
      tomba::guest::dispatchJalToReturn(*c, 0x800597ACu, 0x80059604u, G);     // matrix-compose
      tomba::guest::dispatchJalToReturn(*c, 0x80053FDCu, 0x80059610u, G, 0u); // outerTransitionCommit (mode=0)

      c->mem_w16(TURN_SUPPRESS_A, savedA);
      c->mem_w16(TURN_SUPPRESS_B, savedB);
      break;
    }
    case 2: {
      tomba.setCommitting(1);
      tomba::guest::dispatchJalToReturn(*c, 0x80067CA4u, 0x80059628u, G);
      tomba::guest::dispatchJalToReturn(*c, 0x800597ACu, 0x800596D8u, G);
      break;
    }
    case 3:
      break; // unused jump-table slot (jump target = epilogue) — no-op
    case 4: {
      const uint16_t savedA = c->mem_r16(TURN_SUPPRESS_A);
      const uint16_t savedB = c->mem_r16(TURN_SUPPRESS_B);
      GuestReg<18> r18Reg(c);
      r18Reg = savedA;
      GuestReg<17> r17Reg(c);
      r17Reg = savedB;
      tomba.setLatchFlags((uint8_t)(tomba.latchFlags() & 0x7Fu));
      if (c->mem_r8(FRAME_PAUSE_FLAG) != 0) {
        c->mem_w16(TURN_SUPPRESS_A, c->mem_r16(CASE4_MASK_SRC_A));
        c->mem_w16(TURN_SUPPRESS_B, c->mem_r16(CASE4_MASK_SRC_B));
      } else {
        c->mem_w16(TURN_SUPPRESS_A, 0);
        c->mem_w16(TURN_SUPPRESS_B, 0);
      }
      tomba::guest::dispatchJalToReturn(
          *c, 0x80055C9Cu, 0x8005968Cu, G, (uint32_t)(int32_t)tomba.facing()); // turnBiasCompute
      tomba::guest::dispatchJalToReturn(*c, 0x80058F5Cu, 0x80059694u, G);      // mode-N dispatch table B
      tomba::guest::dispatchJalToReturn(*c, 0x800597ACu, 0x8005969Cu, G);      // matrix-compose
      tomba::guest::dispatchJalToReturn(*c, 0x80053E50u, 0x800596A4u, G);      // outerTransitionGate (bare tick)

      c->mem_w16(TURN_SUPPRESS_A, savedA);
      c->mem_w16(TURN_SUPPRESS_B, savedB);
      break;
    }
    case 5: {
      tomba::guest::dispatchJalToReturn(*c, 0x8018BD30u, 0x800596C0u, G); // scripted leaf (overlay)
      tomba::guest::dispatchJalToReturn(*c, 0x800597ACu, 0x800596D8u, G);
      break;
    }
    case 6: {
      tomba::guest::dispatchJalToReturn(*c, 0x8018BE40u, 0x800596D0u, G); // scripted leaf (overlay)
      tomba::guest::dispatchJalToReturn(*c, 0x800597ACu, 0x800596D8u, G);
      break;
    }
    case 7: {
      const uint8_t sub = tomba.loadStep();
      GuestReg<17> r17Reg(c);
      r17Reg = sub;
      GuestReg<18> r18Reg(c);
      r18Reg = 1u;
      bool advance = false;
      if (sub == 0) {
        tomba::guest::dispatchJalToReturn(*c, 0x8001CF2Cu, 0x80059720u, G); // engine tick cue
        advance = true;
      } else if (sub == 1) {
        advance = tomba::guest::dispatchJalToReturn(*c, 0x80045580u, 0x80059730u, 1u) != 0; // assetReady
      } else if (sub == 2) {
        if (c->mem_r8(LOAD_KICK_GATE_SPAD) != 0) {
          c->mem_w8(LOAD_KICK_MODE_BYTE, 4);
          tomba::guest::dispatchJalToReturn(*c, 0x80042310u, 0x80059768u, G); // resetLoadGate
          tomba.setOuterState(1);
          tomba.setLoadStep(0);
          tomba.setLoadSub(0);
          tomba.setLoadSub2(0);
          const uint32_t anim = c->mem_r32(ANIM_PTR_SPAD);
          c->mem_w16(anim + 0x4Cu, (uint16_t)sub); // sub==2 — matches gen's r17 trace
          c->mem_w16(anim + 0x4Eu, 1);
        }
      }
      if (advance) {
        tomba.setLoadStep((uint8_t)(tomba.loadStep() + 1));
      }
      tomba::guest::dispatchJalToReturn(*c, 0x80076D68u, 0x80059794u, G); // Animation::step
      break;
    }
    }
  }
}

} // namespace tomba::player
