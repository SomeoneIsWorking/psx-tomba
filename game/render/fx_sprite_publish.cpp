// game/render/fx_sprite_publish.cpp — scene camera load and anchor projection. See fx_sprite_publish.h.
#include "fx_sprite_publish.h"
#include "gte_registers.h"
#include "guest_ordering_table.h"

namespace {

using tomba2::render::OrderingTable;

constexpr uint32_t kSceneCamera = 0x1F8000F8u; // rotation then translation, as GTE CR0-7 words
constexpr uint32_t kSceneCameraWords = 8;
constexpr int32_t kOtDepthShift = 2; // SZ3 >> 2 before the node's bias

} // namespace

void FxSpritePublish::loadSceneCamera(uint32_t dqa) const {
  for (uint32_t word = 0; word < kSceneCameraWords; word++) {
    gte_write_ctrl(psx::gte::kRotation + word, mCore->mem_r32(kSceneCamera + word * 4));
  }
  gte_write_ctrl(psx::gte::kDqa, dqa);
  gte_write_ctrl(psx::gte::kDqb, 0);
}

bool FxSpritePublish::projectAnchor(uint32_t worldXY, uint32_t worldZ, int32_t otBias) {
  gte_write_data(psx::gte::kVxy0, worldXY);
  gte_write_data(psx::gte::kVz0, worldZ); // VZ0 takes the low half only
  gte_op(mCore, psx::gte::kRtps);

  // The guest keeps the key in the scratchpad slot, so a culled anchor still leaves its trail there.
  setOtKey((int32_t)gte_read_ctrl(psx::gte::kFlag));
  if (otKey() < 0) {
    return false;
  }
  setOtKey((int32_t)gte_read_data(psx::gte::kSz3));
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
  setScreenXY(gte_read_data(psx::gte::kSxy2));
  setScaleX((int32_t)gte_read_data(psx::gte::kMac0));
  return true;
}
