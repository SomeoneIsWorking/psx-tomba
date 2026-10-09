// game/render/sway_model_emitter.cpp — A01's sway, scroll and cue emitters. See sway_model_emitter.h.
#include "sway_model_emitter.h"

#include "core.h"
#include "core/overrides/guest_jal.h"
#include "core/overrides/native_override_catalog.h"
#include "gte_registers.h"
#include "guest_abi.h"
#include "horizontal_visibility_cull.h"
#include "list_state_producer.h"
#include "model_packet.h"
#include "trig.h"

#include <array>
#include <memory>

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
constexpr std::uint32_t kScrollBytes = 3u;        // 0x801388EC..EE
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

std::int32_t
swayOffset(const psx::present::EmitMemory &memory, bool cosine, std::uint32_t returnAddress, std::int32_t angle) {
  Core &core = memory.core();
  std::int32_t value = 0;
  if (memory.hosted()) {
    // The guest's trig frames are guest stack; the render reads the same table through the callee's own port.
    Trig trig;
    trig.core = &core;
    value = cosine ? trig.rcos(angle) : trig.rsin(angle);
  } else {
    value = static_cast<std::int32_t>(tomba::guest::dispatchJalToReturn(
        core, cosine ? kRcos : kRsin, returnAddress, static_cast<std::uint32_t>(angle)));
  }
  return value >> kSwayShift;
}

void sway(const psx::present::EmitMemory &memory, const ModelPacket &packet, const SwayCalls &calls) {
  const std::uint32_t record = packet.record();
  for (int corner = 0; corner < 3; ++corner) {
    const ModelCorner &model = kModelGt3.corners[corner];
    const bool odd = (memory.mem_r16(record + model.z) & 1u) != 0u;
    const std::int32_t angle = memory.mem_r16s(kSwayPhase) + memory.mem_r16s(record + model.x);
    const std::int32_t offset =
        swayOffset(memory, true, odd ? calls.cosineOdd[corner] : calls.cosineEven[corner], angle);
    const std::uint32_t sx = packet.packet() + ModelPacket::kScreen[corner];
    const std::uint32_t moved = memory.mem_r16(sx) + static_cast<std::uint32_t>(odd ? offset : -offset);
    memory.mem_w16(sx, static_cast<std::uint16_t>(moved));
  }
  for (int corner = 0; corner < 3; ++corner) {
    const ModelCorner &model = kModelGt3.corners[corner];
    const std::int32_t angle = memory.mem_r16s(kSwayPhase) + memory.mem_r16s(record + model.z);
    const std::int32_t offset = swayOffset(memory, false, calls.sine[corner], angle);
    const std::uint32_t sy = packet.packet() + ModelPacket::kScreen[corner] + 2u;
    memory.mem_w16(sy, static_cast<std::uint16_t>(memory.mem_r16(sy) + static_cast<std::uint32_t>(offset)));
  }
}

void scrollUv(const psx::present::EmitMemory &memory, const ModelShape &shape, const ModelPacket &packet) {
  for (int corner = 0; corner < shape.cornerCount; ++corner) {
    const std::uint32_t uv = packet.packet() + ModelPacket::kUv[corner];
    memory.mem_w8(uv, static_cast<std::uint8_t>(memory.mem_r8(uv) + memory.mem_r8(kScrollU)));
    memory.mem_w8(uv + 1u, static_cast<std::uint8_t>(memory.mem_r8(uv + 1u) + memory.mem_r8(kScrollV)));
  }
}

std::int32_t stagedDepth(const psx::present::EmitMemory &memory, int corner) {
  return static_cast<std::int32_t>(memory.mem_r32(kDepthStage + static_cast<std::uint32_t>(corner) * 4u));
}

// IR0 = SZ / 4 plus the next scratchpad LCG value's top 10 bits.
void cueRandom(const psx::present::EmitMemory &memory, const ModelPacket &packet, int corner) {
  Core &core = memory.core();
  guest_mult(
      &core, static_cast<std::int32_t>(memory.mem_r32(kRandomSeed)), static_cast<std::int32_t>(kRandomMultiplier));
  const std::uint32_t seed = core.lo + kRandomIncrement;
  memory.mem_w32(kRandomSeed, seed);
  packet.shade(corner, stagedDepth(memory, corner) / 4 + static_cast<std::int32_t>(seed >> 22));
}

