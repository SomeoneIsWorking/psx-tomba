// game/player/actor_interaction.cpp — what happens when Tomba touches something.
//
// ActorTomba's two per-frame walks decide WHICH items are in range; this class decides what each one
// MEANS, one method per guest item kind. The bodies are faithful transcriptions of the authenticated
// executable/overlay code (ground truth, matching Ghidra 1:1); see docs/engine_re.md and
// game/player/actor_tomba.h for the per-address writeup.
#include "player/actor_interaction.h"

#include "core.h"
#include "core/entry/game_ctx.h"
#include "core/overrides/guest_jal.h"
#include "guest_abi.h"
#include "guest_call.h"
#include "player/actor_tomba.h"
#include "player/tomba_state.h"
#include "trig.h"

#include <cstdint>

namespace tomba::player {

namespace {
inline constexpr uint32_t kLeafProximityStep =
    0x8001F40Cu; // FUN_8001F40C — shared proximity+step (== the shared proximity+step leaf)
inline constexpr uint32_t kLeafAltTagSet = 0x8001FDB4u; // FUN_8001FDB4 — alt-tag stamp (== the alt-tag stamp)
inline constexpr uint32_t kLeafGrownPush =
    0x8001F054u; // FUN_8001F054 — grown-state push (stepModeInteract's 0x8000-set/mode&3 branch)
inline constexpr uint32_t kLeafNiladicCue =
    0x8001F830u; // FUN_8001F830 — niladic cue (type8Interact's item[0]==5 branch)
inline constexpr uint32_t kLeafGrownDelegate =
    0x8001EC3Cu; // FUN_8001EC3C — whole-hog grown-state delegate (type8Interact's 0x8000-set branch)
inline constexpr uint32_t kLeafStepModeFlag = 0x8001FF7Cu; // FUN_8001FF7C — type7Interact's mode/flag call
} // namespace

// FUN_80022060 — cylinder proximity + Y-band check.
//
// BOTH gates compare an UNSIGNED 16-bit quantity (`andi …,0xffff` at 0x800220E0 and 0x80022118)
// against a sign-extended 32-bit limit. That is what makes the vertical gate one-sided in the way
// the game depends on: the vertical term is Tomba-above-item PLUS both up-extents, so once Tomba
// clears the item (jumping over it) the sum goes NEGATIVE, `andi 0xffff` turns it into ~0xFFxx =
// a huge positive, and the `slt limit, band` rejects the touch. Sign-extending it instead makes it
// a small negative that sails under the limit — i.e. every jump-over collects the item (kanban #1
// / #30). The distance gate is the same shape: isqrt can return > 0x7FFF, and sign-extending that
// turns a very distant object into a negative "distance" that passes. Keep both zero-extended.
// The scratchpad output at +0x8C is the one place the guest DOES sign-extend it (sll/sra at
// 0x80022134), so that store keeps its own sign-extended value.
void ActorInteraction::proximityCheck(uint32_t item) {
  Core *c = tomba_.core;
  const uint32_t G = ActorTomba::G_ADDR;
  if (c->mem_r8(0x1F80027Au) != 0) {
    return;
  }

  TombaState tomba{c, G};
  TombaState other{c, item};

  const int32_t dx = (int32_t)(int16_t)(tomba.posX() - other.posX());
  const int32_t dz = (int32_t)(int16_t)(tomba.posZ() - other.posZ());
  c->r[4] = (uint32_t)(dx * dx + dz * dz);
  psx::cpu::dispatchGuestToReturn0(*c, kLeafIsqrt, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
  const uint16_t distBits = (uint16_t)c->r[2];
  const int32_t dist = (int32_t)(uint32_t)distBits; // `andi a0,a2,0xffff` — UNSIGNED

  const int32_t rxz = (int32_t)tomba.boundXZ() + (int32_t)other.boundXZ();
  if (dist > rxz) {
    return;
  }

  const int32_t vbandRaw = (int32_t)(uint32_t)(uint16_t)((tomba.posY() - other.posY()) + tomba.boundYUp() +
                                                         other.boundYUp()); // `andi v1,v1,0xffff`
  const int32_t vbandLim = (int32_t)tomba.boundYDown() + (int32_t)other.boundYDown();
  if (vbandRaw > vbandLim) {
    return;
  }

  c->mem_w32(kOutDistanceSlot, (uint32_t)(int32_t)(int16_t)distBits); // sll/sra 16 — sign-extended
  c->r[4] = (uint32_t)(-dz);
  c->r[5] = (uint32_t)dx;
  psx::cpu::dispatchGuestToReturn0(*c, kLeafAtan2, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
  c->mem_w32(kOutHeadingSlot, c->r[2]);
  markItemConsumed(c, item);
  c->mem_w8(0x800BF81Eu, 0);
}

// FUN_80114E74 — type-4 guarded proximity.
void ActorInteraction::type4GuardedCheck(uint32_t item) {
  Core *c = tomba_.core;
  const uint32_t G = ActorTomba::G_ADDR;
  if (c->mem_r8(G + 0x164u) == 5 && c->mem_r8(G + 0x147u) == c->mem_r8(item + 0x47u)) {
    return;
  }
  c->r[4] = G;
  c->r[5] = item;
  psx::cpu::dispatchGuestToReturn0(*c, kLeafProximityF04, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
  if (c->r[2] == 0) {
    return;
  }
  markItemConsumed(c, item);
}

// FUN_80022190 — per-sub-hitbox collision variant.
void ActorInteraction::subHitboxCheck(uint32_t item) {
  Core *c = tomba_.core;
  const uint32_t G = ActorTomba::G_ADDR;
  TombaState tomba{c, G};
  const int16_t hitboxCount = (int16_t)c->mem_r16(item + 0x6Au);
  if (hitboxCount <= 0) {
    return;
  }
  uint32_t hitboxArr = c->mem_r32(item + 0x6Cu);

  for (int32_t i = 0; i < hitboxCount; i++, hitboxArr += 0x10u) {
    const uint32_t mask = 1u << (i & 0x1F);
    if ((c->mem_r32(item + 0x70u) & mask) == 0) {
      continue;
    }

    const uint32_t typeParam = (uint32_t)c->mem_r8(hitboxArr + 3u) * 8u;
    const int32_t dx = (int32_t)(int16_t)(tomba.posX() - c->mem_r16(hitboxArr + 4u));
    const int32_t dz = (int32_t)(int16_t)(tomba.posZ() - c->mem_r16(hitboxArr + 8u));
    c->r[4] = (uint32_t)(dx * dx + dz * dz);
    psx::cpu::dispatchGuestToReturn0(*c, kLeafIsqrt, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
    const int32_t dist = (int32_t)(uint32_t)(c->r[2] & 0xFFFFu);

    const int32_t rxz = (int32_t)tomba.boundXZ() + (int32_t)c->mem_r8(kSubHitboxParams + typeParam + 0u);
    if (dist > rxz) {
      continue;
    }

    const uint32_t vbandRaw = (uint32_t)((tomba.posY() - c->mem_r16(hitboxArr + 6u)) + tomba.boundYUp() +
                                         c->mem_r8(kSubHitboxParams + typeParam + 1u));
    const int32_t vbandLim = (int32_t)tomba.boundYDown() + (int32_t)c->mem_r8(kSubHitboxParams + typeParam + 1u) * 2;
    if ((int32_t)(uint16_t)vbandRaw > vbandLim) {
      continue;
    }

    c->mem_w32(item + 0x74u, c->mem_r32(item + 0x74u) | mask);
    c->mem_w32(item + 0x70u, c->mem_r32(item + 0x70u) & ~mask);
    c->r[4] = item;
    c->r[5] = hitboxArr;
    c->r[6] = 0;
    psx::cpu::dispatchGuestToReturn0(*c, kLeafCollisionCallback, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
    return;
  }
}

// FUN_80020364 — postInteractWalk case 0xF/0x14/0x56 (mode=0) / 0x2F (mode=2).
uint8_t ActorInteraction::stepModeInteract(uint32_t item, uint32_t mode) {
  Core *c = tomba_.core;
  const uint32_t G = ActorTomba::G_ADDR;

  // Guest frame: addiu sp,-40; spill s0,s1,s2,s3,ra (mirrored for completeness though this
  // draft has no re-entrant native call that would observe the guest stack bytes yet).
  const uint32_t sp0 = c->r[29];
  c->r[29] = sp0 - 40;
  c->mem_w32(c->r[29] + 20, c->r[17]);
  c->mem_w32(c->r[29] + 24, c->r[18]);
  c->mem_w32(c->r[29] + 32, c->r[31]);
  c->mem_w32(c->r[29] + 28, c->r[19]);
  c->mem_w32(c->r[29] + 16, c->r[16]);
  c->r[17] = G;
  c->r[18] = item;
  c->r[19] = mode;

  uint8_t result;
  if (c->mem_r16(G + 0x17Eu) & 0x200u) {
    result = 0; // paused — no interaction
  } else {
    c->r[4] = G;
    c->r[5] = item;
    c->r[6] = 1;
    c->r[31] = 0x800203A8u;
    psx::cpu::dispatchGuestToReturn0(*c, kLeafProximityStep, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
    const int32_t v0 = (int32_t)c->r[2];
    if (v0 < 0) {
      result = 0; // no hit
    } else if (c->mem_r8(G + 0x144u) == 1 && v0 < 2) {
      // Just-transitioned state.
      if ((c->mem_r16(G + 0x17Eu) & 0x8000u) == 0) {
        c->r[4] = item;
        c->r[5] = 1;
        c->r[6] = 0x10;
        c->r[7] = 0x20;
        c->r[31] = 0x80020418u;
        psx::cpu::dispatchGuestToReturn0(*c, kLeafAltTagSet, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
        result = 1;
      } else if (mode & 3u) {
        c->r[4] = G;
        c->r[5] = item;
        c->r[31] = 0x800203FCu;
        psx::cpu::dispatchGuestToReturn0(*c, kLeafGrownPush, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
        result = 1;
      } else {
        result = 1;
      }
    } else {
      // Steady-state: optional trig-offset separation, then a mode-bit-keyed result/state stamp.
      // Heading is the FULL 32-bit word proximityCheck stamped into kOutHeadingSlot (a raw
      // Trig::ratan2 result register width, not a 16-bit angle) — read as mem_r32 throughout.
      if (mode & 0x3Fu) {
        const int32_t heading = (int32_t)c->mem_r32(0x1F80009Cu); // kOutHeadingSlot
        const int32_t cosv = trigOf(c).rcos(heading);
        const int32_t sinv = trigOf(c).rsin(heading);
        const int32_t sum80 = (int32_t)c->mem_r16s(G + 0x80u) + (int32_t)c->mem_r16s(item + 0x80u);
        const int16_t dx = (int16_t)((cosv * sum80) >> 12);
        const int16_t dz = (int16_t)((sinv * sum80) >> 12);
        if ((mode & 0x7Fu) == 1) {
          c->mem_w16(item + 0x2Eu, (uint16_t)((int16_t)c->mem_r16(G + 0x2Eu) - dx));
          c->mem_w16(item + 0x36u, (uint16_t)((int16_t)c->mem_r16(G + 0x36u) + dz));
        } else if ((c->mem_r8(G) & 4u) == 0) {
          c->mem_w16(G + 0x2Eu, (uint16_t)((int16_t)c->mem_r16(item + 0x2Eu) + dx));
          c->mem_w16(G + 0x36u, (uint16_t)((int16_t)c->mem_r16(item + 0x36u) - dz));
        }
      }
      // bVar6 (gen: `(byte)(_DAT_1f80009c >> 4)`) — truncate the 32-bit heading word, not a byte load.
      const uint8_t bVar6 = (uint8_t)((uint32_t)c->mem_r32(0x1F80009Cu) >> 4);
      if ((mode & 0x40u) == 0) {
        // mode&0x80 ladder — gate 0x1F80027A (proximityCheck's own "already consumed" guard).
        if (mode & 0x80u) {
          if (c->mem_r8(0x1F80027Au) != 0) {
            result = 2;
            goto done;
          }
          if (c->mem_r8(G + 4u) != 1) {
            result = 2;
            goto done;
          }
          if (c->mem_r8(G + 5u) != 0x13) {
            c->mem_w8(G + 5u, 0x13);
            c->mem_w8(G + 6u, 0);
            c->mem_w8(G + 7u, 0);
            c->mem_w8(G + 0x2Bu, bVar6);
            result = 3;
            goto done;
          }
        }
        result = 2;
      } else {
        uint8_t bVar3 = c->mem_r8(0x1F800137u); // PAUSE_FLAG_SPAD
        if (bVar3 == 0) {
          bVar3 = c->mem_r8(G) & 6u;
          if (bVar3 == 0) {
            bVar3 = c->mem_r8(item) & 2u;
            if (bVar3 == 0) {
              bVar3 = 4;
              c->mem_w8(G + 4u, 2);
              c->mem_w8(G + 5u, 2);
              c->mem_w8(G, 3);
              c->mem_w8(G + 6u, 0);
              c->mem_w16(G + 0x172u, 0x78u); // single u16 store covers both G+0x172(=0x78)/G+0x173(=0)
              c->mem_w8(G + 0x2Bu, bVar6);
            }
          }
        }
        result = bVar3;
      }
    }
  }
done:
  c->r[31] = c->mem_r32(c->r[29] + 32);
  c->r[19] = c->mem_r32(c->r[29] + 28);
  c->r[18] = c->mem_r32(c->r[29] + 24);
  c->r[17] = c->mem_r32(c->r[29] + 20);
  c->r[16] = c->mem_r32(c->r[29] + 16);
  c->r[29] = sp0;
  return result;
}

// FUN_800205CC — postInteractWalk case 8.
void ActorInteraction::type8Interact(uint32_t item) {
  Core *c = tomba_.core;
  const uint32_t G = ActorTomba::G_ADDR;
  // Guest frame per abi_extract --contract 0x800205CC: single epilogue label -> RAII is safe.
  static constexpr GuestFrameSpill kSpills[] = {{17, 20}, {18, 24}, {31, 28}, {16, 16}};
  GuestFrame<32, 4> frameGuard(c, kSpills);
  TombaState tomba{c, G};
  TombaState other{c, item};

  if (c->mem_r8(item) == 5) {
    if ((tomba.growthFlags() & 0x200u) == 0 && tomba.frozenFlag() == 0) {
      tomba::guest::dispatchJalToReturn(*c, kLeafNiladicCue, 0x80020620u);
    }
  } else if (tomba.growthFlags() & 0x8000u) {
    tomba::guest::dispatchJalToReturn(*c, kLeafGrownDelegate, 0x80020644u, G, item);
  } else {
    const int32_t v0 = (int32_t)tomba::guest::dispatchJalToReturn(*c, kLeafProximityStep, 0x80020658u, G, item, 0u);
    if (v0 >= 0) {
      if (c->mem_r8(item) == 1) {
        if (tomba.justTransitioned() == 1 && v0 < 2) {
          tomba::guest::dispatchJalToReturn(*c, kLeafAltTagSet, 0x8002069Cu, item, (uint32_t)-32766, 3u, 30u);
        } else if ((tomba.growthFlags() & 0x200u) == 0) {
          if ((v0 & 1) == 0) {
            if ((tomba.statusFlags() & 4u) == 0) {
              const int32_t heading = (int32_t)c->mem_r32(0x1F80009Cu); // full 32-bit word
              const int32_t cosv = trigOf(c).rcos(heading);
              const int32_t sinv = trigOf(c).rsin(heading);
              const int32_t sum80 = (int32_t)tomba.boundXZ() + (int32_t)other.boundXZ();
              tomba.setPosX((int16_t)(other.posX() + (int16_t)((cosv * sum80) >> 12)));
              tomba.setPosZ((int16_t)(other.posZ() - (int16_t)((sinv * sum80) >> 12)));
            }
            tomba.setGroundContactFlag(1);
            // Heading arg is the full 32-bit kOutHeadingSlot word (Ghidra: `iVar7 = (int)_DAT_1f80009c`
            // — a straight int cast, no 16-bit truncation, matching stepModeInteract's bVar6 fix).
            const int32_t cmp = Trig::angleCmp((int32_t)c->mem_r32(0x1F80009Cu), (int32_t)tomba.facing(), 1);
            tomba.setFlag95((uint8_t)(cmp + 2));
          } else if (v0 == 1 && (tomba.groundedGate() & 1u) == 0) {
            // G+0x32 = item[0x32] - (G[0x84] + item[0x84]) (all u16, unsigned per gen), THEN
            // tomba_.growthYSnap()'s own reset+gated-Y-resnap tail — this branch's G+0x29/0x145/0x4A/
            // 0x50/0x148 reset (v0==1 here) plus the G+0x78/DAT_800BF816-gated const-140/70 snap
            // on G+0x32 are BYTE-IDENTICAL to guest FUN_80022C78 (growthYSnap), reused rather than
            // duplicated (authenticated executable/overlay evidence lines 1-19 == authenticated executable/overlay
            // evidence lines 1-16).
            tomba.setPosY((int16_t)(other.posY() - (tomba.boundYUp() + other.boundYUp())));
            tomba_.growthYSnap();
          }
        }
      } else if ((tomba.growthFlags() & 0x200u) == 0 && (tomba.groundedGate() & 1u) == 0) {
        c->mem_w8(item + 0x29u, 1);
      }
    }
  }
}

// FUN_800235A0 — postInteractWalk case 7.
uint8_t ActorInteraction::type7Interact(uint32_t item) {
  Core *c = tomba_.core;
  const uint32_t G = ActorTomba::G_ADDR;
  // Guest frame per abi_extract --contract 0x800235A0: single epilogue label -> RAII is safe.
  static constexpr GuestFrameSpill kSpills[] = {{16, 16}, {17, 20}, {31, 24}};
  GuestFrame<32, 3> frameGuard(c, kSpills);
  TombaState tomba{c, G};

  const int32_t v0 = (int32_t)tomba::guest::dispatchJalToReturn(*c, kLeafProximityStep, 0x800235C0u, G, item, 1u);
  uint8_t result = 0;
  if (v0 >= 0) {
    const uint32_t flag = (tomba.transitionSlot() == 0x0Cu) ? 4u : 1u;
    c->r[4] = G;
    c->r[5] = item;
    c->r[7] = flag;
    tomba::guest::dispatchJalToReturn(*c, kLeafStepModeFlag, 0x80023600u);
    result = 1;
  }
  return result;
}

} // namespace tomba::player
