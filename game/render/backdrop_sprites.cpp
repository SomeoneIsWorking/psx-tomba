// game/render/backdrop_sprites.cpp — A0E FUN_80111E14. See backdrop_sprites.h.
#include "backdrop_sprites.h"

#include "core.h"
#include "core/overrides/native_override_catalog.h"
#include "fx_sprite_publish.h"
#include "gte_registers.h"
#include "guest_abi.h"
#include "guest_ordering_table.h"
#include "list_job.h"
#include "list_state_producer.h"
#include "object_draw.h"
#include "scaled_sprites.h"

#include <cstdint>
#include <memory>

namespace tomba::render {
namespace {

constexpr std::uint32_t kEntry = 0x80111E14u;
constexpr GuestFrameSpill kSpills[] = {
    {31, 0x34}, {30, 0x30}, {23, 0x2C}, {22, 0x28}, {21, 0x24}, {20, 0x20}, {19, 0x1C}, {18, 0x18}, {17, 0x14},
    {16, 0x10}};

constexpr std::uint32_t kReseed = 0x8011CE60u;  // u32; non-zero seeds the anchors on the next call
constexpr std::uint32_t kAnchors = 0x80126F4Cu; // 8 x {s16 x, y, z, w}
constexpr std::uint32_t kAnchorCount = 8u;
constexpr std::uint32_t kAnchorBytes = 8u;
constexpr std::uint32_t kRecords = 0x8011B8ECu;
constexpr std::uint32_t kTextureClut = 0x8011B8E8u;
constexpr std::uint32_t kSeed = 0xFEDCB7u;
constexpr std::uint16_t kHeightBase = 2000;
constexpr std::uint32_t kDqa = 6u;
constexpr std::int32_t kBucket = 0x7FE;
constexpr std::int32_t kScaleShift = 4;

std::uint32_t lcgStep(std::uint32_t value) {
  return value * 5u + 1u;
}

std::uint16_t high(std::uint32_t value, int shift) {
  return static_cast<std::uint16_t>(static_cast<std::int32_t>(value) >> shift);
}

void seedAnchors(Core &core) {
  std::uint32_t lcg = kSeed;
  for (std::uint32_t anchor = 0; anchor < kAnchorCount; ++anchor) {
    const std::uint32_t at = kAnchors + anchor * kAnchorBytes;
    const std::uint32_t x = lcg;
    const std::uint32_t z = lcgStep(x);
    const std::uint32_t y = lcgStep(z);
    const std::uint32_t w = lcgStep(y);
    lcg = lcgStep(w);
    core.mem_w16(at, high(x, 15) & 0x7FFFu);
    core.mem_w16(at + 2u, static_cast<std::uint16_t>((high(y, 15) & 0x7FFu) + kHeightBase));
    core.mem_w16(at + 4u, high(z, 15) & 0x7FFFu);
    core.mem_w16(at + 6u, high(w, 14));
  }
  core.mem_w32(kReseed, 0u);
}

// Each anchor under the loaded camera; one that projects in front of it stamps the records there.
void stampAnchors(const psx::present::EmitMemory &memory, std::uint32_t ot) {
  const FxSpritePublish publish{memory};
  psx::cpu::Gte &gte = memory.core().gte();
  for (std::uint32_t anchor = 0; anchor < kAnchorCount; ++anchor) {
    const std::uint32_t at = kAnchors + anchor * kAnchorBytes;
    gte.writeData(psx::gte::kVxy0, memory.mem_r32(at));
    gte.writeData(psx::gte::kVz0, memory.mem_r32(at + 4u));
    gte.execute(psx::gte::kRtps);
    publish.setOtKey(static_cast<std::int32_t>(gte.readControl(psx::gte::kFlag)));
    if (publish.otKey() < 0) {
      continue;
    }
    publish.setOtKey(static_cast<std::int32_t>(gte.readData(psx::gte::kSz3)));
    if (publish.otKey() <= 0) {
      continue;
    }
    memory.storeVertex(fxpublish::kScreenXY, gte.projected(2));
    const auto scale = static_cast<std::int32_t>(gte.readData(psx::gte::kMac0)) << kScaleShift;
    publish.setOtKey(kBucket);
    publish.setDepthCue(fxpublish::kDepthCueOff);
    publish.setScaleX(scale);
    publish.setScaleY(scale);
    ScaledSprites::stamp(memory, {.records = kRecords, .textureClut = memory.mem_r32(kTextureClut), .ot = ot});
  }
}

class BackdropSpritesRender final : public ListStateProducer {
public:
  using ListStateProducer::ListStateProducer;

protected:
  void emit(const psx::present::EmitMemory &memory, std::uint32_t, const ListCall &call) const override {
    stampAnchors(memory, call.ot);
  }
};

} // namespace

void BackdropSprites::draw(Core *c) {
  GuestFrame<0x38, 10> frame(c, kSpills);
  if (c->mem_r32(kReseed) != 0u) {
    seedAnchors(*c);
  }
  const psx::present::EmitMemory memory(*c);
  FxSpritePublish{memory}.loadSceneCamera(kDqa);
  const std::uint32_t ot = OrderingTable::active(memory).base();
  const ListCall call{.list = kAnchors, .ot = ot, .count = 0, .a3 = 0, .sp = c->r[psx::cpu::kSp], .a1 = 0};
  const EmitterObject object(*c, kEntry, OwnObject{kAnchors});
  const std::uint32_t recordBytes = ScaledSprites::recordCount(memory, kRecords) * ScaledSprites::kRecordBytes;
  ListJobWriter job(
      *c, 0u, call, kAnchorCount * ScaledSprites::recordCount(memory, kRecords) * ScaledSprites::kPacketBytes);
  job.input(*c, kAnchors, kAnchorCount * kAnchorBytes);
  job.input(*c, kTextureClut, sizeof(std::uint32_t) + recordBytes);
  stampAnchors(memory, ot);
  job.save(*c);
}

void BackdropSprites::registerOverrides(native::NativeOverrideCatalog &natives) {
  natives.declareOverlay("A0E", kEntry, "&BackdropSprites::draw", &BackdropSprites::draw);
}

void BackdropSprites::registerStateRenders(psx::present::StateProducerTable &renders, Core &core) {
  renders.install(kEntry, std::make_unique<BackdropSpritesRender>(core));
}

} // namespace tomba::render
