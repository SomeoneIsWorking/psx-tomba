// libcd_native.cpp — see header.
#include "libcd_native.h"
#include "core.h"
#include "guest_call.h" // rc0/rc1/rc2 wrappers around typed runtime address dispatch

// guest execution leaves (declared in authenticated executable/overlay evidence). Callable directly — they are
// leaf-ish (no coroutine yields inside) and preserve the caller-save contract, so we can enter
// them synchronously via typed runtime address dispatch and read the return value out of $v0 (c->r[2]).
//
// Guest ABI: a0=r[4], a1=r[5], v0=r[2].

uint32_t LibcdNative::newMedia() {
  psx::cpu::dispatchGuestToReturn0(*core, 0x8008BBE8u, psx::cpu::ExecutionBudget::currentTurn(*core), __func__);
  return core->r[2];
}

uint32_t LibcdNative::cacheFile(uint32_t dir_index) {
  psx::cpu::dispatchGuestToReturn1(
      *core, 0x8008BF50u, dir_index, psx::cpu::ExecutionBudget::currentTurn(*core), __func__);
  return core->r[2];
}

uint32_t LibcdNative::searchFile(uint32_t out_cdlfile_guest_addr, uint32_t path_guest_addr, uint32_t ra) {
  core->r[31] = ra; // guest call-site — spilled by CdSearchFile's prologue
  psx::cpu::dispatchGuestToReturn2(*core,
                                   0x8008B8F0u,
                                   out_cdlfile_guest_addr,
                                   path_guest_addr,
                                   psx::cpu::ExecutionBudget::currentTurn(*core),
                                   __func__);
  return core->r[2];
}

uint32_t LibcdNative::posToInt(uint32_t cdlfile_guest_addr, uint32_t ra) {
  core->r[31] = ra;
  psx::cpu::dispatchGuestToReturn1(
      *core, 0x8008A110u, cdlfile_guest_addr, psx::cpu::ExecutionBudget::currentTurn(*core), __func__);
  return core->r[2];
}