// Every cue body cues its corners the same way; false when the record is hidden.
bool cue(const psx::present::EmitMemory &memory,
         const ModelShape &shape,
         const ModelPacket &packet,
         std::uint32_t flags) {
  if ((flags & kHideFlag) != 0u) {
    return !ModelPacket::hidden(memory, flags);
  }
  for (int corner = 0; corner < shape.cornerCount; ++corner) {
    if ((flags & kCueRandomFlag) != 0u) {
      cueRandom(memory, packet, corner);
    } else {
      packet.cueByDepth(corner, kDepthStage);
    }
  }
  return true;
}

// Projection through the bucket, the steps all four bodies share; false when the record is dropped.
bool place(const psx::present::EmitMemory &memory,
           const ModelShape &shape,
           const ModelPacket &packet,
           const Visibility &visible,
           bool hides) {
  const bool projected = shape.list == ModelList::Gt4 ? packet.projectGt4(kColourCodeMask, kColourMask, visible)
                                                      : packet.projectGt3(kColourMask, visible);
  if (!projected) {
    return false;
  }
  const std::uint32_t flags = packet.flags();
  const bool hidesBeforeStaging = shape.list == ModelList::Gt4;
  if (hides && hidesBeforeStaging && ModelPacket::hidden(memory, flags)) {
    return false;
  }
  packet.stageDepths(kDepthStage);
  if (hides && !hidesBeforeStaging && ModelPacket::hidden(memory, flags)) {
    return false;
  }
  packet.depth(ModelPacket::depthOf(flags, 3u), kDepthStage);
  if (packet.bucket() < 0) {
    return false;
  }
  packet.storeLastUv();
  return true;
}

std::uint32_t swayGt3Body(const psx::present::EmitMemory &memory, const ListCall &call, const Visibility &visible) {
  const std::uint32_t sp = call.sp;
  memory.mem_w32(sp + kSwayGt3OtArgument, call.ot);
  return emitModelList(memory, call, kModelGt3, kOverlayStage, [&](const ModelPacket &packet) {
    if (!place(memory, kModelGt3, packet, visible, true)) {
      return false;
    }
    const std::uint32_t flags = packet.flags();
    if ((flags & kSwayFlag) != 0u) {
      sway(memory, packet, kSwayGt3Calls);
      packet.biasBucket(kSwayGt3Bias);
    } else if ((flags & kScrollFlag) != 0u) {
      scrollUv(memory, kModelGt3, packet);
    }
    packet.link(memory.mem_r32(sp + kSwayGt3OtArgument));
    return true;
  });
}

std::uint32_t scrollGt4Body(const psx::present::EmitMemory &memory, const ListCall &call, const Visibility &visible) {
  return emitModelList(memory, call, kModelGt4, kOverlayStage, [&](const ModelPacket &packet) {
    if (!place(memory, kModelGt4, packet, visible, true)) {
      return false;
    }
    if ((packet.flags() & kScrollFlag) != 0u) {
      scrollUv(memory, kModelGt4, packet);
    }
    packet.link(call.ot);
    return true;
  });
}

std::uint32_t cueGt3Body(const psx::present::EmitMemory &memory, const ListCall &call, const Visibility &visible) {
  const std::uint32_t sp = call.sp;
  memory.mem_w32(sp + kCueGt3OtArgument, call.ot);
  if (call.count != 0u) {
    memory.mem_w32(sp + kCueGt3MultiplierLocal, kRandomMultiplier);
  }
  return emitModelList(memory, call, kModelGt3, kOverlayStage, [&](const ModelPacket &packet) {
    if (!place(memory, kModelGt3, packet, visible, false)) {
      return false;
    }
    const std::uint32_t flags = packet.flags();
    if ((flags & kSwayFlag) != 0u) {
      sway(memory, packet, kCueGt3Calls);
      packet.biasBucket(kCueGt3Bias);
    } else if (!cue(memory, kModelGt3, packet, flags)) {
      return false;
    }
    if ((flags & kScrollFlag) != 0u) {
      scrollUv(memory, kModelGt3, packet);
    }
    packet.link(memory.mem_r32(sp + kCueGt3OtArgument));
    return true;
  });
}

