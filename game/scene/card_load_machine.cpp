#include "card_load_machine.h"

#include "core.h"
#include "core/entry/game_ctx.h"
#include "core/overrides/native_override_catalog.h"
#include "game.h"
#include "guest_call.h"

namespace tomba::scene {

namespace {

inline constexpr std::uint32_t kMachineState = 0x800bf84au; // DAT_800bf84a: the sub-machine's state byte
inline constexpr std::uint32_t kLoadDone = 0x1f80019bu;     // set once the AREA-slot read completed
inline constexpr std::uint32_t kPageFlag137 = 0x1f800137u;  // ==1: state-index re-select suppressed
inline constexpr std::uint32_t kStateIndex = 0x1f80023bu;   // published area state index
inline constexpr std::uint32_t kCrdFile = 1u;
inline constexpr std::uint32_t kLoadPageIndex = 0x0cu;

// FUN_800750D8(idx, 1) after the page opens/closes: the DEMO page always selects the load page's
// index; the in-game pages re-select the area's own index unless 0x1F800137 suppresses it.
void reselectStateIndex(Core &c, std::uint32_t inGame) {
  if (inGame == 0u) {
    eng(&c).audioDispatch.dispatch3Way(kLoadPageIndex, 1u);
    return;
  }
  if (c.mem_r8(kPageFlag137) == 1u) {
    return;
  }
  eng(&c).audioDispatch.dispatch3Way(c.mem_r8(kStateIndex), 1u);
}

} // namespace

void stepCardLoadMachine(Core &core, std::uint32_t mode, std::uint32_t inGame) {
  Core *c = &core;
  const std::uint8_t state = c->mem_r8(kMachineState);
  switch (state) {
  case 0: {
    psx::cpu::callGuestNow(*c, __func__, 0x8001cf2cu);                // engine update
    native::loadAreaSlotFile(*c, eng(c).activeAreaOverlay, kCrdFile); // = FUN_80045558(1): CRD, SYNC
    c->mem_w8(kLoadDone, 1);                                          // mark the load complete (case 3 reads this)
    c->mem_w8(kMachineState, 1);
    reselectStateIndex(*c, inGame);
    break;
  }
  case 1:
    psx::cpu::callGuestNow(*c, __func__, 0x8007be18u, mode); // overlay slot browser
    if (c->mem_r16(c->mem_r32(kStatePtr) + 0x50) < 2u) {
      break;
    }
    if (inGame == 0u) {
      c->mem_w8(kMachineState, 3);
      break;
    }
    c->mem_w8(kMachineState, 2);
    psx::cpu::callGuestNow(*c, __func__, 0x8001cf2cu); // engine update
    break;
  case 2:
    // In-game only: FUN_80045580(1) (assetReady) starts the spawned reload of the area's DAT
    // payload over the AREA slot, which overwrites the CRD overlay's code, so its image retires
    // first. The guest never restores an OPN image here either (asset.cpp's 0x800BF89C==2 path).
    native::retireOverlay(*c, eng(c).activeAreaOverlay);
    psx::cpu::callGuestNow(*c, __func__, 0x80045580u, kCrdFile);
    if (c->r[2] != 0u) {
      c->mem_w8(kMachineState, 3);
    }
    break;
  case 3:
    if (c->mem_r8(kLoadDone) != 0u) {
      c->mem_w8(kMachineState, 4);
    }
    break;
  case 4: {
    psx::cpu::callGuestNow(*c, __func__, 0x8007be18u, mode); // run the chosen action (sets sm[0x6b])
    const bool loadAccepted = mode == 0u && c->mem_r8(c->mem_r32(kStatePtr) + 0x6b) == 7u;
    if (!loadAccepted && inGame == 1u) {
      reselectStateIndex(*c, inGame);
    }
    c->mem_w8(kMachineState, 0);
    break;
  }
  default:
    c->mem_w8(kMachineState, 0);
    break;
  }
}

} // namespace tomba::scene
