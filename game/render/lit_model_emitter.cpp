// game/render/lit_model_emitter.cpp — the point-lit GT3/GT4 model emitters. See lit_model_emitter.h.
#include "lit_model_emitter.h"

#include "core.h"
#include "core/overrides/native_override_catalog.h"
#include "gte_registers.h"
#include "guest_abi.h"
#include "horizontal_visibility_cull.h"
#include "model_packet.h"
#include "trig.h"

#include <array>
#include <string_view>

namespace tomba2::render {
namespace {

using horizontal_cull::Visibility;

constexpr GuestFrameSpill kSpills[] = {{23, 0x34},
                                       {22, 0x30},
                                       {21, 0x2C},
                                       {20, 0x28},
                                       {31, 0x3C},
                                       {30, 0x38},
                                       {19, 0x24},
                                       {18, 0x20},
                                       {17, 0x1C},
                                       {16, 0x18}};
constexpr std::uint32_t kFrameBytes = 0x40u;
constexpr std::uint32_t kOtArgument = 0x44u;        // a1, spilled to the caller's argument area
constexpr std::uint32_t kIntensityArgument = 0x4Cu; // a3, likewise
constexpr std::uint32_t kSeedLocal = 0x10u;         // RGBC is loaded with this word's address, not the word
constexpr std::uint32_t kMaskLocal = 0x14u;         // GT4 only, once a record is drawn

constexpr std::uint32_t kGt3Seed = 0x34808080u;
constexpr std::uint32_t kGt4Seed = 0x3C808080u;

constexpr std::uint32_t kLight = 0x1F800160u; // s16 x, y, z
constexpr std::int32_t kMaxIntensity = 0x4000;
constexpr std::uint32_t kDeepenFlag = 8u;
constexpr std::int32_t kDeepenBias = 0x80;

// Twice the corner's distance to the light, capped.
std::int32_t lightIntensity(Core &core, std::uint32_t record, const ModelCorner &corner) {
  const std::int32_t length = Trig::vecLen(core.mem_r16s(record + corner.x) - core.mem_r16s(kLight + 0u),
                                           core.mem_r16s(record + corner.y) - core.mem_r16s(kLight + 2u),
                                           core.mem_r16s(record + corner.z) - core.mem_r16s(kLight + 4u));
  const std::int32_t intensity = length * 2;
  return intensity > kMaxIntensity ? kMaxIntensity : intensity;
}

void light(Core &core, const ModelShape &shape, const ModelPacket &packet, std::uint32_t fixedIntensity) {
  std::array<std::int32_t, 4> intensity{};
  for (int corner = 0; corner < shape.cornerCount; ++corner) {
    intensity[corner] = fixedIntensity != 0u ? static_cast<std::int32_t>(fixedIntensity)
                                             : lightIntensity(core, packet.record(), shape.corners[corner]);
  }
  for (int corner = 0; corner < shape.cornerCount; ++corner) {
    packet.shade(corner, intensity[corner]);
  }
}

// Depth, bucket, last UV, light and link once the record is on screen; false when it is dropped.
bool finish(
    Core &core, const ModelShape &shape, const ModelPacket &packet, LitModelEmitter::FlagBits bits, std::uint32_t sp) {
  const std::uint32_t flags = packet.flags();
  const bool hidesBeforeStaging = shape.list == ModelList::Gt4;
  const bool hides = bits.hideFlag != 0u && ModelPacket::hidden(core, flags, bits.hideFlag);
  if (hides && hidesBeforeStaging) {
    return false;
  }
  packet.stageDepths(kDepthStage);
  if (hides && !hidesBeforeStaging) {
    return false;
  }
  packet.depth(ModelPacket::litDepth(flags), kDepthStage);
  if (bits.deepen && (flags & kDeepenFlag) != 0u) {
    packet.biasBucket(kDeepenBias);
  }
  if (packet.bucket() < 0) {
    return false;
  }
  packet.storeLastUv();
  light(core, shape, packet, core.mem_r32(sp + kIntensityArgument));
  packet.link(core.mem_r32(sp + kOtArgument));
  return true;
}

void emitList(Core &core, const ModelShape &shape, const Visibility &visible, LitModelEmitter::FlagBits bits) {
  GuestFrame<kFrameBytes, 10> frame(&core, kSpills);
  const std::uint32_t sp = core.r[29];
  const bool quad = shape.list == ModelList::Gt4;
  core.mem_w32(sp + kOtArgument, core.r[5]);
  core.mem_w32(sp + kIntensityArgument, core.r[7]);
  core.mem_w32(sp + kSeedLocal, quad ? kGt4Seed : kGt3Seed);
  gte_write_data(gte::kRgbc, sp + kSeedLocal);
  if (core.r[6] != 0u && quad) {
    core.mem_w32(sp + kMaskLocal, kColourMask);
  }
  core.r[2] = emitModelList(core, shape, kOverlayStage, [&](const ModelPacket &packet) {
    const bool projected =
        quad ? packet.projectGt4(kColourCodeMask, kColourMask, visible) : packet.projectGt3(kColourMask, visible);
    return projected && finish(core, shape, packet, bits, sp);
  });
}

template <std::uint32_t Entry, bool Quad, LitModelEmitter::FlagBits Bits> void emitAt(Core *core) {
  const ModelObjectScope object(core->emission, Entry, core->r[4]);
  const Visibility visible = horizontal_cull::forDrawWindow(core);
  if constexpr (Quad) {
    LitModelEmitter::gt4(*core, visible, Bits);
  } else {
    LitModelEmitter::gt3(*core, visible, Bits);
  }
}

struct Copy {
  std::string_view image;
  std::uint32_t gt3;
  std::uint32_t gt4;
  psx::cpu::NativeFunction gt3Body;
  psx::cpu::NativeFunction gt4Body;
};

constexpr auto kRetail = LitModelEmitter::kRetailFlags;
constexpr auto kA01 = LitModelEmitter::kA01Flags;
constexpr auto kA06 = LitModelEmitter::kA06Flags;

constexpr Copy kCopies[] = {
    {"A01", 0x801316A8u, 0x80131BB0u, &emitAt<0x801316A8u, false, kA01>, &emitAt<0x80131BB0u, true, kA01>},
    {"A05", 0x8013544Cu, 0x8013590Cu, &emitAt<0x8013544Cu, false, kRetail>, &emitAt<0x8013590Cu, true, kRetail>},
    {"A06", 0x8013C0D8u, 0x8013C5B4u, &emitAt<0x8013C0D8u, false, kA06>, &emitAt<0x8013C5B4u, true, kA06>},
    {"A07", 0x8012CDF4u, 0x8012D2B4u, &emitAt<0x8012CDF4u, false, kRetail>, &emitAt<0x8012D2B4u, true, kRetail>},
    {"A08", 0x80129BACu, 0x8012A06Cu, &emitAt<0x80129BACu, false, kRetail>, &emitAt<0x8012A06Cu, true, kRetail>},
};

} // namespace

void LitModelEmitter::gt3(Core &core, const Visibility &visible, FlagBits flags) {
  emitList(core, kModelGt3, visible, flags);
}

void LitModelEmitter::gt4(Core &core, const Visibility &visible, FlagBits flags) {
  emitList(core, kModelGt4, visible, flags);
}

void LitModelEmitter::registerOverrides() {
  for (const Copy &copy : kCopies) {
    tomba::native::declareOverlayOverride(copy.image, copy.gt3, "LitModelEmitter::gt3", copy.gt3Body);
    tomba::native::declareOverlayOverride(copy.image, copy.gt4, "LitModelEmitter::gt4", copy.gt4Body);
  }
}

} // namespace tomba2::render
