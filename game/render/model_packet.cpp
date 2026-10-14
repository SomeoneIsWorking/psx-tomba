// game/render/model_packet.cpp — the shared model emitter steps. See model_packet.h.
#include "model_packet.h"

#include "horizontal_visibility_cull.h"

#include <array>
#include <cstddef>
#include <span>

namespace tomba::render {
namespace {

using horizontal_cull::Domain;

constexpr std::int32_t kBucketCap = 0x800;

} // namespace

ModelPacket::ModelPacket(const psx::present::EmitMemory &memory,
                         const ModelShape &shape,
                         const Placement &placement,
                         ModelStage stage)
    : mMemory(memory), mShape(shape), mRecord(placement.record), mPacket(placement.packet), mStage(stage) {}

std::uint32_t ModelPacket::flags() const {
  return mMemory.mem_r32(mRecord + 4u) >> 24;
}

bool ModelPacket::transformFailed() const {
  mMemory.mem_w32(mStage.gte, mMemory.core().gte().readControl(psx::gte::kFlag));
  return static_cast<std::int32_t>(mMemory.mem_r32(mStage.gte)) < 0;
}

bool ModelPacket::facesAway() const {
  mMemory.mem_w32(mStage.gte, mMemory.core().gte().readData(psx::gte::kMac0));
  return static_cast<std::int32_t>(mMemory.mem_r32(mStage.gte)) <= 0;
}

// Corners 0..2 for RTPT; each record packs VZ0/VZ1 in one word and starts VZ2's word.
void ModelPacket::loadTriangle() const {
  const std::uint32_t xy0 = mRecord + mShape.triangle;
  const std::uint32_t z01 = mMemory.mem_r32(xy0 + 4u);
  mMemory.core().gte().writeData(psx::gte::kVxy0, mMemory.mem_r32(xy0));
  mMemory.core().gte().writeData(psx::gte::kVz0, z01);
  mMemory.core().gte().writeData(psx::gte::kVz1, z01 >> 16);
  mMemory.core().gte().writeData(psx::gte::kVxy1, mMemory.mem_r32(xy0 + 8u));
  mMemory.core().gte().writeData(psx::gte::kVxy2, mMemory.mem_r32(xy0 + 12u));
  mMemory.core().gte().writeData(psx::gte::kVz2, mMemory.mem_r32(xy0 + 16u));
}

void ModelPacket::storeTriangle() const {
  for (std::uint32_t corner = 0; corner < 3u; ++corner) {
    mMemory.storeGteXy(mPacket + kScreen[corner], static_cast<int>(psx::gte::kSxy0 + corner));
  }
}

// GT4 corner 3 into V0 for RTPS: VXY3 at record 0x28, VZ3 the top half of record 0x24.
void ModelPacket::loadCornerThree() const {
  mMemory.core().gte().writeData(psx::gte::kVxy0, mMemory.mem_r32(mRecord + 0x28u));
  mMemory.core().gte().writeData(psx::gte::kVz0, mMemory.mem_r32(mRecord + 0x24u) >> 16);
}

bool ModelPacket::onScreen(const horizontal_cull::Visibility &visible) const {
  std::array<std::uint32_t, 4> xs{};
  std::array<std::uint32_t, 4> ys{};
  for (int corner = 0; corner < mShape.cornerCount; ++corner) {
    xs[corner] = mMemory.mem_r16(mPacket + kScreen[corner]);
    ys[corner] = mMemory.mem_r16(mPacket + kScreen[corner] + 2u);
  }
  return visible.keepsX(xs.data(), mShape.cornerCount, Domain::Packed16) &&
         visible.keepsY(ys.data(), mShape.cornerCount, Domain::Packed16);
}

bool ModelPacket::projectGt3(std::uint32_t colourMask, const horizontal_cull::Visibility &visible) const {
  loadTriangle();
  mMemory.mem_w32(mPacket + 4u, mMemory.mem_r32(mRecord));
  mMemory.core().gte().execute(psx::gte::kRtpt);
  mMemory.mem_w32(mPacket + 0xCu, mMemory.mem_r32(mRecord + 8u));
  mMemory.mem_w32(mPacket + 0x18u, mMemory.mem_r32(mRecord + 0xCu));
  const std::uint32_t colour = mMemory.mem_r32(mRecord + 4u);
  if (transformFailed()) {
    return false;
  }
  mMemory.core().gte().execute(psx::gte::kNclip);
  mMemory.mem_w32(mPacket + 0x10u, colour & colourMask);
  if (facesAway()) {
    return false;
  }
  storeTriangle();
  if (!onScreen(visible)) {
    return false;
  }
  mMemory.mem_w32(mPacket + 0x1Cu, (colour << 4) & colourMask);
  return true;
}

bool ModelPacket::projectGt4(const ProjectGt4Args &args) const {
  auto [firstColourMask, colourMask, visible] = args;
  loadTriangle();
  const std::uint32_t colour01 = mMemory.mem_r32(mRecord);
  mMemory.mem_w32(mPacket + 4u, colour01 & firstColourMask);
  mMemory.core().gte().execute(psx::gte::kRtpt);
  mMemory.mem_w32(mPacket + 0x10u, (colour01 << 4) & colourMask);
  const std::uint32_t colour23 = mMemory.mem_r32(mRecord + 4u);
  if (transformFailed()) {
    return false;
  }
  mMemory.core().gte().execute(psx::gte::kNclip);
  mMemory.mem_w32(mPacket + 0xCu, mMemory.mem_r32(mRecord + 8u));
  if (facesAway()) {
    return false;
  }
  storeTriangle();
  loadCornerThree();
  mMemory.mem_w32(mPacket + 0x1Cu, colour23 & colourMask);
  mMemory.core().gte().execute(psx::gte::kRtps);
  mMemory.mem_w32(mPacket + 0x28u, (colour23 << 4) & colourMask);
  mMemory.mem_w32(mPacket + 0x18u, mMemory.mem_r32(mRecord + 0xCu));
  if (transformFailed()) {
    return false;
  }
  mMemory.storeGteXy(mPacket + kScreen[3], static_cast<int>(psx::gte::kSxy2));
  return onScreen(visible);
}

bool ModelPacket::projectGt4CornerThreeFirst(std::uint32_t colourMask,
                                             const horizontal_cull::Visibility &visible) const {
  loadCornerThree();
  const std::uint32_t colour23 = mMemory.mem_r32(mRecord + 4u);
  mMemory.mem_w32(mPacket + 0x1Cu, colour23 & colourMask);
  mMemory.core().gte().execute(psx::gte::kRtps);
  mMemory.mem_w32(mPacket + 0x28u, (colour23 << 4) & colourMask);
  mMemory.mem_w32(mPacket + 0x18u, mMemory.mem_r32(mRecord + 0xCu));
  if (transformFailed()) {
    return false;
  }
  mMemory.storeGteXy(mPacket + kScreen[3], static_cast<int>(psx::gte::kSxy2));
  loadTriangle();
  const std::uint32_t colour01 = mMemory.mem_r32(mRecord);
  mMemory.mem_w32(mPacket + 4u, colour01 & colourMask);
  mMemory.core().gte().execute(psx::gte::kRtpt);
  mMemory.mem_w32(mPacket + 0x10u, (colour01 << 4) & colourMask);
  if (transformFailed()) {
    return false;
  }
  mMemory.core().gte().execute(psx::gte::kNclip);
  mMemory.mem_w32(mPacket + 0xCu, mMemory.mem_r32(mRecord + 8u));
  if (facesAway()) {
    return false;
  }
  storeTriangle();
  return onScreen(visible);
}

void ModelPacket::stageDepths(std::uint32_t address) const {
  for (int corner = 0; corner < mShape.cornerCount; ++corner) {
    mMemory.mem_w32(address + static_cast<std::uint32_t>(corner) * 4u,
                    mMemory.core().gte().readData(mShape.firstDepth + static_cast<std::uint32_t>(corner)));
  }
}

void ModelPacket::depth(ModelDepth mode, std::uint32_t staged) const {
  if (mode == ModelDepth::Average) {
    mMemory.core().gte().execute(mShape.average);
    mMemory.mem_w32(mStage.bucket, mMemory.core().gte().readData(psx::gte::kOtz));
    return;
  }
  std::array<std::int32_t, 4> depths{};
  for (int corner = 0; corner < mShape.cornerCount; ++corner) {
    depths[corner] = static_cast<std::int32_t>(mMemory.mem_r32(staged + static_cast<std::uint32_t>(corner) * 4u));
  }
  const std::span<const std::int32_t> staged4(depths.data(), static_cast<std::size_t>(mShape.cornerCount));
  mMemory.mem_w32(mStage.bucket,
                  static_cast<std::uint32_t>(OrderingTable::extremeDepth(mode != ModelDepth::Nearest, staged4)));
}

void ModelPacket::biasBucket(std::int32_t bias) const {
  const std::int32_t biased = static_cast<std::int32_t>(mMemory.mem_r32(mStage.bucket)) + bias;
  mMemory.mem_w32(mStage.bucket, static_cast<std::uint32_t>(biased));
  if (biased >= kBucketCap) {
    mMemory.mem_w32(mStage.bucket, static_cast<std::uint32_t>(OrderingTable::kFarthestBucket));
  }
}

void ModelPacket::scrollU(std::uint32_t amount) const {
  for (int corner = 0; corner < mShape.cornerCount; ++corner) {
    const std::uint32_t u = mPacket + kUv[static_cast<std::size_t>(corner)];
    mMemory.mem_w8(u, static_cast<std::uint8_t>(mMemory.mem_r8(u) + amount));
  }
}

void ModelPacket::clampNear(const NearClamp &clamp) const {
  if (clamp.reach == 0u) {
    return;
  }
  const std::int32_t lifted = static_cast<std::int32_t>(mMemory.mem_r32(mStage.bucket)) + clamp.lift;
  mMemory.mem_w32(mStage.bucket, static_cast<std::uint32_t>(lifted));
  if (static_cast<std::uint32_t>(lifted - kNearDepth) + clamp.reach < clamp.reach) {
    mMemory.mem_w32(mStage.bucket, static_cast<std::uint32_t>(kNearDepth));
  }
}

std::int32_t ModelPacket::bucket() const {
  const std::int32_t index = OrderingTable::compressDepth(static_cast<std::int32_t>(mMemory.mem_r32(mStage.bucket)));
  mMemory.mem_w32(mStage.bucket, static_cast<std::uint32_t>(index));
  if (!OrderingTable::inDepthRange(index)) {
    mMemory.mem_w32(mStage.bucket, static_cast<std::uint32_t>(OrderingTable::kNoBucket));
  }
  return static_cast<std::int32_t>(mMemory.mem_r32(mStage.bucket));
}

void ModelPacket::storeLastUv() const {
  if (mShape.list == ModelList::Gt3) {
    mMemory.mem_w16(mPacket + kUv[2], mMemory.mem_r16(mRecord + 0x22u));
    return;
  }
  const std::uint32_t uv23 = mMemory.mem_r32(mRecord + 0x10u);
  mMemory.mem_w32(mPacket + kUv[2], uv23);
  mMemory.mem_w32(mPacket + kUv[3], uv23 >> 16);
}

void ModelPacket::shade(const ShadeArgs &args) const {
  const auto [corner, intensity] = args;
  const std::uint32_t colour = mPacket + kColour[corner];
  mMemory.core().gte().writeData(psx::gte::kIr0, static_cast<std::uint32_t>(intensity));
  mMemory.core().gte().writeData(psx::gte::kRgbc, mMemory.mem_r32(colour));
  mMemory.core().gte().execute(psx::gte::kDpcs);
  mMemory.mem_w32(colour, mMemory.core().gte().readData(psx::gte::kRgb2));
}

void ModelPacket::cueByDepth(int corner, std::uint32_t staged) const {
  const auto depth = static_cast<std::int32_t>(mMemory.mem_r32(staged + static_cast<std::uint32_t>(corner) * 4u));
  shade({.corner = corner, .intensity = depth >> 2});
}

void ModelPacket::link(std::uint32_t orderingTable) const {
  OrderingTable(mMemory, orderingTable)
      .link({.packet = mPacket, .words = mShape.packetWords, .bucket = mMemory.mem_r32(mStage.bucket)});
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

bool ModelPacket::hidden(const psx::present::EmitMemory &memory, std::uint32_t flags, std::uint32_t hideFlag) {
  return (flags & hideFlag) != 0u && memory.mem_r32(kHideWord) != 0u;
}

} // namespace tomba::render
