// game/render/sway_model_emitter.cpp — A01's sway, scroll and cue emitters. See sway_model_emitter.h.
#include "sway_model_emitter.h"

#include "core.h"
#include "core/overrides/guest_jal.h"
#include "core/overrides/native_override_catalog.h"
#include "gte_registers.h"
#include "guest_abi.h"
#include "horizontal_visibility_cull.h"
#include "model_packet.h"

#include <array>

namespace tomba2::render {
namespace {

using horizontal_cull::Visibility;

constexpr std::uint32_t kSwayGt3 = 0x80130838u;
constexpr std::uint32_t kScrollGt4 = 0x80130D9Cu;
constexpr std::uint32_t kCueGt3 = 0x8012F8D8u;
constexpr std::uint32_t kCueGt4 = 0x8013000Cu;

constexpr GuestFrameSpill kSwayGt3Spills[] = {{22, 0x28},
                                              {21, 0x24},
                                              {18, 0x18},
                                              {19, 0x1C},
                                              {30, 0x30},
                                              {31, 0x34},
                                              {23, 0x2C},
                                              {20, 0x20},
                                              {17, 0x14},
                                              {16, 0x10}};
constexpr std::uint32_t kSwayGt3Frame = 0x38u;
constexpr GuestFrameSpill kScrollGt4Spills[] = {{19, 0xC}, {20, 0x10}, {18, 8}, {17, 4}, {16, 0}};
constexpr std::uint32_t kScrollGt4Frame = 0x18u;
constexpr GuestFrameSpill kCueGt3Spills[] = {{23, 0x34},
                                             {22, 0x30},
                                             {19, 0x24},
                                             {18, 0x20},
                                             {31, 0x3C},
                                             {30, 0x38},
                                             {21, 0x2C},
                                             {20, 0x28},
                                             {17, 0x1C},
                                             {16, 0x18}};
constexpr std::uint32_t kCueGt3Frame = 0x40u;
constexpr GuestFrameSpill kCueGt4Spills[] = {
    {22, 0x18}, {23, 0x1C}, {21, 0x14}, {20, 0x10}, {19, 0xC}, {18, 8}, {17, 4}, {16, 0}};
constexpr std::uint32_t kCueGt4Frame = 0x20u;

constexpr std::uint32_t kSwayGt3OtArgument = 0x3Cu; // a1, spilled to the caller's argument area
constexpr std::uint32_t kCueGt3OtArgument = 0x44u;
constexpr std::uint32_t kCueGt3MultiplierLocal = 0x10u;
constexpr std::uint32_t kCueGt4Rgbc = 0x3C808080u;

constexpr std::uint32_t kSwayPhase = 0x80139004u; // s16
constexpr std::uint32_t kScrollU = 0x801388ECu;   // u8
constexpr std::uint32_t kScrollV = 0x801388EEu;   // u8
constexpr std::uint32_t kRandomSeed = 0x1F800080u;
constexpr std::uint32_t kRandomMultiplier = 0x41C64E6Du;
constexpr std::uint32_t kRandomIncrement = 0x3039u;
constexpr std::uint32_t kRcos = 0x80083F50u;
constexpr std::uint32_t kRsin = 0x80083E80u;

constexpr std::uint32_t kCueRandomFlag = 4u;
constexpr std::uint32_t kSwayFlag = 8u;
constexpr std::uint32_t kScrollFlag = 0x10u;
constexpr std::uint32_t kHideFlag = 0x40u;
constexpr std::uint32_t kCueCornersFlag = 0x80u;
constexpr std::uint32_t kFirstCornerCueFlag = 8u; // GT4 bit 7: bits 3..6 pick the cued corners
constexpr std::int32_t kSwayShift = 9;
constexpr std::int32_t kSwayGt3Bias = 0x80;
constexpr std::int32_t kCueGt3Bias = 0x96;

// Guest return addresses of the sway's trig calls (rsin's frame stores its ra below the caller's sp).
struct SwayCalls {
  std::array<std::uint32_t, 3> cosineOdd;
  std::array<std::uint32_t, 3> cosineEven;
  std::array<std::uint32_t, 3> sine;
};
constexpr SwayCalls kSwayGt3Calls{{0x80130B48u, 0x80130B9Cu, 0x80130BF0u},
                                  {0x80130B68u, 0x80130BBCu, 0x80130C10u},
                                  {0x80130C30u, 0x80130C50u, 0x80130C70u}};
constexpr SwayCalls kCueGt3Calls{{0x8012FBDCu, 0x8012FC30u, 0x8012FC84u},
                                 {0x8012FBFCu, 0x8012FC50u, 0x8012FCA4u},
                                 {0x8012FCC4u, 0x8012FCE4u, 0x8012FD04u}};

std::int32_t swayOffset(Core &core, std::uint32_t function, std::uint32_t returnAddress, std::int32_t angle) {
  const auto value = static_cast<std::int32_t>(
      tomba::guest::dispatchJalToReturn(core, function, returnAddress, static_cast<std::uint32_t>(angle)));
  return value >> kSwayShift;
}

void sway(Core &core, const ModelPacket &packet, const SwayCalls &calls) {
  const std::uint32_t record = packet.record();
  for (int corner = 0; corner < 3; ++corner) {
    const ModelCorner &model = kModelGt3.corners[corner];
    const bool odd = (core.mem_r16(record + model.z) & 1u) != 0u;
    const std::int32_t angle = core.mem_r16s(kSwayPhase) + core.mem_r16s(record + model.x);
    const std::int32_t offset =
        swayOffset(core, kRcos, odd ? calls.cosineOdd[corner] : calls.cosineEven[corner], angle);
    const std::uint32_t sx = packet.packet() + ModelPacket::kScreen[corner];
    const std::uint32_t moved = core.mem_r16(sx) + static_cast<std::uint32_t>(odd ? offset : -offset);
    core.mem_w16(sx, static_cast<std::uint16_t>(moved));
  }
  for (int corner = 0; corner < 3; ++corner) {
    const ModelCorner &model = kModelGt3.corners[corner];
    const std::int32_t angle = core.mem_r16s(kSwayPhase) + core.mem_r16s(record + model.z);
    const std::int32_t offset = swayOffset(core, kRsin, calls.sine[corner], angle);
    const std::uint32_t sy = packet.packet() + ModelPacket::kScreen[corner] + 2u;
    core.mem_w16(sy, static_cast<std::uint16_t>(core.mem_r16(sy) + static_cast<std::uint32_t>(offset)));
  }
}

void scrollUv(Core &core, const ModelShape &shape, const ModelPacket &packet) {
  for (int corner = 0; corner < shape.cornerCount; ++corner) {
    const std::uint32_t uv = packet.packet() + ModelPacket::kUv[corner];
    core.mem_w8(uv, static_cast<std::uint8_t>(core.mem_r8(uv) + core.mem_r8(kScrollU)));
    core.mem_w8(uv + 1u, static_cast<std::uint8_t>(core.mem_r8(uv + 1u) + core.mem_r8(kScrollV)));
  }
}

std::int32_t stagedDepth(Core &core, int corner) {
  return static_cast<std::int32_t>(core.mem_r32(kDepthStage + static_cast<std::uint32_t>(corner) * 4u));
}

// IR0 = SZ / 4 plus the next scratchpad LCG value's top 10 bits.
void cueRandom(Core &core, const ModelPacket &packet, int corner) {
  guest_mult(&core, static_cast<std::int32_t>(core.mem_r32(kRandomSeed)), static_cast<std::int32_t>(kRandomMultiplier));
  const std::uint32_t seed = core.lo + kRandomIncrement;
  core.mem_w32(kRandomSeed, seed);
  packet.shade(corner, stagedDepth(core, corner) / 4 + static_cast<std::int32_t>(seed >> 22));
}

// Every cue body cues its corners the same way; false when the record is hidden.
bool cue(Core &core, const ModelShape &shape, const ModelPacket &packet, std::uint32_t flags) {
  if ((flags & kHideFlag) != 0u) {
    return !ModelPacket::hidden(core, flags);
  }
  for (int corner = 0; corner < shape.cornerCount; ++corner) {
    if ((flags & kCueRandomFlag) != 0u) {
      cueRandom(core, packet, corner);
    } else {
      packet.cueByDepth(corner, kDepthStage);
    }
  }
  return true;
}

// Projection through the bucket, the steps all four bodies share; false when the record is dropped.
bool place(Core &core, const ModelShape &shape, const ModelPacket &packet, const Visibility &visible, bool hides) {
  const bool projected = shape.list == ModelList::Gt4 ? packet.projectGt4(kColourCodeMask, kColourMask, visible)
                                                      : packet.projectGt3(kColourMask, visible);
  if (!projected) {
    return false;
  }
  const std::uint32_t flags = packet.flags();
  const bool hidesBeforeStaging = shape.list == ModelList::Gt4;
  if (hides && hidesBeforeStaging && ModelPacket::hidden(core, flags)) {
    return false;
  }
  packet.stageDepths(kDepthStage);
  if (hides && !hidesBeforeStaging && ModelPacket::hidden(core, flags)) {
    return false;
  }
  packet.depth(ModelPacket::depthOf(flags, 3u), kDepthStage);
  if (packet.bucket() < 0) {
    return false;
  }
  packet.storeLastUv();
  return true;
}

} // namespace

void SwayModelEmitter::swayGt3(Core &core, const Visibility &visible) {
  GuestFrame<kSwayGt3Frame, 10> frame(&core, kSwayGt3Spills);
  const std::uint32_t sp = core.r[29];
  core.mem_w32(sp + kSwayGt3OtArgument, core.r[5]);
  core.r[2] = emitModelList(core, kModelGt3, kOverlayStage, [&](const ModelPacket &packet) {
    if (!place(core, kModelGt3, packet, visible, true)) {
      return false;
    }
    const std::uint32_t flags = packet.flags();
    if ((flags & kSwayFlag) != 0u) {
      sway(core, packet, kSwayGt3Calls);
      packet.biasBucket(kSwayGt3Bias);
    } else if ((flags & kScrollFlag) != 0u) {
      scrollUv(core, kModelGt3, packet);
    }
    packet.link(core.mem_r32(sp + kSwayGt3OtArgument));
    return true;
  });
}

void SwayModelEmitter::scrollGt4(Core &core, const Visibility &visible) {
  GuestFrame<kScrollGt4Frame, 5> frame(&core, kScrollGt4Spills);
  const std::uint32_t orderingTable = core.r[5];
  core.r[2] = emitModelList(core, kModelGt4, kOverlayStage, [&](const ModelPacket &packet) {
    if (!place(core, kModelGt4, packet, visible, true)) {
      return false;
    }
    if ((packet.flags() & kScrollFlag) != 0u) {
      scrollUv(core, kModelGt4, packet);
    }
    packet.link(orderingTable);
    return true;
  });
}

void SwayModelEmitter::cueGt3(Core &core, const Visibility &visible) {
  GuestFrame<kCueGt3Frame, 10> frame(&core, kCueGt3Spills);
  const std::uint32_t sp = core.r[29];
  core.mem_w32(sp + kCueGt3OtArgument, core.r[5]);
  if (core.r[6] != 0u) {
    core.mem_w32(sp + kCueGt3MultiplierLocal, kRandomMultiplier);
  }
  core.r[2] = emitModelList(core, kModelGt3, kOverlayStage, [&](const ModelPacket &packet) {
    if (!place(core, kModelGt3, packet, visible, false)) {
      return false;
    }
    const std::uint32_t flags = packet.flags();
    if ((flags & kSwayFlag) != 0u) {
      sway(core, packet, kCueGt3Calls);
      packet.biasBucket(kCueGt3Bias);
    } else if (!cue(core, kModelGt3, packet, flags)) {
      return false;
    }
    if ((flags & kScrollFlag) != 0u) {
      scrollUv(core, kModelGt3, packet);
    }
    packet.link(core.mem_r32(sp + kCueGt3OtArgument));
    return true;
  });
}

void SwayModelEmitter::cueGt4(Core &core, const Visibility &visible) {
  GuestFrame<kCueGt4Frame, 8> frame(&core, kCueGt4Spills);
  gte_write_data(gte::kRgbc, kCueGt4Rgbc);
  const std::uint32_t orderingTable = core.r[5];
  core.r[2] = emitModelList(core, kModelGt4, kOverlayStage, [&](const ModelPacket &packet) {
    if (!place(core, kModelGt4, packet, visible, false)) {
      return false;
    }
    const std::uint32_t flags = packet.flags();
    if ((flags & kCueCornersFlag) != 0u) {
      for (int corner = 0; corner < kModelGt4.cornerCount; ++corner) {
        if ((flags & (kFirstCornerCueFlag << corner)) != 0u) {
          cueRandom(core, packet, corner);
        }
      }
    } else {
      if (!cue(core, kModelGt4, packet, flags)) {
        return false;
      }
      if ((flags & kScrollFlag) != 0u) {
        scrollUv(core, kModelGt4, packet);
      }
    }
    packet.link(orderingTable);
    return true;
  });
}

namespace {

template <std::uint32_t Entry, void (*Body)(Core &, const Visibility &)> void emitAt(Core *core) {
  const ModelObjectScope object(core->emission, Entry, core->r[4]);
  Body(*core, horizontal_cull::forDrawWindow(core));
}

} // namespace

void SwayModelEmitter::registerOverrides() {
  tomba::native::declareOverlayOverride(
      "A01", kSwayGt3, "SwayModelEmitter::swayGt3", &emitAt<kSwayGt3, &SwayModelEmitter::swayGt3>);
  tomba::native::declareOverlayOverride(
      "A01", kScrollGt4, "SwayModelEmitter::scrollGt4", &emitAt<kScrollGt4, &SwayModelEmitter::scrollGt4>);
  tomba::native::declareOverlayOverride(
      "A01", kCueGt3, "SwayModelEmitter::cueGt3", &emitAt<kCueGt3, &SwayModelEmitter::cueGt3>);
  tomba::native::declareOverlayOverride(
      "A01", kCueGt4, "SwayModelEmitter::cueGt4", &emitAt<kCueGt4, &SwayModelEmitter::cueGt4>);
}

} // namespace tomba2::render