std::uint32_t cueGt4Body(const psx::present::EmitMemory &memory, const ListCall &call, const Visibility &visible) {
  gte_write_data(psx::gte::kRgbc, kCueGt4Rgbc);
  return emitModelList(memory, call, kModelGt4, kOverlayStage, [&](const ModelPacket &packet) {
    if (!place(memory, kModelGt4, packet, visible, false)) {
      return false;
    }
    const std::uint32_t flags = packet.flags();
    if ((flags & kCueCornersFlag) != 0u) {
      for (int corner = 0; corner < kModelGt4.cornerCount; ++corner) {
        if ((flags & (kFirstCornerCueFlag << corner)) != 0u) {
          cueRandom(memory, packet, corner);
        }
      }
    } else {
      if (!cue(memory, kModelGt4, packet, flags)) {
        return false;
      }
      if ((flags & kScrollFlag) != 0u) {
        scrollUv(memory, kModelGt4, packet);
      }
    }
    packet.link(call.ot);
    return true;
  });
}

bool isQuad(SwayModelEmitter::Kind kind) {
  return kind == SwayModelEmitter::Kind::ScrollGt4 || kind == SwayModelEmitter::Kind::CueGt4;
}

// The guest call inside its frame: the call and the words the body reads are saved, then the body runs.
void runGuest(Core &core, SwayModelEmitter::Kind kind, const Visibility &visible) {
  const ListCall call = ListCall::fromRegisters(core);
  ListJobWriter job = modelListJob(core, static_cast<std::uint32_t>(kind), call, isQuad(kind) ? kModelGt4 : kModelGt3);
  job.input(core, kSwayPhase, sizeof(std::uint16_t), InputBlend::Wrapped, kAngleTurn);
  job.input(core, kScrollU, kScrollBytes);
  job.input(core, kHideWord, sizeof(std::uint32_t));
  job.input(core, kRandomSeed, sizeof(std::uint32_t));
  core.r[2] = SwayModelEmitter::emit(core, call, visible, kind);
  job.save(core);
}

class SwayStateProducer final : public ListStateProducer {
public:
  using ListStateProducer::ListStateProducer;

protected:
  void emit(const psx::present::EmitMemory &memory, std::uint32_t variant, const ListCall &call) const override {
    SwayModelEmitter::emit(
        memory, call, horizontal_cull::forDrawWindow(&core()), static_cast<SwayModelEmitter::Kind>(variant));
  }
};

} // namespace

std::uint32_t SwayModelEmitter::emit(const psx::present::EmitMemory &memory,
                                     const ListCall &call,
                                     const Visibility &visible,
                                     Kind kind) {
  switch (kind) {
  case Kind::SwayGt3:
    return swayGt3Body(memory, call, visible);
  case Kind::ScrollGt4:
    return scrollGt4Body(memory, call, visible);
  case Kind::CueGt3:
    return cueGt3Body(memory, call, visible);
  case Kind::CueGt4:
    break;
  }
  return cueGt4Body(memory, call, visible);
}

void SwayModelEmitter::swayGt3(Core &core, const Visibility &visible) {
  GuestFrame<kSwayGt3Frame, 10> frame(&core, kSwayGt3Spills);
  runGuest(core, Kind::SwayGt3, visible);
}

void SwayModelEmitter::scrollGt4(Core &core, const Visibility &visible) {
  GuestFrame<kScrollGt4Frame, 5> frame(&core, kScrollGt4Spills);
  runGuest(core, Kind::ScrollGt4, visible);
}

void SwayModelEmitter::cueGt3(Core &core, const Visibility &visible) {
  GuestFrame<kCueGt3Frame, 10> frame(&core, kCueGt3Spills);
  runGuest(core, Kind::CueGt3, visible);
}

void SwayModelEmitter::cueGt4(Core &core, const Visibility &visible) {
  GuestFrame<kCueGt4Frame, 8> frame(&core, kCueGt4Spills);
  runGuest(core, Kind::CueGt4, visible);
}

namespace {

template <std::uint32_t Entry, void (*Body)(Core &, const Visibility &)> void emitAt(Core *core) {
  const EmitterObject object(core->emission, Entry, core->r[4]);
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

void SwayModelEmitter::registerStateRenders(Core &core) {
  for (const std::uint32_t entry : {kSwayGt3, kScrollGt4, kCueGt3, kCueGt4}) {
    core.stateProducers.install(entry, std::make_unique<SwayStateProducer>(core));
  }
}

} // namespace tomba2::render
