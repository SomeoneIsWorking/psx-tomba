// game/render/model_packet.cpp — the shared model emitter steps. See model_packet.h.
#include "model_packet.h"

#include "horizontal_visibility_cull.h"

namespace tomba2::render {
namespace {

using horizontal_cull::Domain;

constexpr std::uint32_t kHideWord = 0x1F80009Cu;
constexpr std::int32_t kBucketCap = 0x800;

} // namespace

ModelPacket::ModelPacket(
    Core &core, const ModelShape &shape, std::uint32_t record, std::uint32_t packet, ModelStage stage)
    : mCore(core), mShape(shape), mRecord(record), mPacket(packet), mStage(stage) {}

std::uint32_t ModelPacket::flags() const {
  return mCore.mem_r32(mRecord + 4u) >> 24;
}

bool ModelPacket::transformFailed() const {
  mCore.mem_w32(mStage.gte, gte_read_ctrl(gte::kFlag));
  return static_cast<std::int32_t>(mCore.mem_r32(mStage.gte)) < 0;
}

bool ModelPacket::facesAway() const {
  mCore.mem_w32(mStage.gte, gte_read_data(gte::kMac0));
  return static_cast<std::int32_t>(mCore.mem_r32(mStage.gte)) <= 0;
}

// Corners 0..2 for RTPT; each record packs VZ0/VZ1 in one word and starts VZ2's word.
void ModelPacket::loadTriangle() const {
  const std::uint32_t xy0 = mRecord + mShape.triangle;
  const std::uint32_t z01 = mCore.mem_r32(xy0 + 4u);
  gte_write_data(gte::kVxy0, mCore.mem_r32(xy0));
  gte_write_data(gte::kVz0, z01);
  gte_write_data(gte::kVz1, z01 >> 16);
  gte_write_data(gte::kVxy1, mCore.mem_r32(xy0 + 8u));
  gte_write_data(gte::kVxy2, mCore.mem_r32(xy0 + 12u));
  gte_write_data(gte::kVz2, mCore.mem_r32(xy0 + 16u));
}

void ModelPacket::storeTriangle() const {
  for (std::uint32_t corner = 0; corner < 3u; ++corner) {
    gte_store_xy(&mCore, mPacket + kScreen[corner], static_cast<int>(gte::kSxy0 + corner));
  }
}

// GT4 corner 3 into V0 for RTPS: VXY3 at record 0x28, VZ3 the top half of record 0x24.
void ModelPacket::loadCornerThree() const {
  gte_write_data(gte::kVxy0, mCore.mem_r32(mRecord + 0x28u));
  gte_write_data(gte::kVz0, mCore.mem_r32(mRecord + 0x24u) >> 16);
}

bool ModelPacket::onScreen(const horizontal_cull::Visibility &visible) const {
  std::array<std::uint32_t, 4> xs{};
  std::array<std::uint32_t, 4> ys{};
  for (int corner = 0; corner < mShape.cornerCount; ++corner) {
    xs[corner] = mCore.mem_r16(mPacket + kScreen[corner]);
    ys[corner] = mCore.mem_r16(mPacket + kScreen[corner] + 2u);
  }
  return visible.keepsX(xs.data(), mShape.cornerCount, Domain::Packed16) &&
         visible.keepsY(ys.data(), mShape.cornerCount, Domain::Packed16);
}

bool ModelPacket::projectGt3(std::uint32_t colourMask, const horizontal_cull::Visibility &visible) const {
  loadTriangle();
  mCore.mem_w32(mPacket + 4u, mCore.mem_r32(mRecord));
  gte_op(&mCore, gte::kRtpt);
  mCore.mem_w32(mPacket + 0xCu, mCore.mem_r32(mRecord + 8u));
  mCore.mem_w32(mPacket + 0x18u, mCore.mem_r32(mRecord + 0xCu));
  const std::uint32_t colour = mCore.mem_r32(mRecord + 4u);
  if (transformFailed()) {
    return false;
  }
  gte_op(&mCore, gte::kNclip);
  mCore.mem_w32(mPacket + 0x10u, colour & colourMask);
  if (facesAway()) {
    return false;
  }
  storeTriangle();
  if (!onScreen(visible)) {
    return false;
  }
  mCore.mem_w32(mPacket + 0x1Cu, (colour << 4) & colourMask);
  return true;
}

bool ModelPacket::projectGt4(std::uint32_t firstColourMask,
                             std::uint32_t colourMask,
                             const horizontal_cull::Visibility &visible) const {
  loadTriangle();
  const std::uint32_t colour01 = mCore.mem_r32(mRecord);
  mCore.mem_w32(mPacket + 4u, colour01 & firstColourMask);
  gte_op(&mCore, gte::kRtpt);
  mCore.mem_w32(mPacket + 0x10u, (colour01 << 4) & colourMask);
  const std::uint32_t colour23 = mCore.mem_r32(mRecord + 4u);
  if (transformFailed()) {
    return false;
  }
  gte_op(&mCore, gte::kNclip);
  mCore.mem_w32(mPacket + 0xCu, mCore.mem_r32(mRecord + 8u));
  if (facesAway()) {
    return false;
  }
  storeTriangle();
  loadCornerThree();
  mCore.mem_w32(mPacket + 0x1Cu, colour23 & colourMask);
  gte_op(&mCore, gte::kRtps);
  mCore.mem_w32(mPacket + 0x28u, (colour23 << 4) & colourMask);
  mCore.mem_w32(mPacket + 0x18u, mCore.mem_r32(mRecord + 0xCu));
  if (transformFailed()) {
    return false;
  }
  gte_store_xy(&mCore, mPacket + kScreen[3], static_cast<int>(gte::kSxy2));
  return onScreen(visible);
}

bool ModelPacket::projectGt4CornerThreeFirst(std::uint32_t colourMask,
                                             const horizontal_cull::Visibility &visible) const {
  loadCornerThree();
  const std::uint32_t colour23 = mCore.mem_r32(mRecord + 4u);
  mCore.mem_w32(mPacket + 0x1Cu, colour23 & colourMask);
  gte_op(&mCore, gte::kRtps);
  mCore.mem_w32(mPacket + 0x28u, (colour23 << 4) & colourMask);
  mCore.mem_w32(mPacket + 0x18u, mCore.mem_r32(mRecord + 0xCu));
  if (transformFailed()) {
    return false;
  }
  gte_store_xy(&mCore, mPacket + kScreen[3], static_cast<int>(gte::kSxy2));
  loadTriangle();
  const std::uint32_t colour01 = mCore.mem_r32(mRecord);
  mCore.mem_w32(mPacket + 4u, colour01 & colourMask);
  gte_op(&mCore, gte::kRtpt);
  mCore.mem_w32(mPacket + 0x10u, (colour01 << 4) & colourMask);
  if (transformFailed()) {
    return false;
  }
  gte_op(&mCore, gte::kNclip);
  mCore.mem_w32(mPacket + 0xCu, mCore.mem_r32(mRecord + 8u));
  if (facesAway()) {
    return false;
  }
  storeTriangle();
  return onScreen(visible);
}

void ModelPacket::stageDepths(std::uint32_t address) const {
  for (int corner = 0; corner < mShape.cornerCount; ++corner) {
    mCore.mem_w32(address + static_cast<std::uint32_t>(corner) * 4u,
                  gte_read_data(mShape.firstDepth + static_cast<std::uint32_t>(corner)));
  }
}

void ModelPacket::depth(ModelDepth mode, std::uint32_t staged) const {
  if (mode == ModelDepth::Average) {
    gte_op(&mCore, mShape.average);
    mCore.mem_w32(mStage.bucket, gte_read_data(gte::kOtz));
    return;
  }
  auto picked = static_cast<std::int32_t>(mCore.mem_r32(staged));
  for (int corner = 1; corner < mShape.cornerCount; ++corner) {
    const auto next = static_cast<std::int32_t>(mCore.mem_r32(staged + static_cast<std::uint32_t>(corner) * 4u));
    if (mode == ModelDepth::Nearest ? next < picked : next > picked) {
      picked = next;
    }
  }
  mCore.mem_w32(mStage.bucket, static_cast<std::uint32_t>(picked >> 2));
}

void ModelPacket::biasBucket(std::int32_t bias) const {
  const std::int32_t biased = static_cast<std::int32_t>(mCore.mem_r32(mStage.bucket)) + bias;
  mCore.mem_w32(mStage.bucket, static_cast<std::uint32_t>(biased));
  if (biased >= kBucketCap) {
    mCore.mem_w32(mStage.bucket, static_cast<std::uint32_t>(OrderingTable::kFarthestBucket));
  }
}

void ModelPacket::scrollU(std::uint32_t amount) const {
  for (int corner = 0; corner < mShape.cornerCount; ++corner) {
    const std::uint32_t u = mPacket + kUv[static_cast<std::size_t>(corner)];
    mCore.mem_w8(u, static_cast<std::uint8_t>(mCore.mem_r8(u) + amount));
  }
}

void ModelPacket::clampNear(const NearClamp &clamp) const {
  if (clamp.reach == 0u) {
    return;
  }
  const std::int32_t lifted = static_cast<std::int32_t>(mCore.mem_r32(mStage.bucket)) + clamp.lift;
  mCore.mem_w32(mStage.bucket, static_cast<std::uint32_t>(lifted));
  if (static_cast<std::uint32_t>(lifted - kNearDepth) + clamp.reach < clamp.reach) {
    mCore.mem_w32(mStage.bucket, static_cast<std::uint32_t>(kNearDepth));
  }
}

std::int32_t ModelPacket::bucket() const {
  const std::int32_t index = OrderingTable::compressDepth(static_cast<std::int32_t>(mCore.mem_r32(mStage.bucket)));
  mCore.mem_w32(mStage.bucket, static_cast<std::uint32_t>(index));
  if (!OrderingTable::inDepthRange(index)) {
    mCore.mem_w32(mStage.bucket, static_cast<std::uint32_t>(OrderingTable::kNoBucket));
  }
  return static_cast<std::int32_t>(mCore.mem_r32(mStage.bucket));
}

void ModelPacket::storeLastUv() const {
  if (mShape.list == ModelList::Gt3) {
    mCore.mem_w16(mPacket + kUv[2], mCore.mem_r16(mRecord + 0x22u));
    return;
  }
  const std::uint32_t uv23 = mCore.mem_r32(mRecord + 0x10u);
  mCore.mem_w32(mPacket + kUv[2], uv23);
  mCore.mem_w32(mPacket + kUv[3], uv23 >> 16);
}

void ModelPacket::shade(int corner, std::int32_t intensity) const {
  const std::uint32_t colour = mPacket + kColour[corner];
  gte_write_data(gte::kIr0, static_cast<std::uint32_t>(intensity));
  gte_write_data(gte::kRgbc, mCore.mem_r32(colour));
  gte_op(&mCore, gte::kDpcs);
  mCore.mem_w32(colour, gte_read_data(gte::kRgb2));
}

void ModelPacket::cueByDepth(int corner, std::uint32_t staged) const {
  const auto depth = static_cast<std::int32_t>(mCore.mem_r32(staged + static_cast<std::uint32_t>(corner) * 4u));
  shade(corner, depth >> 2);
}

void ModelPacket::link(std::uint32_t orderingTable) const {
  OrderingTable(mCore, orderingTable).link(mPacket, mShape.packetWords, mCore.mem_r32(mStage.bucket));
}

ModelDepth ModelPacket::litDepth(std::uint32_t flags) {
  switch (flags & 3u) {
  case 0u:
    return ModelDepth::Average;
  case 2u:
    return ModelDepth::Nearest;
  default:
    return ModelDepth::Farthest;
  }
}

ModelDepth ModelPacket::flaggedDepth(std::uint32_t flags) {
  switch (flags) {
  case 0u:
    return ModelDepth::Average;
  case 1u:
    return ModelDepth::Farthest;
  default:
    return ModelDepth::Nearest;
  }
}

ModelDepth ModelPacket::depthOf(std::uint32_t flags, std::uint32_t modeMask) {
  switch (flags & modeMask) {
  case 1u:
    return ModelDepth::Farthest;
  case 2u:
    return ModelDepth::Nearest;
  default:
    return ModelDepth::Average;
  }
}

bool ModelPacket::hidden(Core &core, std::uint32_t flags, std::uint32_t hideFlag) {
  return (flags & hideFlag) != 0u && core.mem_r32(kHideWord) != 0u;
}

} // namespace tomba2::render
