// game/render/unlit_model_emitter.cpp — the unlit model emitters. See unlit_model_emitter.h.
#include "unlit_model_emitter.h"

#include "core.h"
#include "core/overrides/native_override_catalog.h"
#include "horizontal_visibility_cull.h"
#include "list_state_producer.h"

#include <memory>
#include <string_view>

namespace tomba2::render {
namespace {

using horizontal_cull::Visibility;

constexpr std::uint32_t kScrollFlag = 8u;

ModelDepth depthOf(const UnlitModelEmitter::DepthRule &rule, std::uint32_t flags) {
  switch (rule.code) {
  case UnlitModelEmitter::ModeCode::ZeroAverage:
    return ModelPacket::flaggedDepth(flags);
  case UnlitModelEmitter::ModeCode::LowBits:
    return ModelPacket::litDepth(flags);
  case UnlitModelEmitter::ModeCode::OneFarTwoNear:
    break;
  }
  return ModelPacket::depthOf(flags, rule.modeMask);
}

template <std::uint32_t Entry, bool Quad, const UnlitModelEmitter::Variant &Data> void emitAt(Core *core) {
  const ListCall call = ListCall::fromRegisters(*core);
  const EmitterObject object(core->emission, Entry, call.list);
  const Visibility visible = horizontal_cull::forDrawWindow(core);
  const ModelShape &shape = Quad ? kModelGt4 : kModelGt3;
  ListJobWriter job(*core, Quad ? 1u : 0u, call, call.count * shape.packetBytes);
  job.input(*core, call.list, call.count * shape.recordBytes);
  if (Data.hideFlag != 0u) {
    job.input(*core, kHideWord, sizeof(std::uint32_t));
  }
  if (Data.uScroll != 0u) {
    job.input(*core, Data.uScroll, sizeof(std::uint16_t));
  }
  core->r[2] = UnlitModelEmitter::emit(*core, call, visible, Data, Quad);
  job.save(*core);
}

// The render of one emitter entry: its body over host memory.
class UnlitStateProducer final : public ListStateProducer {
public:
  UnlitStateProducer(Core &core, const UnlitModelEmitter::Variant &variant, bool quad)
      : ListStateProducer(core), mVariant(variant), mQuad(quad) {}

protected:
  void emit(const psx::present::EmitMemory &memory, std::uint32_t, const ListCall &call) const override {
    UnlitModelEmitter::emit(memory, call, horizontal_cull::forDrawWindow(&core()), mVariant, mQuad);
  }

private:
  const UnlitModelEmitter::Variant &mVariant;
  bool mQuad;
};

struct Copy {
  std::string_view image; // empty for the resident executable
  std::uint32_t gt3;
  std::uint32_t gt4;
  psx::cpu::NativeFunction gt3Body;
  psx::cpu::NativeFunction gt4Body;
  const UnlitModelEmitter::Variant *variant;
};

constexpr const auto &kResident = UnlitModelEmitter::kResident;
constexpr const auto &kSop = UnlitModelEmitter::kSop;
constexpr const auto &kA01Cue = UnlitModelEmitter::kA01Cue;
constexpr const auto &kFlagged = UnlitModelEmitter::kFlagged;
constexpr const auto &kSopNear = UnlitModelEmitter::kSopNear;
constexpr const auto &kSopNearWide = UnlitModelEmitter::kSopNearWide;
constexpr const auto &kSopNearLifted = UnlitModelEmitter::kSopNearLifted;
constexpr const auto &kA06 = UnlitModelEmitter::kA06;
constexpr const auto &kA06Scroll = UnlitModelEmitter::kA06Scroll;

constexpr Copy kCopies[] = {
    {{},
     0x8007FDB0u,
     0x8008007Cu,
     &emitAt<0x8007FDB0u, false, kResident>,
     &emitAt<0x8008007Cu, true, kResident>,
     &kResident},
    {"SOP", 0x801099B4u, 0x80109C80u, &emitAt<0x801099B4u, false, kSop>, &emitAt<0x80109C80u, true, kSop>, &kSop},
    {"A0B", 0x80112A24u, 0x80112CF0u, &emitAt<0x80112A24u, false, kSop>, &emitAt<0x80112CF0u, true, kSop>, &kSop},
    {"A0C", 0x80113788u, 0x80113A54u, &emitAt<0x80113788u, false, kSop>, &emitAt<0x80113A54u, true, kSop>, &kSop},
    {"A01",
     0x80132690u,
     0x801329C4u,
     &emitAt<0x80132690u, false, kA01Cue>,
     &emitAt<0x801329C4u, true, kA01Cue>,
     &kA01Cue},
    {"A0A",
     0x801103F4u,
     0x80110698u,
     &emitAt<0x801103F4u, false, kFlagged>,
     &emitAt<0x80110698u, true, kFlagged>,
     &kFlagged},
    {"A0B",
     0x80113150u,
     0x801133F4u,
     &emitAt<0x80113150u, false, kFlagged>,
     &emitAt<0x801133F4u, true, kFlagged>,
     &kFlagged},
    {"A0C",
     0x80113EB4u,
     0x80114158u,
     &emitAt<0x80113EB4u, false, kFlagged>,
     &emitAt<0x80114158u, true, kFlagged>,
     &kFlagged},
    {"A0D",
     0x80113748u,
     0x801139ECu,
     &emitAt<0x80113748u, false, kFlagged>,
     &emitAt<0x801139ECu, true, kFlagged>,
     &kFlagged},
    {"A0E",
     0x80114458u,
     0x801146FCu,
     &emitAt<0x80114458u, false, kFlagged>,
     &emitAt<0x801146FCu, true, kFlagged>,
     &kFlagged},
    {"A0F",
     0x801157CCu,
     0x80115A70u,
     &emitAt<0x801157CCu, false, kFlagged>,
     &emitAt<0x80115A70u, true, kFlagged>,
     &kFlagged},
    {"A0H",
     0x8010B6BCu,
     0x8010B960u,
     &emitAt<0x8010B6BCu, false, kFlagged>,
     &emitAt<0x8010B960u, true, kFlagged>,
     &kFlagged},
    {"A0I",
     0x8010BB40u,
     0x8010BDE4u,
     &emitAt<0x8010BB40u, false, kFlagged>,
     &emitAt<0x8010BDE4u, true, kFlagged>,
     &kFlagged},
    {"A0J",
     0x8010AB20u,
     0x8010ADC4u,
     &emitAt<0x8010AB20u, false, kFlagged>,
     &emitAt<0x8010ADC4u, true, kFlagged>,
     &kFlagged},
    {"A0G",
     0x8010BC40u,
     0x8010BF28u,
     &emitAt<0x8010BC40u, false, kSopNear>,
     &emitAt<0x8010BF28u, true, kSopNear>,
     &kSopNear},
    {"A0I",
     0x8010B3DCu,
     0x8010B6C4u,
     &emitAt<0x8010B3DCu, false, kSopNear>,
     &emitAt<0x8010B6C4u, true, kSopNear>,
     &kSopNear},
    {"A0H",
     0x8010AF58u,
     0x8010B240u,
     &emitAt<0x8010AF58u, false, kSopNearWide>,
     &emitAt<0x8010B240u, true, kSopNearWide>,
     &kSopNearWide},
    {"A06",
     0x8013CF00u,
     0x8013D1E4u,
     &emitAt<0x8013CF00u, false, kA06Scroll>,
     &emitAt<0x8013D1E4u, true, kA06Scroll>,
     &kA06Scroll},
    {"A06", 0x8013BA44u, 0x8013BD40u, &emitAt<0x8013BA44u, false, kA06>, &emitAt<0x8013BD40u, true, kA06>, &kA06},
    {"A0J",
     0x8010A3ACu,
     0x8010A69Cu,
     &emitAt<0x8010A3ACu, false, kSopNearLifted>,
     &emitAt<0x8010A69Cu, true, kSopNearLifted>,
     &kSopNearLifted},
};

} // namespace

std::uint32_t UnlitModelEmitter::emit(const psx::present::EmitMemory &memory,
                                      const ListCall &call,
                                      const Visibility &visible,
                                      const Variant &variant,
                                      bool quad) {
  const ModelShape &shape = quad ? kModelGt4 : kModelGt3;
  const std::uint32_t frame = call.sp - (quad ? variant.gt4FrameBytes : variant.gt3FrameBytes);
  return emitModelList(memory, call, shape, variant.stage, [&](const ModelPacket &packet) {
    bool projected = false;
    if (!quad) {
      projected = packet.projectGt3(variant.colourMask, visible);
    } else if (variant.gt4CornerThreeFirst) {
      projected = packet.projectGt4CornerThreeFirst(variant.colourMask, visible);
    } else {
      projected = packet.projectGt4(variant.firstGt4ColourMask, variant.colourMask, visible);
    }
    if (!projected) {
      return false;
    }
    const DepthRule &rule = variant.depth;
    const std::uint32_t flags = packet.flags() & rule.modeMask;
    if (variant.hideFlag != 0u && ModelPacket::hidden(memory, packet.flags(), variant.hideFlag)) {
      return false;
    }
    const ModelDepth mode = depthOf(rule, flags);
    const bool perMode = rule.staging == Staging::PerMode;
    const std::uint32_t staged =
        frame + (perMode && mode == ModelDepth::Nearest ? static_cast<std::uint32_t>(shape.cornerCount) * 4u : 0u);
    if (rule.staging == Staging::Always || mode != ModelDepth::Average) {
      packet.stageDepths(staged);
    }
    packet.depth(mode, staged);
    if (quad && rule.gt4DropsSorted && mode != ModelDepth::Average) {
      return false;
    }
    packet.clampNear(rule.nearClamp);
    if (packet.bucket() < 0) {
      return false;
    }
    if (variant.cue) {
      for (int corner = 0; corner < shape.cornerCount; ++corner) {
        packet.cueByDepth(corner, staged);
      }
    }
    packet.storeLastUv();
    if (variant.uScroll != 0u && (packet.flags() & kScrollFlag) != 0u) {
      packet.scrollU(memory.mem_r16(variant.uScroll) >> 4);
    }
    packet.link(call.ot);
    return true;
  });
}

void UnlitModelEmitter::gt3(Core &core, const Visibility &visible, const Variant &variant) {
  core.r[2] = emit(core, ListCall::fromRegisters(core), visible, variant, false);
}

void UnlitModelEmitter::gt4(Core &core, const Visibility &visible, const Variant &variant) {
  core.r[2] = emit(core, ListCall::fromRegisters(core), visible, variant, true);
}

void UnlitModelEmitter::registerStateRenders(Core &core) {
  for (const Copy &copy : kCopies) {
    core.stateProducers.install(copy.gt3, std::make_unique<UnlitStateProducer>(core, *copy.variant, false));
    core.stateProducers.install(copy.gt4, std::make_unique<UnlitStateProducer>(core, *copy.variant, true));
  }
}

void UnlitModelEmitter::registerOverrides() {
  for (const Copy &copy : kCopies) {
    if (copy.image.empty()) {
      tomba::native::declareOverride(copy.gt3, "UnlitModelEmitter::gt3", copy.gt3Body);
      tomba::native::declareOverride(copy.gt4, "UnlitModelEmitter::gt4", copy.gt4Body);
    } else {
      tomba::native::declareOverlayOverride(copy.image, copy.gt3, "UnlitModelEmitter::gt3", copy.gt3Body);
      tomba::native::declareOverlayOverride(copy.image, copy.gt4, "UnlitModelEmitter::gt4", copy.gt4Body);
    }
  }
}

} // namespace tomba2::render
