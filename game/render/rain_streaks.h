// game/render/rain_streaks.h — A08's rain: FUN_80116904, a render node's custom draw (node+24).
#pragma once

#include <cstdint>

struct Core;

class RainStreaks {
public:
  // FUN_80116904(node=a0). 32 drops on a 2048-unit lattice around the camera, placed by the LCG
  // seed = seed * *(0x801450D8) + 1 from node+0x50 (0 in A08), so drop i is the same lattice point
  // every frame. Each visible drop draws a semi-transparent LINE from its screen position back
  // twice its last movement, kept per drop at 0x801485E8 + 4i, plus a DR_TPAGE. A drop whose
  // lattice cell wrapped, or that fails RTPS, restarts its streak. Drop i is element i of the node.
  static void draw(Core *c);
  static void registerOverrides();
};
