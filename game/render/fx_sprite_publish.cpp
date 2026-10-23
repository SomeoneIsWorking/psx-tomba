// game/render/fx_sprite_publish.cpp — scene camera load, anchor projection and scaled corners. See fx_sprite_publish.h.
#include "fx_sprite_publish.h"
#include "gte_registers.h"
#include "guest_abi.h"
#include "guest_ordering_table.h"

namespace tomba {

namespace {

using tomba::render::OrderingTable;

constexpr uint32_t kSceneCamera = 0x1F8000F8u; // rotation then translation, as GTE CR0-7 words
constexpr uint32_t kSceneCameraWords = 8;
constexpr int32_t kOtDepthShift = 2; // SZ3 >> 2 before the node's bias

} // namespace

void FxSpritePublish::loadSceneCamera(uint32_t dqa) const {
  psx::cpu::Gte &gte = mMemory.core().gte();
  for (uint32_t word = 0; word < kSceneCameraWords; word++) {
    gte.writeControl(psx::gte::kRotation + word, mMemory.mem_r32(kSceneCamera + word * 4));
  }
  gte.writeControl(psx::gte::kDqa, dqa);
  gte.writeControl(psx::gte::kDqb, 0);
}

bool FxSpritePublish::projectAnchor(const ProjectAnchorArgs &args) const {
  auto [worldXY, worldZ, otBias] = args;
  psx::cpu::Gte &gte = mMemory.core().gte();
  gte.writeData(psx::gte::kVxy0, worldXY);
  gte.writeData(psx::gte::kVz0, worldZ); // VZ0 takes the low half only
  gte.execute(psx::gte::kRtps);

  // The guest keeps the key in the scratchpad slot, so a culled anchor still leaves its trail there.
  setOtKey((int32_t)gte.readControl(psx::gte::kFlag));
  if (otKey() < 0) {
    return false;
  }
  setOtKey((int32_t)gte.readData(psx::gte::kSz3));
  const int32_t depth = otKey();
  if (depth <= 0) {
    return false;
  }
  setOtKey((depth >> kOtDepthShift) + otBias);
  if (otKey() < OrderingTable::kNearestBucket) {
    setOtKey(OrderingTable::kNearestBucket);
  }
  setOtKey(OrderingTable::compressDepth(otKey()));
  if (!OrderingTable::inDepthRange(otKey())) {
    setOtKey(OrderingTable::kNoBucket);
  }
  if (otKey() < 0) {
    return false;
  }
  mMemory.storeVertex(fxpublish::kScreenXY, gte.projected(2));
  setScaleX((int32_t)gte.readData(psx::gte::kMac0));
  return true;
}

void keepSceneCamera(render::ListJobWriter &job, Core &core) {
  job.input(core, kSceneCamera, kSceneCameraWords * 4, render::InputBlend::Camera);
}

psx::gte::ProjectedVertex FxSpritePublish::scaledCorner(const ScaledOffset &offset) const {
  Core *core = &mMemory.core();
  const uint32_t anchor = offset.anchor.word();
  const auto x = (uint16_t)(anchor + ((int32_t)psx::cpu::multiply(core, offset.x, offset.scaleX) >> 16));
  const auto y = (uint16_t)((anchor >> 16) + ((int32_t)psx::cpu::multiply(core, offset.y, offset.scaleY) >> 16));
  return offset.anchor.steppedTo(x | ((uint32_t)y << 16));
}

} // namespace tomba
