// game/render/fx_sprite_swarm.h — FUN_800281EC, the per-particle emitter of the FUN_80027A4C
// scaled-sprite family.
#pragma once
#include "core.h"
#include "fx_sprite_publish.h"
#include <stdint.h>

class Game;

// Type-0x20 render node; 0x800281EC is its render fn at node+0x18. Its own anchor at +0x2C is unused.
namespace fxswarm {
constexpr uint32_t kSizeClass = 0x03;     // u8 node tag; '!' selects the small size class
constexpr uint32_t kOtBias = 0x32;        // s16 added to the OT bucket key
constexpr uint32_t kRecordHead = 0x34;    // u32 8-byte sprite-record list, the writer's a0
constexpr uint32_t kClut = 0x44;          // u16 CLUT id, low half of the writer's a1
constexpr uint32_t kTexturePage = 0x46;   // u16 texture page, high half of the writer's a1
constexpr uint32_t kParticleCount = 0x4E; // s16 live particles
constexpr uint32_t kParticleArray = 0x50; // particle records, stride 8

constexpr uint32_t kParticleStride = 8;
constexpr uint32_t kPartPosXY = 0x00;   // u32 packed world VX (lo16) | VY (hi16)
constexpr uint32_t kPartPosZ = 0x04;    // u32 world VZ in the low half; the high half is kPartSizeMul
constexpr uint32_t kPartSizeMul = 0x06; // s16 particle size, 8.8 fixed point
} // namespace fxswarm

struct FxSwarmNode {
  Core *mCore;
  uint32_t mBase;

  uint32_t sizeClassTag() const {
    return mCore->mem_r8(mBase + fxswarm::kSizeClass);
  }
  int32_t otBias() const {
    return mCore->mem_r16s(mBase + fxswarm::kOtBias);
  }
  uint32_t recordHead() const {
    return mCore->mem_r32(mBase + fxswarm::kRecordHead);
  }
  uint32_t clut() const {
    return mCore->mem_r16(mBase + fxswarm::kClut);
  }
  uint32_t texturePage() const {
    return mCore->mem_r16(mBase + fxswarm::kTexturePage);
  }
  int32_t particleCount() const {
    return mCore->mem_r16s(mBase + fxswarm::kParticleCount);
  }
  uint32_t particleArray() const {
    return mBase + fxswarm::kParticleArray;
  }
};

struct FxSwarmParticle {
  Core *mCore;
  uint32_t mBase;

  uint32_t worldXY() const {
    return mCore->mem_r32(mBase + fxswarm::kPartPosXY);
  }
  uint32_t worldZ() const {
    return mCore->mem_r32(mBase + fxswarm::kPartPosZ);
  }
  int32_t sizeMul() const {
    return mCore->mem_r16s(mBase + fxswarm::kPartSizeMul);
  }
};

class FxSpriteSwarm {
public:
  // FUN_800281EC: stamp the node's sprite cluster once per particle at its own position and size. a0 = node.
  static void emitPerParticle(Core *c);

  static void registerOverrides(Game *game);
};
