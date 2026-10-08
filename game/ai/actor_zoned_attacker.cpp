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

void ActorZonedAttacker::gateCheck(Core *c) {
  GuestFrame<24, 2> frame(c, kSpills_8014047C);
  const uint32_t node = c->r[R_A0];
  Actor a(c, node);
  const int8_t n66 = (int8_t)c->mem_r8(node + 0x66);
  bool result;
  if (n66 == (int8_t)0x81) {
    int32_t v = (int32_t)c->mem_r8(kGateTimer) - 0xc;
    result = (0x10 < v);
  } else if (n66 == (int8_t)0x80) {
    c->r[R_A0] = c->mem_r8(kGateTimer);
    c->r[R_A1] = kGateScratch;
    psx::cpu::dispatchGuestToReturn0(*c, kZoneClassify, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
    result = (c->r[R_V0] != 0);
  } else {
    const uint32_t typePtr = c->mem_r32(node + 0x14);
    int32_t iVar2 = (int32_t)c->mem_r32(typePtr + 0x48);
    if (iVar2 == -1) {
      iVar2 = 0;
      if ((int32_t)c->mem_r8(kGateTimer) > 2) {
        iVar2 = 1;
        if ((int32_t)c->mem_r8(kGateTimer) > 0xb) {
          c->r[R_V0] = 1;
          return;
        }
      }
    }
    c->r[R_A0] = c->mem_r8(node + 0x2a);
    c->r[R_A1] = node + 0x2c;
    psx::cpu::dispatchGuestToReturn0(*c, kZoneClassify, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
    const int32_t iVar3 = (int32_t)c->r[R_V0];
    result = (iVar2 != iVar3);
  }
  c->r[R_V0] = result ? 1u : 0u;
}

void ActorZonedAttacker::typeInit(Core *c) {
  GuestFrame<32, 4> frame(c, kSpills_80140544);
  const uint32_t node = c->r[R_A0];
  Actor a(c, node);
  if (c->mem_r16s(kGlobal_800ED098) < 0x12) {
    c->mem_w8(node + 9, 0);
    c->mem_w8(node + 4, 3);
    return;
  }
  c->mem_w8(node + 0xd, 0);
  c->mem_w8(node + 9, 0x12);
  // Direct 4-arg guest call; call1/call2 helpers only take fewer.
  c->r[R_A0] = node;
  c->r[R_A1] = 0x12u;
  c->r[R_A2] = c->mem_r32(kTableBase);
  c->r[R_A3] = kNodeFieldBase;
  psx::cpu::dispatchGuestToReturn0(*c, kInstallTypeTable, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
  c->mem_w32(node + 0x3c, c->mem_r32(kCopyScratch));
  call1(c, node, kGridResolve);
  if (c->r[R_V0] == 0) {
    c->mem_w8(node + 4, 3);
    return;
  }
  call1(c, node, kLeaf_80049674);
  const uint16_t v1a2 = c->mem_r16(kS_1F8001A2);
  const uint16_t v1a0 = c->mem_r16(kS_1F8001A0);
  a.setRotX(0);
  a.setRotZ(v1a2);
  a.setRotY(v1a0);
  c->mem_w16(node + 0x60, v1a0);
  c->mem_w8(node + 100, 0);
  const int32_t ix = c->mem_r16s(kS_1F800160);
  const int32_t iy = c->mem_r16s(kS_1F800164);
  a.setStateEcho(0);
  const int32_t s60 = a.triggerParam();
  c->r[R_A0] = node + 0x2c;
  c->r[R_A1] = (uint32_t)ix;
  c->r[R_A2] = (uint32_t)iy;
  psx::cpu::dispatchGuestToReturn0(*c, kLeaf_800782B0, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
  const int32_t s4 = (int32_t)(int16_t)c->r[R_V0];
  uint16_t v62 = a.stateEcho_u();
  const uint32_t val = (uint32_t)(s60 - s4 + 0x400) & 0xfffu;
  v62 = (val < 0x801u) ? (uint16_t)(v62 & 0xfffe) : (uint16_t)(v62 | 1);
  a.setStateEcho(v62);
  c->mem_w8(node + 0x5f, 0);
  c->mem_w8(node + 0x1b, (uint8_t)(c->mem_r8(node + 0x1b) & 0xbf));
  a.setInteractState(0);
  c->mem_w16(node + 4, 1);
  c->mem_w8(node + 0x66, c->mem_r8(node + 3));
  c->mem_w8(node + 0, 1);
  call2(c, node, kSetAnimStateCue, 1u, 0u);
  c->mem_w16(node + 0xbc, 0x1000);
  c->mem_w16(node + 0xba, 0x1000);
  c->mem_w16(node + 0xb8, 0x1000);
  c->mem_w16(node + 0x80, 0x38);
  c->mem_w16(node + 0x82, 0x70);
  c->mem_w16(node + 0x84, 0x8c);
  c->mem_w16(node + 0x4e, 0);
  a.setAccelY(0);
  c->mem_w16(node + 0x86, 0xf0);
  c->mem_w16(node + 0x7c, 0);
  c->mem_w16(node + 0x7e, 0);
}

void ActorZonedAttacker::pickAttackByRange(Core *c) {
  GuestFrame<40, 5> frame(c, kSpills_801409C0);
  const uint32_t node = c->r[R_A0];
  Actor a(c, node);
  psx::cpu::dispatchGuestToReturn0(*c, kRngRead, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
  const uint32_t rngv = c->r[R_V0];
  if (c->mem_r16(kS_800E7FFE) & 0x8200) {
    c->r[R_V0] = 0;
    return;
  }
  const int32_t nx = a.posX();
  const int32_t ny = a.posZ();
  const int32_t tx = c->mem_r16s(kS_1F800160);
  const int32_t ty = c->mem_r16s(kS_1F800164);
  c->r[R_A0] = (uint32_t)(int32_t)(int16_t)(nx - tx);
  c->r[R_A1] = (uint32_t)(int32_t)(int16_t)(ny - ty);
  psx::cpu::dispatchGuestToReturn0(*c, kDist2D, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
  const int32_t dist = (int32_t)(int16_t)c->r[R_V0];
  int32_t zone;
  if ((c->mem_r16(kS_800E7FFE) & 0x8200) == 0 && dist < 0x641) {
    if (dist < 0x44d) {
      zone = 2;
      if (dist > 600) {
        zone = 1;
      }
    } else {
      zone = 0;
    }
  } else {
    zone = -1;
  }
  uint32_t table;
  if (zone == 1) {
    table = kTable_8014BEE4;
  } else if (zone < 2) {
    if (zone != 0) {
      c->r[R_V0] = 0;
      return;
    }
    table = kTable_8014BED4;
  } else {
    if (zone != 2) {
      c->r[R_V0] = 0;
      return;
    }
    table = kTable_8014BEF4;
  }
  c->r[R_V0] = c->mem_r8(table + (rngv & 0xf));
}

void ActorZonedAttacker::approachAndFace(Core *c) {
  GuestFrame<32, 4> frame(c, kSpills_80144928);
  const uint32_t node = c->r[R_A0];
  Actor a(c, node);
  uint32_t uVar4 = 0;
  const uint8_t st = c->mem_r8(node + 7);

  goto label_dispatch;

label_ac8:
  c->mem_w8(node + 7, (uint8_t)(c->mem_r8(node + 7) + 1));
  goto label_caseD_7;

label_caseD_2: {
  const int32_t nx = a.posX();
  const int32_t ny = a.posY();
  const int32_t nz = a.posZ();
  const int32_t tx = c->mem_r16s(kS_1F800160);
  const int32_t ty = c->mem_r16s(kS_1F800162);
  const int32_t tz = c->mem_r16s(kS_1F800164);
  c->r[R_A0] = (uint32_t)(int32_t)(int16_t)(tx - nx);
  c->r[R_A1] = (uint32_t)(int32_t)(int16_t)(ty - ny);
  c->r[R_A2] = (uint32_t)(int32_t)(int16_t)(tz - nz);
  psx::cpu::dispatchGuestToReturn0(*c, kDist3D, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
  if (c->r[R_V0] < 0x3c0u) {
    goto label_ac8;
  }
  goto label_caseD_7;
}

label_caseD_7:
  a.setPosY((uint16_t)(a.posY() + 0x10));
  call1(c, node, kMotionAnimStep);
  c->r[R_V0] = uVar4;
  return;

label_dispatch:
  switch (st) {
  case 0:
  case 1: {
    uint32_t uVar3;
    switch (c->mem_r8(node + 3)) {
    default:
      uVar3 = 0x12;
      break;
    case 2:
      uVar3 = 7;
      break;
    case 3:
    case 4:
      uVar3 = 0x1b;
      break;
    case 5:
      uVar3 = 6;
      break;
    }
    call2(c, node, kSetAnimStateCue, uVar3, 0u);
    a.setStateEcho((uint16_t)(a.stateEcho_u() & 0xfffb));
    call1(c, node, kMotionAnimStep);
    c->mem_w8(node + 7, 2);
    goto label_caseD_2;
  }
  case 2:
    goto label_caseD_2;
  case 3: {
    uint32_t uVar3;
    switch (c->mem_r8(node + 3)) {
    default:
      uVar3 = 0x13;
      break;
    case 2:
      uVar3 = 7;
      uVar4 = 1;
      break;
    case 3:
    case 4:
      uVar3 = 0x1d;
      break;
    case 5:
      uVar3 = 0x34;
      break;
    }
    call2(c, node, kSetAnimStateCue, uVar3, 8u);
    c->mem_w16(node + 0x40, 0x28);
    c->mem_w8(node + 7, (uint8_t)(c->mem_r8(node + 7) + 1));
    [[fallthrough]];
  }
  case 4: {
    const uint16_t v = (uint16_t)(c->mem_r16(node + 0x40) - 1);
    c->mem_w16(node + 0x40, v);
    if ((int16_t)v > 0) {
      goto label_caseD_7;
    }
    goto label_ac8;
  }
  case 5:
    call2(c, node, kSetAnimStateCue, 7u, 8u);
    c->mem_w16(node + 0x40, 0x1e);
    c->mem_w8(node + 7, (uint8_t)(c->mem_r8(node + 7) + 1));
    [[fallthrough]];
  case 6: {
    const uint16_t v = (uint16_t)(c->mem_r16(node + 0x40) - 1);
    c->mem_w16(node + 0x40, v);
    if ((int16_t)v < 1) {
      uVar4 = 1;
    }
    goto label_caseD_7;
  }
  default:
    goto label_caseD_7;
  }
}

void ActorZonedAttacker::registerOverrides(Game * /*game*/) {
  // All seven are A00 functions; 0x80145C78 and 0x80140544 were matched by the
  // split-prologue entry shape. docs/issues/0015.
  tomba::native::declareOverlayOverride(
      "A00", kZoneClassify, "ActorZonedAttacker::zoneClassify", &ActorZonedAttacker::zoneClassify);
  tomba::native::declareOverlayOverride(
      "A00", 0x80140544u, "ActorZonedAttacker::typeInit", ActorZonedAttacker::typeInit);
  tomba::native::declareOverlayOverride(
      "A00", kGateCheck, "ActorZonedAttacker::gateCheck", ActorZonedAttacker::gateCheck);
  tomba::native::declareOverlayOverride(
      "A00", kPickAttackByRange, "ActorZonedAttacker::pickAttackByRange", ActorZonedAttacker::pickAttackByRange);
  tomba::native::declareOverlayOverride(
      "A00", 0x80143A00u, "ActorZonedAttacker::defaultSubStateMachine", ActorZonedAttacker::defaultSubStateMachine);
  tomba::native::declareOverlayOverride(
      "A00", 0x80144928u, "ActorZonedAttacker::approachAndFace", ActorZonedAttacker::approachAndFace);
  tomba::native::declareOverlayOverride(
      "A00", 0x80144B50u, "ActorZonedAttacker::idleTick", ActorZonedAttacker::idleTick);
}

} // namespace tomba::ai
