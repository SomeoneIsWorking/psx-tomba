// game/render/model_packet.cpp — the shared model emitter steps. See model_packet.h.
#include "model_packet.h"

#include "core/overrides/guest_jal.h"
#include "horizontal_visibility_cull.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdlib>
#include <span>
#include <utility>

namespace tomba::render {
namespace {

using psx::present::CornerDomain;

constexpr std::int32_t kBucketCap = 0x800;
constexpr std::uint32_t kRcos = 0x80083F50u;
constexpr std::uint32_t kRsin = 0x80083E80u;
constexpr std::uint32_t kFocus = 0x1F800160u;      // s16 x, y, z
constexpr std::uint32_t kFocusAngle = 0x1F8000C0u; // s16 -rcos, -rsin of the drawer's angle
constexpr std::int32_t kFocusLift = 0x100;
constexpr int kQ12 = 12;

// The guest's 32-bit mult/mflo and addu, which wrap.
std::int32_t mul32(std::int32_t a, std::int32_t b) {
  return static_cast<std::int32_t>(static_cast<std::uint32_t>(static_cast<std::int64_t>(a) * b));
}
std::int32_t add32(std::int32_t a, std::int32_t b) {
  return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) + static_cast<std::uint32_t>(b));
}

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
    mMemory.storeVertex(mPacket + kScreen[corner], mMemory.core().gte().projected(corner));
  }
}

// GT4 corner 3 into V0 for RTPS: VXY3 at record 0x28, VZ3 the top half of record 0x24.
void ModelPacket::loadCornerThree() const {
  mMemory.core().gte().writeData(psx::gte::kVxy0, mMemory.mem_r32(mRecord + 0x28u));
  mMemory.core().gte().writeData(psx::gte::kVz0, mMemory.mem_r32(mRecord + 0x24u) >> 16);
}

bool ModelPacket::onScreen(const psx::present::ScreenCull &visible) const {
  std::array<std::uint32_t, 4> xs{};
  std::array<std::uint32_t, 4> ys{};
  for (int corner = 0; corner < mShape.cornerCount; ++corner) {
    xs[corner] = mMemory.mem_r16(mPacket + kScreen[corner]);
    ys[corner] = mMemory.mem_r16(mPacket + kScreen[corner] + 2u);
  }
  return visible.keepsX(xs.data(), mShape.cornerCount, CornerDomain::Packed16) &&
         visible.keepsY(ys.data(), mShape.cornerCount, CornerDomain::Packed16);
}

