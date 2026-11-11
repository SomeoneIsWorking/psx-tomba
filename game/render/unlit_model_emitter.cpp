// game/render/unlit_model_emitter.cpp — the unlit model emitters. See unlit_model_emitter.h.
#include "unlit_model_emitter.h"

#include "blend.h"
#include "core.h"
#include "core/overrides/native_override_catalog.h"
#include "entry/session.h"
#include "horizontal_visibility_cull.h"
#include "projected_vertex.h"

#include <algorithm>
#include <optional>
#include <string_view>

namespace tomba::render {
namespace {

using horizontal_cull::Visibility;

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

std::uint32_t
scrollAmount(const psx::present::EmitMemory &memory, UnlitModelEmitter::ScrollRead read, std::uint32_t word) {
  if (word == 0u) {
    return 0u;
  }
  return read == UnlitModelEmitter::ScrollRead::Byte ? memory.mem_r8(word) : memory.mem_r16(word) >> 4;
}

void scrollUv(const psx::present::EmitMemory &memory,
              const ModelPacket &packet,
              const UnlitModelEmitter::UvScroll &scroll) {
  for (const UnlitModelEmitter::ScrollBit &bit : scroll.bits) {
    if ((packet.flags() & bit.flag) == 0u) {
      continue;
    }
    packet.scrollUv(scrollAmount(memory, scroll.read, bit.u), scrollAmount(memory, scroll.read, bit.v));
    if (bit.bucket != 0) {
      packet.setBucket(bit.bucket);
    }
    if (scroll.firstOnly) {
      return;
    }
  }
}

// rsin's 4.12 result to a screen-row offset.
constexpr std::int32_t kWaveShift = 10;

std::int32_t waveRows(const psx::present::EmitMemory &memory, std::uint32_t call, std::int32_t phase, std::int32_t x) {
  return emitterTrig(memory, {.cosine = false, .returnAddress = call}, phase + x * 4) >> kWaveShift;
}

// A replay places each corner's whole-row offset t of the way from the one the list read in the draw before.
void wave(const psx::present::EmitMemory &memory,
          const ModelPacket &packet,
          const ModelShape &shape,
          const UnlitModelEmitter::ScreenWave &rule,
          std::uint32_t list) {
  if (rule.flag == 0u || (packet.flags() & rule.flag) == 0u) {
    return;
  }
  const std::uint32_t phaseWord = memory.mem_r32(rule.phase);
  const auto phase = static_cast<std::int32_t>(phaseWord);
  DrawReplay &replay = sessionOf(&memory.core()).drawReplay;
  struct Blend {
    std::int32_t earlier;
    float t;
  };
  std::optional<Blend> blend;
  if (const std::optional<float> t = replay.frame().replaying()) {
    if (const std::optional<std::uint32_t> earlier = replay.wavePhases().earlier(list)) {
      blend = Blend{.earlier = static_cast<std::int32_t>(*earlier), .t = *t};
    }
  } else {
    replay.wavePhases().note(list, phaseWord);
  }
  for (int corner = 0; corner < shape.cornerCount; ++corner) {
    const auto index = static_cast<std::size_t>(corner);
    const std::int32_t x = memory.mem_r16s(packet.record() + shape.corners[index].x);
    const std::uint32_t call = shape.cornerCount == 3 ? rule.gt3Calls[index] : rule.gt4Calls[index];
    const std::int32_t from = blend ? waveRows(memory, call, blend->earlier, x) : 0;
    const std::int32_t rows = waveRows(memory, call, phase, x);
    const std::uint32_t at = packet.packet() + ModelPacket::kScreen[index];
    const psx::gte::ProjectedVertex vertex = memory.loadVertex(at);
    const std::uint32_t word = vertex.word();
    const auto y = static_cast<std::uint16_t>((word >> 16) + static_cast<std::uint32_t>(rows));
    const psx::gte::ProjectedVertex stepped =
        vertex.steppedTo((word & 0xFFFFu) | (static_cast<std::uint32_t>(y) << 16));
    if (!blend) {
      memory.storeVertex(at, stepped);
      continue;
    }
    GtePlace place = vertex.place();
    place.y += static_cast<float>(psx::present::lerpExact(from, rows, blend->t));
    memory.storeVertex(at, {stepped.word(), place});
  }
  packet.biasBucket(rule.bucketBias);
}

// Every copy draws inside a replayed section, so it saves nothing.
template <std::uint32_t Entry, bool Quad, const UnlitModelEmitter::Variant &Data> void emitAt(Core *core) {
  const ListCall call = ListCall::fromRegisters(*core);
  const EmitterObject object(*core, Entry, call.list);
  core->r[2] = UnlitModelEmitter::emit(*core, call, horizontal_cull::forDrawWindow(core), Data, Quad);
}

struct Copy {
  std::string_view image; // empty for the resident executable
  std::uint32_t gt3;
  std::uint32_t gt4; // 0 when the copy has no GT4
  psx::cpu::NativeFunction gt3Body;
  psx::cpu::NativeFunction gt4Body;
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
constexpr const auto &kA0eArena = UnlitModelEmitter::kA0eArena;
constexpr const auto &kA02 = UnlitModelEmitter::kA02;
constexpr const auto &kA0a = UnlitModelEmitter::kA0a;
constexpr const auto &kA0f = UnlitModelEmitter::kA0f;
constexpr const auto &kA0d = UnlitModelEmitter::kA0d;
constexpr const auto &kA03 = UnlitModelEmitter::kA03;

constexpr Copy kCopies[] = {
    {{}, 0x8007FDB0u, 0x8008007Cu, &emitAt<0x8007FDB0u, false, kResident>, &emitAt<0x8008007Cu, true, kResident>},
    {"SOP", 0x801099B4u, 0x80109C80u, &emitAt<0x801099B4u, false, kSop>, &emitAt<0x80109C80u, true, kSop>},
    {"A0B", 0x80112A24u, 0x80112CF0u, &emitAt<0x80112A24u, false, kSop>, &emitAt<0x80112CF0u, true, kSop>},
    {"A0C", 0x80113788u, 0x80113A54u, &emitAt<0x80113788u, false, kSop>, &emitAt<0x80113A54u, true, kSop>},
    {"A01", 0x80132690u, 0x801329C4u, &emitAt<0x80132690u, false, kA01Cue>, &emitAt<0x801329C4u, true, kA01Cue>},
    {"A0A", 0x801103F4u, 0x80110698u, &emitAt<0x801103F4u, false, kFlagged>, &emitAt<0x80110698u, true, kFlagged>},
    {"A0B", 0x80113150u, 0x801133F4u, &emitAt<0x80113150u, false, kFlagged>, &emitAt<0x801133F4u, true, kFlagged>},
    {"A0C", 0x80113EB4u, 0x80114158u, &emitAt<0x80113EB4u, false, kFlagged>, &emitAt<0x80114158u, true, kFlagged>},
    {"A0D", 0x80113748u, 0x801139ECu, &emitAt<0x80113748u, false, kFlagged>, &emitAt<0x801139ECu, true, kFlagged>},
    {"A0E", 0x80113AECu, 0x80113DE0u, &emitAt<0x80113AECu, false, kA0eArena>, &emitAt<0x80113DE0u, true, kA0eArena>},
    {"A0E", 0x80114458u, 0x801146FCu, &emitAt<0x80114458u, false, kFlagged>, &emitAt<0x801146FCu, true, kFlagged>},
    {"A0F", 0x801157CCu, 0x80115A70u, &emitAt<0x801157CCu, false, kFlagged>, &emitAt<0x80115A70u, true, kFlagged>},
    {"A0H", 0x8010B6BCu, 0x8010B960u, &emitAt<0x8010B6BCu, false, kFlagged>, &emitAt<0x8010B960u, true, kFlagged>},
    {"A0I", 0x8010BB40u, 0x8010BDE4u, &emitAt<0x8010BB40u, false, kFlagged>, &emitAt<0x8010BDE4u, true, kFlagged>},
    {"A0J", 0x8010AB20u, 0x8010ADC4u, &emitAt<0x8010AB20u, false, kFlagged>, &emitAt<0x8010ADC4u, true, kFlagged>},
    {"A0G", 0x8010BC40u, 0x8010BF28u, &emitAt<0x8010BC40u, false, kSopNear>, &emitAt<0x8010BF28u, true, kSopNear>},
    {"A0I", 0x8010B3DCu, 0x8010B6C4u, &emitAt<0x8010B3DCu, false, kSopNear>, &emitAt<0x8010B6C4u, true, kSopNear>},
    {"A0H",
     0x8010AF58u,
     0x8010B240u,
     &emitAt<0x8010AF58u, false, kSopNearWide>,
     &emitAt<0x8010B240u, true, kSopNearWide>},
    {"A06", 0x8013CF00u, 0x8013D1E4u, &emitAt<0x8013CF00u, false, kA06Scroll>, &emitAt<0x8013D1E4u, true, kA06Scroll>},
    {"A06", 0x8013BA44u, 0x8013BD40u, &emitAt<0x8013BA44u, false, kA06>, &emitAt<0x8013BD40u, true, kA06>},
    {"A0A", 0x8010F97Cu, 0x8010FD74u, &emitAt<0x8010F97Cu, false, kA0a>, &emitAt<0x8010FD74u, true, kA0a>},
    {"A0F", 0x80114A5Cu, 0x80114E70u, &emitAt<0x80114A5Cu, false, kA0f>, &emitAt<0x80114E70u, true, kA0f>},
    {"A0D", 0x80112DE4u, 0x801131B0u, &emitAt<0x80112DE4u, false, kA0d>, &emitAt<0x801131B0u, true, kA0d>},
    {"A03", 0x80110094u, 0u, &emitAt<0x80110094u, false, kA03>, nullptr},
    {"A02", 0x80124DB8u, 0x80125100u, &emitAt<0x80124DB8u, false, kA02>, &emitAt<0x80125100u, true, kA02>},
    {"A0J",
     0x8010A3ACu,
     0x8010A69Cu,
     &emitAt<0x8010A3ACu, false, kSopNearLifted>,
     &emitAt<0x8010A69Cu, true, kSopNearLifted>},
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
    if (packet.hiddenByX(variant.xHide)) {
      return false;
    }
    bool projected = false;
    if (!quad) {
      projected = packet.projectGt3(
          variant.colourMask, visible, variant.twoSidedFlag != 0u && (packet.flags() & variant.twoSidedFlag) != 0u);
    } else if (variant.gt4CornerThreeFirst) {
      projected = packet.projectGt4CornerThreeFirst(variant.colourMask, visible);
    } else {
      projected = packet.projectGt4(
          {.firstColourMask = variant.firstGt4ColourMask, .colourMask = variant.colourMask, .visible = visible});
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
    if (rule.staging == Staging::Registers) {
      packet.depthFromRegisters(mode);
    } else {
      if (rule.staging == Staging::Always || mode != ModelDepth::Average) {
        packet.stageDepths(staged);
      }
      packet.depth(mode, staged);
    }
    if (quad && rule.gt4DropsSorted && mode != ModelDepth::Average) {
      return false;
    }
    packet.clampNear(rule.nearClamp);
    if (packet.bucket(rule.bandBias) < 0) {
      return false;
    }
    if (variant.cue) {
      for (int corner = 0; corner < shape.cornerCount; ++corner) {
        packet.cueByDepth(corner, staged);
      }
    }
    packet.storeLastUv();
    scrollUv(memory, packet, variant.uvScroll);
    wave(memory, packet, shape, variant.wave, call.list);
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

void UnlitModelEmitter::registerOverrides(native::NativeOverrideCatalog &natives) {
  for (const Copy &copy : kCopies) {
    if (copy.image.empty()) {
      natives.declare(copy.gt3, "UnlitModelEmitter::gt3", copy.gt3Body);
      natives.declare(copy.gt4, "UnlitModelEmitter::gt4", copy.gt4Body);
    } else {
      natives.declareOverlay(copy.image, copy.gt3, "UnlitModelEmitter::gt3", copy.gt3Body);
      if (copy.gt4 != 0u) {
        natives.declareOverlay(copy.image, copy.gt4, "UnlitModelEmitter::gt4", copy.gt4Body);
      }
    }
  }
}

} // namespace tomba::render
