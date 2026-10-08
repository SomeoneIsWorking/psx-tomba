// game/render/sop_ground.h — the SOP intro's ground: the terrain blocks FUN_8010A0E0 culls into the
// visible list at 0x800F2418, drawn by FUN_80109FE0.
#pragma once

#include <cstdint>

struct Core;

class SopGround {
public:
  // FUN_80109FE0(list=a0). The list holds a count byte at +6, the block buffer at +0xC and one u16
  // word index per visible block from +0x10. A block is {u8 gt3, -, u8 gt4, -} then its GT3 records,
  // then its GT4 records; each is drawn under the scene camera (scratchpad 0x1F8000F8, GTE RT/TR) by
  // SOP's GT3/GT4 list emitters. Each block is its own drawn object: blocks are per map cell, so a
  // block keeps its address while it stays visible, whatever its slot in the list.
  static void draw(Core *c);
  static void registerOverrides();
};