bool ModelPacket::projectGt3(std::uint32_t colourMask, const psx::present::ScreenCull &visible, bool twoSided) const {
  loadTriangle();
  mMemory.mem_w32(mPacket + 4u, mMemory.mem_r32(mRecord));
  mMemory.core().gte().execute(psx::gte::kRtpt);
  mMemory.mem_w32(mPacket + 0xCu, mMemory.mem_r32(mRecord + 8u));
  mMemory.mem_w32(mPacket + 0x18u, mMemory.mem_r32(mRecord + 0xCu));
  const std::uint32_t colour = mMemory.mem_r32(mRecord + 4u);
  if (transformFailed()) {
    return false;
  }
  mMemory.mem_w32(mPacket + 0x10u, colour & colourMask);
  if (!twoSided) {
    mMemory.core().gte().execute(psx::gte::kNclip);
    if (facesAway()) {
      return false;
    }
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
  mMemory.storeVertex(mPacket + kScreen[3], mMemory.core().gte().projected(2));
  return onScreen(visible);
}

bool ModelPacket::projectGt4CornerThreeFirst(std::uint32_t colourMask, const psx::present::ScreenCull &visible) const {
  loadCornerThree();
  const std::uint32_t colour23 = mMemory.mem_r32(mRecord + 4u);
  mMemory.mem_w32(mPacket + 0x1Cu, colour23 & colourMask);
  mMemory.core().gte().execute(psx::gte::kRtps);
  mMemory.mem_w32(mPacket + 0x28u, (colour23 << 4) & colourMask);
  mMemory.mem_w32(mPacket + 0x18u, mMemory.mem_r32(mRecord + 0xCu));
  if (transformFailed()) {
    return false;
  }
  mMemory.storeVertex(mPacket + kScreen[3], mMemory.core().gte().projected(2));
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
  std::array<std::int32_t, 4> depths{};
  if (mode != ModelDepth::Average) {
    for (int corner = 0; corner < mShape.cornerCount; ++corner) {
      depths[corner] = static_cast<std::int32_t>(mMemory.mem_r32(staged + static_cast<std::uint32_t>(corner) * 4u));
    }
  }
  storeDepth(mode, depths);
}

void ModelPacket::depthFromRegisters(ModelDepth mode) const {
  std::array<std::int32_t, 4> depths{};
  if (mode != ModelDepth::Average) {
    for (int corner = 0; corner < mShape.cornerCount; ++corner) {
      depths[corner] = static_cast<std::int32_t>(
          mMemory.core().gte().readData(mShape.firstDepth + static_cast<std::uint32_t>(corner)));
    }
  }
  storeDepth(mode, depths);
}

void ModelPacket::storeDepth(ModelDepth mode, const std::array<std::int32_t, 4> &depths) const {
  if (mode == ModelDepth::Average) {
    mMemory.core().gte().execute(mShape.average);
    mMemory.mem_w32(mStage.bucket, mMemory.core().gte().readData(psx::gte::kOtz));
    return;
  }
  const std::span<const std::int32_t> corners(depths.data(), static_cast<std::size_t>(mShape.cornerCount));
  mMemory.mem_w32(mStage.bucket,
                  static_cast<std::uint32_t>(OrderingTable::extremeDepth(mode != ModelDepth::Nearest, corners)));
}

void ModelPacket::biasBucket(std::int32_t bias) const {
  const std::int32_t biased = static_cast<std::int32_t>(mMemory.mem_r32(mStage.bucket)) + bias;
  mMemory.mem_w32(mStage.bucket, static_cast<std::uint32_t>(biased));
  if (biased >= kBucketCap) {
    mMemory.mem_w32(mStage.bucket, static_cast<std::uint32_t>(OrderingTable::kFarthestBucket));
  }
}

void ModelPacket::scrollUv(std::uint32_t u, std::uint32_t v) const {
  for (int corner = 0; corner < mShape.cornerCount; ++corner) {
    const std::uint32_t uv = mPacket + kUv[static_cast<std::size_t>(corner)];
    mMemory.mem_w8(uv, static_cast<std::uint8_t>(mMemory.mem_r8(uv) + u));
    mMemory.mem_w8(uv + 1u, static_cast<std::uint8_t>(mMemory.mem_r8(uv + 1u) + v));
  }
}

void ModelPacket::setBucket(std::int32_t bucket) const {
  mMemory.mem_w32(mStage.bucket, static_cast<std::uint32_t>(bucket));
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

std::int32_t ModelPacket::bucket(std::int32_t bandBias) const {
  const std::int32_t index =
      OrderingTable::bucketOf(static_cast<std::int32_t>(mMemory.mem_r32(mStage.bucket)), bandBias);
  mMemory.mem_w32(mStage.bucket, static_cast<std::uint32_t>(index));
  return index;
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

void ModelPacket::cueByBucket(int corner) const {
  shade({.corner = corner, .intensity = static_cast<std::int32_t>(mMemory.mem_r32(mStage.bucket) << 1)});
}

void ModelPacket::cueByFocusDistance(int corner) const {
  const ModelCorner &vertex = mShape.corners[static_cast<std::size_t>(corner)];
  const std::int32_t cosine = mMemory.mem_r16s(kFocusAngle);
  const std::int32_t sine = mMemory.mem_r16s(kFocusAngle + 2u);
  const std::int32_t dx = mMemory.mem_r16s(mRecord + vertex.x) - mMemory.mem_r16s(kFocus);
  const std::int32_t dz = mMemory.mem_r16s(mRecord + vertex.z) - mMemory.mem_r16s(kFocus + 4u);
  const std::int32_t along = (add32(mul32(dx, sine), mul32(dz, cosine)) >> kQ12) * 2;
  std::int32_t across = (add32(mul32(-dx, cosine), mul32(dz, sine)) >> kQ12) * 2;
  if (across < 0) {
    across *= 2;
  }
  std::int32_t major = std::abs(add32(mul32(along, sine), -mul32(across, cosine)) >> kQ12);
  std::int32_t minor = std::abs(add32(mul32(along, cosine), mul32(across, sine)) >> kQ12);
  if (major < minor) {
    std::swap(major, minor);
  }
  const std::int32_t side = minor + (minor >> 1);
  std::int32_t length = major - (major >> 5) - (major >> 7) + (side >> 2) + (side >> 6);
  length -= length >> 2;
  const std::int32_t dy = mMemory.mem_r16s(mRecord + vertex.y) - mMemory.mem_r16s(kFocus + 2u) - kFocusLift;
  shade({.corner = corner, .intensity = std::max(length + std::abs(dy), 0)});
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

bool ModelPacket::hiddenByX(const ModelXHide &rule) const {
  if (!rule.active || mMemory.mem_r32(kHideWord) == 0u) {
    return false;
  }
  if (mShape.list == ModelList::Gt3) {
    return mMemory.mem_r16s(mRecord + mShape.corners[2].x) <= rule.gt3;
  }
  return mMemory.mem_r16s(mRecord + mShape.corners[0].x) <= rule.gt4 &&
         mMemory.mem_r16s(mRecord + mShape.corners[1].x) <= rule.gt4;
}

bool ModelPacket::hidden(const psx::present::EmitMemory &memory, std::uint32_t flags, std::uint32_t hideFlag) {
  return (flags & hideFlag) != 0u && memory.mem_r32(kHideWord) != 0u;
}

std::int32_t emitterTrig(const psx::present::EmitMemory &memory, EmitterTrigCall call, std::int32_t angle) {
  Core &core = memory.core();
  return static_cast<std::int32_t>(
      tomba::guest::dispatchJalToReturn(core,
                                        {.target = call.cosine ? kRcos : kRsin, .returnAddress = call.returnAddress},
                                        static_cast<std::uint32_t>(angle)));
}

} // namespace tomba::render
