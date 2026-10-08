// game/ai/actor_zoned_attacker_substate.cpp —
// ActorZonedAttacker::defaultSubStateMachine, the zoned attacker's default (non-motion)
// sub-state machine. See docs/engine_re.md.

#include "actor_zoned_attacker.h"

#include "ai/zoned_attacker_abi.h"
#include "core.h"
#include "core/entry/game_ctx.h"
#include "core/overrides/native_override_catalog.h"
#include "game.h"
#include "guest_abi.h"
#include "guest_call.h"
#include "object/actor.h"
#include "spawn.h"

#include <cstdint>

namespace tomba::ai {
using namespace tomba::ai::zoned;

void ActorZonedAttacker::defaultSubStateMachine(Core *c) {
  GuestFrame<40, 5> frame(c, kSpills_80143A00);
  const uint32_t node = c->r[R_A0];
  Actor a(c, node);
  uint32_t uVar6 = 0;
  uint32_t uVar7 = 0;
  int32_t iVar5 = 0;
  uint8_t sharedBVar2 = 0; // holds case9/case10's node[6] across the shared LAB_801448a8 jump

  const uint8_t state5 = c->mem_r8(node + 5);
  switch (state5) {
  // JUMP-TABLE ORDER (table @0x8010A1EC, read out of the running game — NOT the address order the
  // labels appear in):
  //   [0]=0x80143A50 [1]=0x80143A68 [2]=0x80143BC8 [3]=0x80143D84 [4]=0x80143C78 [5]=0x80143C00
  //   [6]=0x80143F24 [7]=0x801440E8 [8]=0x80144504 [9]=0x80144848 [10]=0x8014487C [11]=0x801441A8
  //   [12]=0x80144438 [13]=0x8014436C [14]=0x80144700 [15]=0x801447A4
  // Entry [0] is NOT an alias of entry [1]: it clears the stateEcho bit, re-packs node[4..7] as one
  // word, and only THEN falls into [1]. The rebuild collapsed the two into a single `case 0` — which
  // dropped both writes AND shifted every arm below by one, so node[5]==1 ran entry [2]'s body,
  // ==2 ran [3], ==3 ran [4], ==4 ran [5]. (Arms 6..15 were never shifted.) Symptom: an actor in
  // move-id 1 called 0x801425F0 instead of 0x80142788, taking a different attack arm — node[7] 1 vs
  // 2, cooldown node[0x40] 44 vs 60, heading node[0x38] 0xD374 vs 0xD3F0. Found 2026-07-23 by the
  // beh_* end-state A/B (kanban #10), bisected here with PSXPORT_THUNK_FORCE_GEN.
  case 0:
    a.setStateEcho((uint16_t)(a.stateEcho_u() & 0xfffb)); // sh node[0x62] &= ~4    [0x80143A50]
    c->mem_w32(node + 4, 0x101u);                         // sw 0x101,node[4..7]    [0x80143A60]
    [[fallthrough]];                                      // into entry [1]         [0x80143A68]
  case 1: {
    uint8_t n6 = c->mem_r8(node + 6);
    if (n6 == 0) {
      c->mem_w16(node + 6, 1);
    } else if (n6 != 1) {
      return;
    }

    call1(c, node, kLeaf_80142788);
    {
      const int32_t sVar4 = (int32_t)(int16_t)c->r[R_V0];
      if (sVar4 == 2) {
        uVar7 = 0x501u;
      } else if (sVar4 < 3) {
        if (sVar4 != 1) {
          return;
        }
        const uint8_t bVar2 = (uint8_t)(c->mem_r8(node + 100) - 1);
        c->mem_w8(node + 100, bVar2);
        uVar7 = 0x201u;
        if ((int32_t)((uint32_t)bVar2 << 24) < 1) {
          c->mem_w32(node + 4, 0x301u);
          psx::cpu::dispatchGuestToReturn0(*c, kRngRead, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
          c->mem_w8(node + 100, (uint8_t)(c->r[R_V0] & 3));
          return;
        }
      } else if (sVar4 == 3) {
        uVar7 = 0x401u;
      } else {
        if (sVar4 != 4) {
          return;
        }
        const int32_t nx = a.posX();
        const int32_t tx = c->mem_r16s(kS_1F800160);
        const int32_t nz = a.posZ();
        const int32_t tz = c->mem_r16s(kS_1F800164);
        c->r[R_A0] = (uint32_t)(int32_t)(int16_t)(nx - tx);
        c->r[R_A1] = (uint32_t)(int32_t)(int16_t)(nz - tz);
        psx::cpu::dispatchGuestToReturn0(*c, kDist2D, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
        const int32_t d = (int32_t)(int16_t)c->r[R_V0];
        int32_t zone;
        if ((c->mem_r16(kS_800E7FFE) & 0x8200) == 0 && d < 0x641) {
          if (d < 0x44d) {
            zone = (d < 0x259) ? 2 : 1;
          } else {
            zone = 0;
          }
        } else {
          zone = -1;
        }
        c->r[R_A0] = node;
        c->r[R_A1] = (uint32_t)zone;
        psx::cpu::dispatchGuestToReturn0(*c, kPickAttackByRange, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
        const int32_t r = (int32_t)(int16_t)c->r[R_V0];
        uVar7 = 0x301u;
        if (r != 0) {
          c->mem_w8(node + 5, (uint8_t)r);
          c->mem_w8(node + 6, 0);
          return;
        }
      }
    }
    goto LAB_80144908;
  }
  case 2: { // entry [2]              [0x80143BC8]
    uint8_t n6 = c->mem_r8(node + 6);
    if (n6 == 0) {
      c->mem_w16(node + 6, 1);
    } else if (n6 != 1) {
      return;
    }
    call1(c, node, kLeaf_801425F0);
    uVar6 = (uint32_t)c->r[R_V0] << 16;
    break;
  }
  case 3: { // entry [3]              [0x80143D84]
    uint8_t bVar2 = c->mem_r8(node + 6);
    if (bVar2 != 1) {
      if (1 < bVar2) {
        uVar6 = 0;
        if (bVar2 != 2) {
          return;
        }
        if (c->mem_r8(node + 7) == 0) {
          a.setStateEcho((uint16_t)(a.stateEcho_u() & 0xfffb));
          call2(c, node, kSetAnimStateCue, 0x1au, 8u);
          c->mem_w16(node + 0x4e, 0);
          c->mem_w8(node + 7, 1);
          goto LAB_80143ea0_a;
        } else {
          if (c->mem_r8(node + 7) == 1) {
            goto LAB_80143ea0_a;
          }
          if (c->mem_r8(node + 7) < 0x14) {
            const int8_t cVar3 = (int8_t)(c->mem_r8(node + 7) + 1);
            c->mem_w8(node + 7, (uint8_t)cVar3);
            goto LAB_80143ea0_tail_skip;
          }
          uVar6 = 1;
          goto LAB_80143ea0_tail_skip;
        }
      LAB_80143ea0_a: {
        const uint32_t p38 = c->mem_r32(node + 0x38);
        if (c->mem_r16s(p38 + 4) != 0) {
          a.setStateEcho((uint16_t)(a.stateEcho_u() ^ 1));
          a.setRotY((uint16_t)(a.rotY_u() + 0x800));
          call2(c, node, kSetAnimStateCue, 7u, 0u);
          c->mem_w8(node + 7, 2);
        }
      }
      LAB_80143ea0_tail_skip:
        a.setPosY((uint16_t)(a.posY() + 0x10));
        call1(c, node, kMotionAnimStep);
        break;
      }
      if (bVar2 != 0) {
        return;
      }
      c->mem_w16(node + 6, 1);
    }
    if (c->mem_r8(node + 7) == 0) {
      a.setStateEcho((uint16_t)(a.stateEcho_u() & 0xfffb));
      call2(c, node, kSetAnimStateCue, 5u, 8u);
      c->mem_w8(node + 7, 0x2d);
      c->mem_w16(node + 0x4e, 0);
    }
    {
      const uint8_t bv = c->mem_r8(node + 7);
      if (1 < bv) {
        c->mem_w8(node + 7, (uint8_t)(bv - 1));
      }
      a.setPosY((uint16_t)(a.posY() + 0x10));
      call1(c, node, kMotionAnimStep);
      if (1 < bv) {
        return;
      }
      psx::cpu::dispatchGuestToReturn0(*c, kRngRead, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
      uVar6 = c->r[R_V0];
      uVar7 = 0x101u;
      if ((uVar6 & 0x8000) == 0) {
        goto LAB_801448e8;
      }
      goto LAB_80144908;
    }
  }
  case 4: { // entry [4]              [0x80143C78]
    uint8_t bVar2 = c->mem_r8(node + 6);
    if (bVar2 != 1) {
      if (1 < bVar2) {
        uVar6 = 0;
        if (bVar2 != 2) {
          return;
        }
        if (c->mem_r8(node + 7) == 0) {
          a.setStateEcho((uint16_t)(a.stateEcho_u() & 0xfffb));
          call2(c, node, kSetAnimStateCue, 0x1au, 8u);
          c->mem_w16(node + 0x4e, 0);
          c->mem_w8(node + 7, 1);
          goto LAB_80143d00_a;
        } else {
          if (c->mem_r8(node + 7) == 1) {
            goto LAB_80143d00_a;
          }
          if (c->mem_r8(node + 7) < 0x14) {
            const int8_t cVar3 = (int8_t)(c->mem_r8(node + 7) + 1);
            c->mem_w8(node + 7, (uint8_t)cVar3);
            goto LAB_80143d00_tail_skip;
          }
          uVar6 = 1;
          goto LAB_80143d00_tail_skip;
        }
      LAB_80143d00_a: {
        const uint32_t p38 = c->mem_r32(node + 0x38);
        if (c->mem_r16s(p38 + 4) != 0) {
          a.setStateEcho((uint16_t)(a.stateEcho_u() ^ 1));
          a.setRotY((uint16_t)(a.rotY_u() + 0x800));
          call2(c, node, kSetAnimStateCue, 7u, 0u);
          c->mem_w8(node + 7, 2);
        }
      }
      LAB_80143d00_tail_skip:
        a.setPosY((uint16_t)(a.posY() + 0x10));
        call1(c, node, kMotionAnimStep);
        break;
      }
      if (bVar2 != 0) {
        return;
      }
      c->mem_w16(node + 6, 1);
    }
    goto LAB_80143c48;
  }
  case 5: { // entry [5]              [0x80143C00]
    uint8_t bVar2 = c->mem_r8(node + 6);
    if (bVar2 != 1) {
      if (1 < bVar2) {
        if (bVar2 == 2) {
          call1(c, node, kLeaf_8014213C);
          iVar5 = (int32_t)((uint32_t)c->r[R_V0] << 16);
          goto LAB_80144550;
        }
        if (bVar2 != 3) {
          return;
        }
        call1(c, node, kLeaf_801422B4);
        uVar6 = (uint32_t)c->r[R_V0] << 16;
        break;
      }
      if (bVar2 != 0) {
        return;
      }
      c->mem_w16(node + 6, 1);
    }
    goto LAB_80143c48;
  }
    { // 0x80143C48 — the shared tail entries [4] and [5] both jump to; NOT a jump-table entry
      // (reached only by `goto LAB_80143c48` from the two arms above.)
    LAB_80143c48:
      call1(c, node, kLeaf_801425F0);
      iVar5 = (int32_t)((uint32_t)c->r[R_V0] << 16);
      goto LAB_801448e0;
    }
  case 6: {
    const uint8_t sub6 = c->mem_r8(node + 6);
    switch (sub6) {
    case 0:
    case 1:
      call1(c, node, kLeaf_801408AC);
      return;
    case 2:
      call2(c, node, kLeaf_80141AC4, 0x20u, 0x500u);
      iVar5 = (int32_t)((uint32_t)c->r[R_V0] << 16);
      goto LAB_80144550;
    case 3: {
      c->r[R_A0] = node;
      c->r[R_A1] = 0;
      c->r[R_A2] = 0x1900u;
      psx::cpu::dispatchGuestToReturn0(*c, kLeaf_80141AC4, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
      const int32_t sVar4 = (int32_t)(int16_t)c->r[R_V0];
      if (sVar4 != 0) {
        c->mem_w16(node + 6, 3);
      }
      bool spawn5f;
      const uint8_t n5f = c->mem_r8(node + 0x5f);
      if (n5f == 0) {
        spawn5f = false;
      } else if (n5f == 3) {
        spawn5f = (a.stateEcho_u() & 1) != 0;
      } else {
        spawn5f = (a.stateEcho_u() & 1) == 0;
      }
      if (!spawn5f) {
        return;
      }
      c->mem_w32(node + 4, 0xa01u);
      return;
    }
    case 4: {
      call1(c, node, kLeaf_80141C20);
      const int32_t sVar4 = (int32_t)(int16_t)c->r[R_V0];
      if (sVar4 != 0) {
        c->mem_w16(node + 6, 4);
      }
      bool spawn5f;
      const uint8_t n5f = c->mem_r8(node + 0x5f);
      if (n5f == 0) {
        spawn5f = false;
      } else if (n5f == 3) {
        spawn5f = (a.stateEcho_u() & 1) != 0;
      } else {
        spawn5f = (a.stateEcho_u() & 1) == 0;
      }
      if (!spawn5f) {
        return;
      }
      c->mem_w32(node + 4, 0xa01u);
      return;
    }
    case 5: {
      if (c->mem_r8(node + 7) == 0) {
        a.setStateEcho((uint16_t)(a.stateEcho_u() & 0xfffb));
        call2(c, node, kSetAnimStateCue, 10u, 8u);
        c->mem_w8(node + 7, 0x5a);
        c->mem_w16(node + 0x4e, 0);
      }
      uint8_t bv = c->mem_r8(node + 7);
      if (1 < bv) {
        c->mem_w8(node + 7, (uint8_t)(bv - 1));
      }
      uVar6 = (uint32_t)(1 >= bv);
      a.setPosY((uint16_t)(a.posY() + 0x10));
      call1(c, node, kMotionAnimStep);
      break;
    }
    default:
      return;
    }
    break;
  }
  case 7: {
    const uint8_t bVar2 = c->mem_r8(node + 6);
    if (bVar2 == 2) {
      call1(c, node, kLeaf_8014181C);
      iVar5 = (int32_t)((uint32_t)c->r[R_V0] << 16);
      goto LAB_80144550;
    }
    if (bVar2 < 3) {
      call1(c, node, kLeaf_801408AC);
      return;
    }
    if (bVar2 != 3) {
      return;
    }
    if (c->mem_r8(node + 7) == 0) {
      a.setStateEcho((uint16_t)(a.stateEcho_u() & 0xfffb));
      call2(c, node, kSetAnimStateCue, 5u, 8u);
      c->mem_w8(node + 7, 0x2d);
      c->mem_w16(node + 0x4e, 0);
    }
    uint8_t bv = c->mem_r8(node + 7);
    if (1 < bv) {
      c->mem_w8(node + 7, (uint8_t)(bv - 1));
    }
    uVar6 = (uint32_t)(1 >= bv);
    a.setPosY((uint16_t)(a.posY() + 0x10));
    call1(c, node, kMotionAnimStep);
    break;
  }
  case 8: {
    const uint8_t sub8 = c->mem_r8(node + 6);
    switch (sub8) {
    case 0:
    case 1:
      call1(c, node, kLeaf_801408AC);
      return;
    case 2:
      call1(c, node, kLeaf_8014103C);
      iVar5 = (int32_t)((uint32_t)c->r[R_V0] << 16);
      goto LAB_80144550;
    case 3: {
      call2(c, node, kLeaf_80141AC4, 0x20u, 0x500u);
      const int32_t sVar4 = (int32_t)(int16_t)c->r[R_V0];
      if (sVar4 == 0) {
        return;
      }
      c->mem_w16(node + 6, 4);
      return;
    }
    case 4: {
      call1(c, node, kLeaf_80141438);
      const int32_t sVar4 = (int32_t)(int16_t)c->r[R_V0];
      if (sVar4 != 0) {
        c->mem_w16(node + 6, 5);
      }
      bool spawn5f;
      const uint8_t n5f = c->mem_r8(node + 0x5f);
      if (n5f == 0) {
        spawn5f = false;
      } else if (n5f == 3) {
        spawn5f = (a.stateEcho_u() & 1) != 0;
      } else {
        spawn5f = (a.stateEcho_u() & 1) == 0;
      }
      if (!spawn5f) {
        return;
      }
      c->mem_w32(node + 4, 0xa01u);
      return;
    }
    case 5: {
      if (c->mem_r8(node + 7) == 0) {
        a.setStateEcho((uint16_t)(a.stateEcho_u() & 0xfffb));
        call2(c, node, kSetAnimStateCue, 0x1fu, 8u);
        c->mem_w8(node + 7, 0x32);
        c->mem_w16(node + 0x4e, 0);
      }
      uint8_t bv = c->mem_r8(node + 7);
      if (1 < bv) {
        c->mem_w8(node + 7, (uint8_t)(bv - 1));
      }
      a.setPosY((uint16_t)(a.posY() + 0x10));
      call1(c, node, kMotionAnimStep);
      if (1 < bv) {
        return;
      }
      c->mem_w16(node + 6, 6);
      return;
    }
    case 6: {
      if (c->mem_r8(node + 7) == 0) {
        a.setStateEcho((uint16_t)(a.stateEcho_u() & 0xfffb));
        call2(c, node, kSetAnimStateCue, 5u, 8u);
        c->mem_w8(node + 7, 0x2d);
        c->mem_w16(node + 0x4e, 0);
      }
      uint8_t bv = c->mem_r8(node + 7);
      if (1 < bv) {
        c->mem_w8(node + 7, (uint8_t)(bv - 1));
      }
      uVar6 = (uint32_t)(1 >= bv);
      a.setPosY((uint16_t)(a.posY() + 0x10));
      call1(c, node, kMotionAnimStep);
      break;
    }
    default:
      return;
    }
    break;
  }
  case 9: {
    const uint8_t bVar2 = c->mem_r8(node + 6);
    if (bVar2 != 1) {
      if (1 < bVar2) {
        sharedBVar2 = bVar2;
        goto LAB_801448a8;
      }
      if (bVar2 != 0) {
        return;
      }
      c->mem_w16(node + 6, 1);
    }
    call1(c, node, kLeaf_80140AF4);
    iVar5 = (int32_t)((uint32_t)c->r[R_V0] << 16);
    goto LAB_801448e0;
  }
  case 10: {
    const uint8_t bVar2 = c->mem_r8(node + 6);
    if (bVar2 != 1) {
      if (1 < bVar2) {
        sharedBVar2 = bVar2;
        goto LAB_801448a8;
      }
      if (bVar2 != 0) {
        return;
      }
      c->mem_w16(node + 6, 1);
    }
    call1(c, node, kLeaf_80140AF4);
    {
      const int32_t v0copy = (int32_t)(int16_t)c->r[R_V0];
      if (c->mem_r8(node + 7) < 3) {
        a.setRotZ(0);
      }
      iVar5 = v0copy << 16;
    }
    goto LAB_801448e0;
  }
  case 0xb: {
    const uint8_t sub_b = c->mem_r8(node + 6);
    switch (sub_b) {
    case 0:
    case 1:
      call1(c, node, kLeaf_801408AC);
      return;
    case 2: {
      call2(c, node, kLeaf_80142A94, 0x300u, 0x1e00u);
      const int32_t sVar4 = (int32_t)(int16_t)c->r[R_V0];
      uVar7 = 0xa01u;
      if (sVar4 != -1) {
        if (sVar4 == 0) {
          bool spawn5f;
          const uint8_t n5f = c->mem_r8(node + 0x5f);
          if (n5f == 0) {
            spawn5f = false;
          } else if (n5f == 3) {
            spawn5f = (a.stateEcho_u() & 1) != 0;
          } else {
            spawn5f = (a.stateEcho_u() & 1) == 0;
          }
          if (!spawn5f) {
            return;
          }
          c->mem_w32(node + 4, 0xa01u);
          return;
        }
        c->mem_w16(node + 6, 4);
        return;
      }
      break;
    }
    case 3: {
      call1(c, node, kLeaf_80142CF4);
      const int32_t sVar4 = (int32_t)(int16_t)c->r[R_V0];
      uVar7 = 0xa01u;
      if (sVar4 != -1) {
        if (sVar4 == 0) {
          bool spawn5f;
          const uint8_t n5f = c->mem_r8(node + 0x5f);
          if (n5f == 0) {
            spawn5f = false;
          } else if (n5f == 3) {
            spawn5f = (a.stateEcho_u() & 1) != 0;
          } else {
            spawn5f = (a.stateEcho_u() & 1) == 0;
          }
          if (!spawn5f) {
            return;
          }
          c->mem_w32(node + 4, 0xa01u);
          return;
        }
        c->mem_w16(node + 6, 3);
        return;
      }
      break;
    }
    case 4: {
      if (c->mem_r8(node + 7) == 0) {
        a.setStateEcho((uint16_t)(a.stateEcho_u() & 0xfffb));
        call2(c, node, kSetAnimStateCue, 10u, 8u);
        c->mem_w8(node + 7, 0x3c);
        c->mem_w16(node + 0x4e, 0);
      }
      uint8_t bv = c->mem_r8(node + 7);
      if (1 < bv) {
        c->mem_w8(node + 7, (uint8_t)(bv - 1));
      }
      uVar6 = (uint32_t)(1 >= bv);
      a.setPosY((uint16_t)(a.posY() + 0x10));
      call1(c, node, kMotionAnimStep);
      goto LAB_801448fc;
    }
    default:
      return; // switchD_80143a48_caseD_10 (final return)
    }
    goto LAB_80144908;
  }
  case 0xc: {
    const uint8_t bVar2 = c->mem_r8(node + 6);
    if (bVar2 == 2) {
      const int32_t nx = a.posX();
      const int32_t tx = c->mem_r16s(kS_1F800160);
      const int32_t nz = a.posZ();
      const int32_t tz = c->mem_r16s(kS_1F800164);
      c->r[R_A0] = (uint32_t)(int32_t)(int16_t)(nx - tx);
      c->r[R_A1] = (uint32_t)(int32_t)(int16_t)(nz - tz);
      psx::cpu::dispatchGuestToReturn0(*c, kDist2D, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
      // `slti v0,v0,800` [0x80144488] — the compare is on the FULL 32-bit v0, with NO
      // `sll 16 / sra 16` first (unlike the range ladders elsewhere in this cluster, which do
      // sign-extend). The rebuild sign-extended the low half, so a distance in 0x8000..0xFFFF read
      // as negative and took the near branch the guest does not take.
      // And the near branch jumps to 0x80144558 — the STORE — not to 0x80144550, which first tests
      // the incoming v0. Jumping to 0x80144550 added a guest-absent `v0 == 0` early-out.
      if ((int32_t)c->r[R_V0] < 800) { // bnez -> 0x80144558 [0x8014448C]
        c->mem_w16(node + 6, 3);       // sh 3,6(s0)        [0x80144558]
        return;
      }
      call2(c, node, kLeaf_80142A94, 800u, 0x1e00u);
      const int32_t sVar4 = (int32_t)(int16_t)c->r[R_V0];
      uVar7 = 0xa01u;
      if (sVar4 != -1) {
        if (sVar4 == 0) {
          return;
        }
        c->mem_w16(node + 6, 3);
        return;
      }
      goto LAB_80144908;
    }
    if (bVar2 < 3) {
      call1(c, node, kLeaf_801408AC);
      return;
    }
    if (bVar2 != 3) {
      return;
    }
    call1(c, node, kLeaf_801436C4);
    {
      const int32_t sVar4 = (int32_t)(int16_t)c->r[R_V0];
      uVar7 = 0xa01u;
      if (sVar4 == -1) {
        goto LAB_80144908;
      }
      if (sVar4 == 0) {
        return;
      }
      if (c->mem_r8(kS_800E7E80) & 2) {
        c->mem_w32(node + 4, 0xf01u);
        return;
      }
      goto LAB_80144904;
    }
  }
  case 0xd: {
    const uint8_t bVar2 = c->mem_r8(node + 6);
    if (bVar2 == 2) {
      call1(c, node, kLeaf_801431C4);
      const int32_t sVar4 = (int32_t)(int16_t)c->r[R_V0];
      uVar7 = 0xa01u;
      if (sVar4 != -1) {
        if (sVar4 == 0) {
          return;
        }
        c->mem_w16(node + 6, 3);
        return;
      }
      goto LAB_80144908;
    }
    if (bVar2 < 3) {
      call1(c, node, kLeaf_801408AC);
      return;
    }
    if (bVar2 != 3) {
      return;
    }
    if (c->mem_r8(node + 7) == 0) {
      a.setStateEcho((uint16_t)(a.stateEcho_u() & 0xfffb));
      call2(c, node, kSetAnimStateCue, 5u, 8u);
      c->mem_w8(node + 7, 0x2d);
      c->mem_w16(node + 0x4e, 0);
    }
    uint8_t bv = c->mem_r8(node + 7);
    if (1 < bv) {
      c->mem_w8(node + 7, (uint8_t)(bv - 1));
    }
    uVar6 = (uint32_t)(1 >= bv);
    a.setPosY((uint16_t)(a.posY() + 0x10));
    call1(c, node, kMotionAnimStep);
    break;
  }
  case 0xe: {
    uint8_t n6 = c->mem_r8(node + 6);
    if (n6 == 0) {
      c->mem_w16(node + 6, 1);
    } else if (n6 != 1) {
      return;
    }
    if (c->mem_r8(node + 7) == 0) {
      a.setStateEcho((uint16_t)(a.stateEcho_u() & 0xfffb));
      call2(c, node, kSetAnimStateCue, 0x2fu, 8u);
      c->mem_w8(node + 7, 0x1e);
      c->mem_w16(node + 0x4e, 0);
    }
    uint8_t bv = c->mem_r8(node + 7);
    if (1 < bv) {
      c->mem_w8(node + 7, (uint8_t)(bv - 1));
    }
    uVar6 = (uint32_t)(1 >= bv);
    a.setPosY((uint16_t)(a.posY() + 0x10));
    call1(c, node, kMotionAnimStep);
    break;
  }
  case 0xf: {
    uint8_t n6 = c->mem_r8(node + 6);
    if (n6 == 0) {
      c->mem_w16(node + 6, 1);
    } else if (n6 != 1) {
      return;
    }
    if (c->mem_r8(node + 7) == 0) {
      a.setStateEcho((uint16_t)(a.stateEcho_u() & 0xfffb));
      call2(c, node, kSetAnimStateCue, 0x30u, 8u);
      c->mem_w8(node + 7, 0x1e);
      c->mem_w16(node + 0x4e, 0);
    }
    uint8_t bv = c->mem_r8(node + 7);
    if (1 < bv) {
      c->mem_w8(node + 7, (uint8_t)(bv - 1));
    }
    uVar6 = (uint32_t)(1 >= bv);
    a.setPosY((uint16_t)(a.posY() + 0x10));
    call1(c, node, kMotionAnimStep);
    break;
  }
  default:
    return; // switchD_80143a48_caseD_10
  }
  goto LAB_801448fc;

LAB_801448a8:
  if (sharedBVar2 != 2) {
    return;
  }
  call1(c, node, kLeaf_8014243C);
  uVar6 = (uint32_t)c->r[R_V0] << 16;

LAB_801448fc:
  if (uVar6 != 0) {
  LAB_80144904:
    uVar7 = 0x101u;
  LAB_80144908:
    c->mem_w32(node + 4, uVar7);
  }
  return; // switchD_80143a48_caseD_10

LAB_80144550:
  if (iVar5 == 0) {
    return;
  }
  // LAB_80144558
  c->mem_w16(node + 6, 3);
  return;

LAB_801448e0:
  if (iVar5 == 0) {
    return;
  }
LAB_801448e8:
  c->mem_w16(node + 6, 2);
  return;
}

} // namespace tomba::ai
