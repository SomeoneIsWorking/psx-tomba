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

void ActorZonedAttacker::idleTick(Core *c) {
  GuestFrame<48, 4> frame(c, kSpills_80144B50);
  const uint32_t node = c->r[R_A0];
  Actor a(c, node);
  const uint8_t state5 = c->mem_r8(node + 5);

  switch (state5) {
  case 0:
  case 6:
    goto switchD_caseD_0;
  case 1:
    if (c->mem_r8(node + 6) == 0) {
      c->r[R_A0] = 4;
      psx::cpu::dispatchGuestToReturn0(*c, kLeaf_80026100, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
      call2(c, node, kSetAnimStateCue, 2u, 4u);
      c->mem_w16(node + 0x84, 0x14);
      c->mem_w16(node + 0x86, 100);
      c->r[R_A0] = 0x89u;
      c->r[R_A1] = 0;
      c->r[R_A2] = 0;
      psx::cpu::dispatchGuestToReturn0(*c, kSfxTrigger, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
      c->mem_w8(node + 0x1b, (uint8_t)(c->mem_r8(node + 0x1b) & 0xbf));
      c->mem_w8(node + 0xd, (uint8_t)(c->mem_r8(node + 0xd) & 0xfd));
      c->mem_w8(node + 6, 1);
    } else if (c->mem_r8(node + 6) != 1) {
      goto switchD_caseD_3;
    }
    goto LAB_801451a0;
  case 2:
    break;
  default:
    goto switchD_caseD_3;
  case 4:
    if ((c->mem_r32(node + 4) & 0xffff00u) == 0x400u) {
      c->r[R_A0] = node;
      c->r[R_A1] = 0x20u;
      c->r[R_A2] = 0x30u;
      c->r[R_A3] = 0xffu;
      psx::cpu::dispatchGuestToReturn0(*c, kPaletteSideEffect, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
      goto LAB_80144bd0;
    }
    goto LAB_80144bd0;
  case 5:
  LAB_80144bd0:
    if ((c->mem_r32(node + 4) & 0xffff00u) == 0x500u) {
      c->r[R_A0] = node;
      c->r[R_A1] = 0xffu;
      c->r[R_A2] = 0x30u;
      c->r[R_A3] = 0x30u;
      psx::cpu::dispatchGuestToReturn0(*c, kPaletteSideEffect, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
    }
  switchD_caseD_0: {
    bool bVar1;
    if (a.posY() < 1) {
      if ((int8_t)c->mem_r8(node + 0x66) == (int8_t)0x81) {
        bVar1 = a.posY() < -0x81;
      } else {
        bVar1 = c->mem_r8(kGateTimer) < 0xc;
      }
      bVar1 = !bVar1;
    } else {
      bVar1 = true;
    }
    if (bVar1) {
      c->mem_w8(node + 4, 3);
      goto switchD_caseD_3;
    }
    if ((c->mem_r8(kS_1F800137) == 0 || c->mem_r8(kS_800BF89C) == 2) && c->mem_r8(kS_800BF809) == 0) {
      uint8_t bVar6 = c->mem_r8(node + 6);
      if (bVar6 == 1) {
        goto LAB_80144d20;
      } else if (bVar6 < 2) {
        if (bVar6 == 0) {
          const uint32_t lhs = (((uint32_t)a.interactState() * 0x10 - 0x800) & 0xfffu);
          const uint32_t val = (uint32_t)((int32_t)lhs - a.triggerParam() + 0x400) & 0xfffu;
          uint16_t v62 = a.stateEcho_u();
          v62 = (val < 0x801u) ? (uint16_t)(v62 | 1) : (uint16_t)(v62 & 0xfffe);
          a.setStateEcho(v62);
          c->r[R_A0] = 0x88u;
          c->r[R_A1] = 0;
          c->r[R_A2] = 0;
          psx::cpu::dispatchGuestToReturn0(*c, kSfxTrigger, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
          c->mem_w16(node + 6, 1);
          goto LAB_80144d20;
        }
      } else if (bVar6 == 2) {
        a.setPosY((uint16_t)(a.posY() + 0x10));
        call1(c, node, kMotionAnimStep);
        uint16_t v42 = c->mem_r16(node + 0x42);
        c->mem_w16(node + 0x42, (uint16_t)(v42 - 1));
        if ((int32_t)((int16_t)v42) < 1) {
          c->mem_w16(node + 6, 3);
        }
      } else if (bVar6 == 3) {
        if (c->mem_r8(node + 0xd) & 2) {
          auto lerp = [&](uint32_t off) {
            const uint8_t v = c->mem_r8(node + off);
            const int32_t diff = (int32_t)(0x80 - (uint32_t)v);
            const int8_t inc = (int8_t)(diff >> 3);
            c->mem_w8(node + off, (uint8_t)(v + (uint8_t)inc));
          };
          lerp(0x18);
          lerp(0x19);
          lerp(0x1a);
        }
        call1(c, node, kLeaf_8014243C);
        const int32_t sVar4 = (int32_t)(int16_t)c->r[R_V0];
        call1(c, node, kAnimationStep);
        if (sVar4 != 0) {
          c->mem_w8(node + 0, 1);
          c->mem_w8(node + 0xd, (uint8_t)(c->mem_r8(node + 0xd) & 0xfd));
          c->mem_w8(node + 0x1b, (uint8_t)(c->mem_r8(node + 0x1b) & 0xbf));
          a.setInteractState(0);
          c->mem_w8(node + 3, 0);
          c->mem_w32(node + 4, 1u);
        }
      }
      goto idle_after_substate;
    LAB_80144d20:
      call1(c, node, kLeaf_80140AF4);
      {
        const int32_t sVar4 = (int32_t)(int16_t)c->r[R_V0];
        if (c->mem_r8(node + 5) == 4) {
          if (sVar4 != 0) {
            c->mem_w16(node + 6, 2);
            c->mem_w16(node + 0x42, 0x5a);
          }
        } else {
          call1(c, node, kAnimationStep);
          if (sVar4 != 0) {
            c->mem_w16(node + 6, 3);
          }
        }
      }
      {
        const uint8_t cd = c->mem_r8(kCountdown);
        c->mem_w8(kCountdown, (uint8_t)(cd - 1));
        if ((int8_t)cd < 0) {
          const uint32_t kind = (uint32_t)(c->mem_r16(node + 0x68) >> 8) & 0xf;
          if (kind == 1 || (kind == 2 && c->mem_r16s(node + 0x4e) > 0x500)) {
            const uint32_t gsp = c->r[29];
            c->mem_w16(gsp + 0x12, (uint16_t)a.posX_u());
            c->mem_w16(gsp + 0x16, (uint16_t)c->mem_r16(node + 0x6a));
            c->mem_w16(gsp + 0x1a, (uint16_t)a.posZ_u());
            eng(c).spawn.spawnAndInit(8u, gsp + 0x10, (uint32_t)(int32_t)-0x50);
            c->mem_w8(kCountdown, 10);
          }
          c->mem_w16(node + 0x68, 0);
        }
      }
    }
  idle_after_substate:
    if (c->mem_r8(node + 0x2a) == 1 && a.posX() > 0x31a8) {
      a.setPosX(0x31a8);
    }
    goto switchD_caseD_3;
  }
  case 7:
    goto switchD_caseD_7;
  case 8:
    goto switchD_caseD_8;
  case 10: {
    call1(c, node, kLeaf_801280E8);
    const int32_t sVar4 = (int32_t)(int16_t)c->r[R_V0];
    if (sVar4 != 0) {
      goto LAB_801451b0;
    }
    c->mem_w8(node + 5, 2);
    goto switchD_caseD_8;
  }
  case 0xb: {
    c->r[R_A0] = node;
    c->r[R_A1] = c->mem_r32(node + 0xc0);
    c->r[R_A2] = 1u;
    psx::cpu::dispatchGuestToReturn0(*c, kLeaf_80080750, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
    const int32_t iVar5 = (int32_t)c->r[R_V0];
    if (iVar5 != 0) {
      goto LAB_801451b0;
    }
    c->mem_w8(node + 5, 2);
    break;
  }
  }

switchD_caseD_2:
  if (c->mem_r8(node + 6) == 0) {
    call2(c, node, kSetAnimStateCue, 2u, 4u);
    c->r[R_A0] = (uint32_t)a.interactState() << 4;
    c->r[R_A1] = (uint32_t)a.triggerParam();
    c->r[R_A2] = 0;
    psx::cpu::dispatchGuestToReturn0(*c, kLeaf_80077768, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
    uint16_t v62 = a.stateEcho_u();
    v62 = (c->r[R_V0] == 0) ? (uint16_t)(v62 | 1) : (uint16_t)(v62 & 0xfffe);
    a.setStateEcho(v62);
    c->mem_w16(node + 0x4e, 0xe000);
    a.setAccelY(0xffd8);
    c->mem_w8(node + 0x29, 0);
    c->mem_w8(node + 6, 1);
    c->r[R_A0] = 0x88u;
    c->r[R_A1] = 0;
    c->r[R_A2] = 0;
    psx::cpu::dispatchGuestToReturn0(*c, kSfxTrigger, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
  } else {
    if (c->mem_r8(node + 6) != 1) {
      c->mem_w8(node + 4, 3);
      return;
    }
    int32_t iVar5 = a.velX() * c->mem_r16s(node + 0x4e);
    int32_t iVar7 = a.velZ() * c->mem_r16s(node + 0x4e);
    if ((a.stateEcho_u() & 1) == 0) {
      iVar5 = (int32_t)a.posXFixed() + iVar5;
      iVar7 = (int32_t)a.posZFixed() + iVar7;
    } else {
      iVar5 = (int32_t)a.posXFixed() - iVar5;
      iVar7 = (int32_t)a.posZFixed() - iVar7;
    }
    a.setPosXFixed((uint32_t)iVar5);
    a.setPosZFixed((uint32_t)iVar7);
    a.setPosY((uint16_t)(a.posY() + a.accelY()));
    a.setRotZ((uint16_t)(a.rotZ() + 0xcc));
    bool despawnNow = (c->mem_r8(node + 0x29) != 0);
    if (!despawnNow) {
      c->r[R_A0] = node;
      c->r[R_A1] = 0;
      c->r[R_A2] = 0;
      psx::cpu::dispatchGuestToReturn0(*c, kLeaf_800495DC, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
      despawnNow = (c->r[R_V0] != 0);
    }
    if (despawnNow) {
      c->r[R_A0] = node + 0x2c;
      psx::cpu::dispatchGuestToReturn0(*c, kLeaf_800315D4, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
      c->r[R_A0] = 0x1bu;
      c->r[R_A1] = 0;
      c->r[R_A2] = 0;
      psx::cpu::dispatchGuestToReturn0(*c, kSfxTrigger, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
      goto LAB_801451b0;
    }
    const int32_t sVar4 = a.accelY();
    a.setAccelY((uint16_t)(sVar4 + 4));
    if ((int16_t)(sVar4 + 4) > 0x3c) {
      a.setAccelY(0x3c);
    }
    goto LAB_801451a0;
  }
  goto switchD_caseD_3;

// Scope note: the case1/10/0xb jumps target function scope, so these labels stay outside
// the if/else above to avoid crossing initialized variables.
LAB_801451b0:
  c->mem_w8(node + 4, 3);
  return;

LAB_801451a0:
  call1(c, node, kAnimationStep);

switchD_caseD_3:
  call1(c, node, kGateCheck);
  {
    const int32_t iVar5 = (int32_t)c->r[R_V0];
    if (iVar5 == 0) {
      call1(c, node, kCullWrapperFlag2);
      if (c->r[R_V0] != 0) {
        call1(c, node, kObjMatrixCompose);
        uint8_t bVar6;
        if (c->mem_r8(node + 0x29) == 0) {
          bVar6 = (uint8_t)(c->mem_r8(node + 0xb) & 0x3f);
        } else {
          bVar6 = (uint8_t)((c->mem_r8(node + 0xb) & 0xc0) | 0x80);
        }
        c->mem_w8(node + 0xb, bVar6);
      }
    }
  }
  c->mem_w8(node + 0x29, 0);
  return;

switchD_caseD_8:
  if ((c->mem_r32(node + 4) & 0xffff00u) == 0x800u) {
    c->r[R_A0] = node;
    c->r[R_A1] = 0xffu;
    c->r[R_A2] = 0x30u;
    c->r[R_A3] = 0x30u;
    psx::cpu::dispatchGuestToReturn0(*c, kPaletteSideEffect, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
  switchD_caseD_7:;
  }
  if ((c->mem_r32(node + 4) & 0xffff00u) == 0x700u) {
    c->r[R_A0] = node;
    c->r[R_A1] = 0x20u;
    c->r[R_A2] = 0x30u;
    c->r[R_A3] = 0xffu;
    psx::cpu::dispatchGuestToReturn0(*c, kPaletteSideEffect, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
  }
  goto switchD_caseD_2;
}

} // namespace tomba::ai
