// game/render/fx_sprite_anchored.h — FUN_80027CB4 and FUN_80027E5C, the single-anchor emitters of the
// FUN_80027A4C scaled-sprite family.
#pragma once
#include "core.h"
#include "fx_sprite_publish.h"
#include <stdint.h>

class Game;

// Type-0x20 render node; these emitters are its render fn at node+0x18.
namespace fxanchored {
constexpr uint32_t kScaleByte = 0x06;     // u8 size multiplier, 4.4 fixed point; 0x80027E5C only
constexpr uint32_t kWorldAnchorXY = 0x2C; // u32 packed world VX (lo16) | VY (hi16)
constexpr uint32_t kWorldAnchorZ = 0x30;  // u32 world VZ in the low half; the high half is kOtBias
constexpr uint32_t kOtBias = 0x32;        // s16 added to the OT bucket key
constexpr uint32_t kRecordHead = 0x34;    // u32 8-byte sprite-record list, the writer's a0
constexpr uint32_t kRecordTail = 0x38;    // u32 tail the writer returns
constexpr uint32_t kClut = 0x44;          // u16 CLUT id, low half of the writer's a1
constexpr uint32_t kTexturePage = 0x46;   // u16 texture page, high half of the writer's a1
} // namespace fxanchored

struct FxAnchoredNode {
  Core *mCore;
  uint32_t mBase;

  uint32_t scaleByte() const {
    return mCore->mem_r8(mBase + fxanchored::kScaleByte);
  }
  uint32_t worldAnchorXY() const {
    return mCore->mem_r32(mBase + fxanchored::kWorldAnchorXY);
  }
  uint32_t worldAnchorZ() const {
    return mCore->mem_r32(mBase + fxanchored::kWorldAnchorZ);
  }
  int32_t otBias() const {
    return mCore->mem_r16s(mBase + fxanchored::kOtBias);
  }
  uint32_t recordHead() const {
    return mCore->mem_r32(mBase + fxanchored::kRecordHead);
  }
  uint32_t clut() const {
    return mCore->mem_r16(mBase + fxanchored::kClut);
  }
  uint32_t texturePage() const {
    return mCore->mem_r16(mBase + fxanchored::kTexturePage);
  }

  void setRecordTail(uint32_t v) {
    mCore->mem_w32(mBase + fxanchored::kRecordTail, v);
  }
};

class FxSpriteAnchored {
public:
  // FUN_80027CB4: stamp the node's sprite cluster at its own anchor, scaled by depth only. a0 = node.
  static void emitUniformScale(Core *c);

  // FUN_80027E5C: as above, with the depth scale multiplied by the node's size byte. a0 = node.
  static void emitByteScale(Core *c);

  static void registerOverrides(Game *game);

private:
  // Project the node in s0; a node with no records or a culled anchor returns false, culled via the resolver.
  static bool projectNode(Core *c, uint32_t raListTailResolve);
  // Call the writer on the node in s0 and keep the tail it returns.
  static void stamp(Core *c, uint32_t raSpriteWriter);
};
