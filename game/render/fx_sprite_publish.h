// Scratchpad handoff from an emitter to the scaled-sprite writer FUN_80027A4C.
// Emitters: 0x80027CB4, 0x80027E5C (FxSpriteAnchored) and 0x800281EC (FxSpriteSwarm).
#pragma once
#include "core.h"
#include <stdint.h>

namespace fxpublish {
constexpr uint32_t kOtKey = 0x1F800080u;    // s32 OT bucket key, -1 when culled
constexpr uint32_t kScaleX = 0x1F800084u;   // s32 horizontal pixel scale
constexpr uint32_t kScaleY = 0x1F800088u;   // s32 vertical pixel scale
constexpr uint32_t kScreenXY = 0x1F80008Cu; // packed screen anchor from SXY2
constexpr uint32_t kDepthCue = 0x1F800090u; // s32 IR0 for the writer's DPCS colour cue

constexpr int32_t kDepthCueOff = 0; // IR0 = 0 leaves the colours untouched
} // namespace fxpublish

struct FxSpritePublish {
  Core *mCore;

  // Scene camera into GTE CR0-7; DQB = 0 so MAC0 = n * DQA, which the family uses as the sprite scale.
  void loadSceneCamera(uint32_t dqa) const;

  // RTPS one world anchor and gate its OT key in place, storing every step; true when it is on screen.
  bool projectAnchor(uint32_t worldXY, uint32_t worldZ, int32_t otBias);

  void setOtKey(int32_t v) {
    mCore->mem_w32(fxpublish::kOtKey, (uint32_t)v);
  }
  void setScaleX(int32_t v) {
    mCore->mem_w32(fxpublish::kScaleX, (uint32_t)v);
  }
  void setScaleY(int32_t v) {
    mCore->mem_w32(fxpublish::kScaleY, (uint32_t)v);
  }
  void setScreenXY(uint32_t v) {
    mCore->mem_w32(fxpublish::kScreenXY, v);
  }
  void setDepthCue(int32_t v) {
    mCore->mem_w32(fxpublish::kDepthCue, (uint32_t)v);
  }

  int32_t otKey() const {
    return (int32_t)mCore->mem_r32(fxpublish::kOtKey);
  }
  int32_t scaleX() const {
    return (int32_t)mCore->mem_r32(fxpublish::kScaleX);
  }
};
