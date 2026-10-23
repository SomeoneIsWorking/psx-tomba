// game/render/fx_sprite_publish.h — scratchpad handoff from a sprite emitter to its record writer.
// Writers: ScaledSprites (FUN_80027A4C) and PackedMesh::emitScreen (FUN_8002847C).
#pragma once
#include "emit_memory.h"
#include "list_job.h"
#include "projected_vertex.h"
#include <stdint.h>

namespace fxpublish {
constexpr uint32_t kOtKey = 0x1F800080u;    // s32 OT bucket key, -1 when culled
constexpr uint32_t kScaleX = 0x1F800084u;   // s32 horizontal pixel scale
constexpr uint32_t kScaleY = 0x1F800088u;   // s32 vertical pixel scale
constexpr uint32_t kScreenXY = 0x1F80008Cu; // packed screen anchor from SXY2, with its place
constexpr uint32_t kDepthCue = 0x1F800090u; // s32 IR0 for the writer's colour cue

constexpr int32_t kDepthCueOff = 0; // IR0 = 0 leaves the colours untouched
} // namespace fxpublish

namespace tomba {

struct FxSpritePublish {
  psx::present::EmitMemory mMemory;

  // Scene camera into GTE CR0-7; DQB = 0 so MAC0 = n * DQA, which the family uses as the sprite scale.
  void loadSceneCamera(uint32_t dqa) const;

  struct ProjectAnchorArgs {
    uint32_t worldXY;
    uint32_t worldZ;
    int32_t otBias;
  };
  // RTPS one world anchor and gate its OT key in place, storing every step; true when it is on screen.
  bool projectAnchor(const ProjectAnchorArgs &args) const;

  void setOtKey(int32_t v) const {
    mMemory.mem_w32(fxpublish::kOtKey, (uint32_t)v);
  }
  void setScaleX(int32_t v) const {
    mMemory.mem_w32(fxpublish::kScaleX, (uint32_t)v);
  }
  void setScaleY(int32_t v) const {
    mMemory.mem_w32(fxpublish::kScaleY, (uint32_t)v);
  }
  void setDepthCue(int32_t v) const {
    mMemory.mem_w32(fxpublish::kDepthCue, (uint32_t)v);
  }

  int32_t otKey() const {
    return (int32_t)mMemory.mem_r32(fxpublish::kOtKey);
  }
  int32_t scaleX() const {
    return (int32_t)mMemory.mem_r32(fxpublish::kScaleX);
  }

  // A writer's corner: each anchor halfword plus the high half of offset * scale, kept to 16 bits; leaves y in HI/LO.
  struct ScaledOffset {
    psx::gte::ProjectedVertex anchor;
    int32_t x;
    int32_t y;
    int32_t scaleX;
    int32_t scaleY;
  };
  psx::gte::ProjectedVertex scaledCorner(const ScaledOffset &offset) const;
};

// The scene camera an emitter loads, kept in its saved call so a render moves it as a transform.
void keepSceneCamera(render::ListJobWriter &job, Core &core);

} // namespace tomba
