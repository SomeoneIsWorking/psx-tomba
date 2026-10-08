#pragma once

#include <cstdint>

class Core;

namespace tomba::scene {

inline constexpr std::uint32_t kStatePtr = 0x1f800138u;

// Advance the memory-card LOAD/SAVE page sub-machine, guest FUN_8007BF20(mode, inGame), one step.
// mode is the browser mode handed to the resident UI driver FUN_8007BE18 (0 = load page,
// 0x81 = save page); inGame selects the caller: 0 for the DEMO title-screen load page, 1 for the
// pause-menu pages, which additionally reload the area's DAT payload over the card overlay's slot
// (states 2/3) and re-select the area's state index afterwards.
//
// Native+SYNC because case 0's disc read (FUN_80045558(1) = FUN_8001DC40 to 0x8018A000) would
// spin forever in the no-IRQ runtime. This is also the one owner of CRD code-image residency in the
// AREA slot: the overlay is activated right after its synchronous load and retired before the
// in-game area-asset reload overwrites it.
void stepCardLoadMachine(Core &core, std::uint32_t mode, std::uint32_t inGame);

} // namespace tomba::scene
