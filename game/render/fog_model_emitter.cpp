// game/render/fog_model_emitter.cpp — A04's fogged model emitters. See fog_model_emitter.h.
#include "fog_model_emitter.h"

#include "core.h"
#include "core/overrides/native_override_catalog.h"
#include "gte_registers.h"
#include "guest_ordering_table.h"
#include "horizontal_visibility_cull.h"
#include "list_job.h"
#include "model_packet.h"
#include "object_draw.h"

#include <algorithm>
#include <array>

namespace tomba::render {
namespace {

using horizontal_cull::Visibility;

constexpr std::uint32_t kGt3 = 0x8013F418u;
constexpr std::uint32_t kGt4 = 0x8013F6E4u;

constexpr std::uint32_t kOwnColours = 4u;
constexpr std::uint32_t kPhaseHide = 8u;
constexpr std::uint32_t kHideFlag = 0x20u;
constexpr std::uint32_t kNearestFlag = 2u;
constexpr std::uint32_t kPhase = 0x1F800207u; // s8
constexpr std::int32_t kPhaseHideFirst = 15;
constexpr std::int32_t kPhaseHideLast = 17;
constexpr std::int32_t kFogBias = 0x200;
constexpr std::int32_t kFogFull = 0x1000;
constexpr std::uint32_t kGt3Tag = 0x09000000u;
constexpr std::uint32_t kGt4Tag = 0x0C000000u;

// The low colour of a record word: its second colour's nibbles moved up, as the packed records store two.
std::uint32_t lowColour(std::uint32_t word) {
  return (word & 0x0FFF0F0Fu) << 4;
}

std::int32_t fogOf(std::int32_t otz) {
  return std::clamp(otz * 4 - kFogBias, 0, kFogFull);
}

bool hidden(const psx::present::EmitMemory &memory, std::uint32_t flags) {
  if ((flags & kOwnColours) != 0u) {
    return false;
  }
  const std::int32_t phase = memory.mem_r8s(kPhase);
  const bool phaseHides = (flags & kPhaseHide) != 0u && phase >= kPhaseHideFirst && phase <= kPhaseHideLast;
  return phaseHides || ((flags & kHideFlag) != 0u && memory.mem_r8(kHideWord) != 0u);
}

std::int32_t depthOf(psx::cpu::Gte &gte, std::uint32_t flags, const ModelShape &shape) {
  if (flags == 0u) {
    return static_cast<std::int32_t>(gte.readData(psx::gte::kOtz));
  }
  std::array<std::int32_t, 4> depths{};
  for (int corner = 0; corner < shape.cornerCount; ++corner) {
    depths[static_cast<std::size_t>(corner)] =
        static_cast<std::int32_t>(gte.readData(shape.firstDepth + static_cast<std::uint32_t>(corner)));
  }
  return OrderingTable::extremeDepth((flags & kNearestFlag) == 0u,
                                     std::span(depths.data(), static_cast<std::size_t>(shape.cornerCount)));
}

// Fogs or keeps the colours, sorts and links; false when the record is hidden or out of the buckets.
bool finish(const psx::present::EmitMemory &memory,
            const ModelPacket &packet,
            const ModelShape &shape,
            const std::array<std::uint32_t, 4> &colours,
            std::uint32_t ot) {
  psx::cpu::Gte &gte = memory.core().gte();
  const std::uint32_t flags = packet.flags();
  if (hidden(memory, flags)) {
    return false;
  }
  if ((flags & kOwnColours) != 0u) {
    for (int corner = 0; corner < shape.cornerCount; ++corner) {
      const auto index = static_cast<std::size_t>(corner);
      memory.mem_w32(packet.packet() + ModelPacket::kColour[index], colours[index]);
    }
  }
  const std::int32_t bucket = OrderingTable::compressDepth(depthOf(gte, flags, shape));
  if (!OrderingTable::inDepthRangeExclusive(bucket)) {
    return false;
  }
  const OrderingTable table(memory, ot);
  const std::uint32_t slot = table.slot(static_cast<std::uint32_t>(bucket));
  const std::uint32_t head = memory.mem_r32(slot);
  memory.mem_w32(slot, packet.packet());
  memory.mem_w32(packet.packet(), head | (shape.cornerCount == 3 ? kGt3Tag : kGt4Tag));
  return true;
}

void fog(psx::cpu::Gte &gte, const std::array<std::uint32_t, 4> &colours, std::int32_t otz) {
  gte.writeData(psx::gte::kRgb0, colours[0]);
  gte.writeData(psx::gte::kRgbc, colours[0]);
  gte.writeData(psx::gte::kRgb1, colours[1]);
  gte.writeData(psx::gte::kRgb2, colours[2]);
  gte.writeData(psx::gte::kIr0, static_cast<std::uint32_t>(fogOf(otz)));
  gte.execute(psx::gte::kDpct);
}

void storeFogged(const psx::present::EmitMemory &memory, const ModelPacket &packet) {
  psx::cpu::Gte &gte = memory.core().gte();
  memory.mem_w32(packet.packet() + ModelPacket::kColour[0], gte.readData(psx::gte::kRgb0));
  memory.mem_w32(packet.packet() + ModelPacket::kColour[1], gte.readData(psx::gte::kRgb1));
  memory.mem_w32(packet.packet() + ModelPacket::kColour[2], gte.readData(psx::gte::kRgb2));
}

bool emitGt3(const psx::present::EmitMemory &memory,
             const ModelPacket &packet,
             const Visibility &visible,
             std::uint32_t ot) {
  psx::cpu::Gte &gte = memory.core().gte();
  const std::uint32_t record = packet.record();
  packet.loadTriangle();
  memory.mem_w32(packet.packet() + ModelPacket::kUv[2], memory.mem_r32(record + 0x20u) >> 16);
  gte.execute(psx::gte::kRtpt);
  const std::uint32_t colour0 = memory.mem_r32(record);
  const std::uint32_t colour12 = memory.mem_r32(record + 4u);
  memory.mem_w32(packet.packet() + ModelPacket::kUv[0], memory.mem_r32(record + 8u));
  memory.mem_w32(packet.packet() + ModelPacket::kUv[1], memory.mem_r32(record + 0xCu));
  if (static_cast<std::int32_t>(gte.readControl(psx::gte::kFlag)) < 0) {
    return false;
  }
  gte.execute(psx::gte::kNclip);
  const std::array<std::uint32_t, 4> colours{colour0, colour12 & kColourCodeMask, lowColour(colour12), 0u};
  if (static_cast<std::int32_t>(gte.readData(psx::gte::kMac0)) <= 0) {
    return false;
  }
  packet.storeTriangle();
  if (!packet.onScreen(visible)) {
    return false;
  }
  gte.execute(psx::gte::kAvsz3);
  const auto otz = static_cast<std::int32_t>(gte.readData(psx::gte::kOtz));
  if (otz <= 0) {
    return false;
  }
  fog(gte, colours, otz);
  storeFogged(memory, packet);
  return finish(memory, packet, kModelGt3, colours, ot);
}

bool emitGt4(const psx::present::EmitMemory &memory,
             const ModelPacket &packet,
             const Visibility &visible,
             std::uint32_t ot) {
  psx::cpu::Gte &gte = memory.core().gte();
  const std::uint32_t record = packet.record();
  packet.loadTriangle();
  gte.execute(psx::gte::kRtpt);
  const std::uint32_t colour01 = memory.mem_r32(record);
  const std::uint32_t colour23 = memory.mem_r32(record + 4u);
  memory.mem_w32(packet.packet() + ModelPacket::kUv[0], memory.mem_r32(record + 8u));
  memory.mem_w32(packet.packet() + ModelPacket::kUv[1], memory.mem_r32(record + 0xCu));
  if (static_cast<std::int32_t>(gte.readControl(psx::gte::kFlag)) < 0) {
    return false;
  }
  gte.execute(psx::gte::kNclip);
  if (static_cast<std::int32_t>(gte.readData(psx::gte::kMac0)) <= 0) {
    return false;
  }
  packet.storeTriangle();
  packet.loadCornerThree();
  gte.execute(psx::gte::kRtps);
  const std::uint32_t lastUvs = memory.mem_r32(record + 0x10u);
  memory.mem_w32(packet.packet() + ModelPacket::kUv[2], lastUvs);
  memory.mem_w32(packet.packet() + ModelPacket::kUv[3], lastUvs >> 16);
  const std::array<std::uint32_t, 4> colours{
      colour01 & kColourCodeMask, lowColour(colour01), colour23 & kColourCodeMask, lowColour(colour23)};
  if (static_cast<std::int32_t>(gte.readControl(psx::gte::kFlag)) < 0) {
    return false;
  }
  memory.storeVertex(packet.packet() + ModelPacket::kScreen[3], gte.projected(2));
  if (!packet.onScreen(visible)) {
    return false;
  }
  gte.execute(psx::gte::kAvsz4);
  fog(gte, colours, static_cast<std::int32_t>(gte.readData(psx::gte::kOtz)));
  storeFogged(memory, packet);
  gte.writeData(psx::gte::kRgbc, colours[3]);
  gte.execute(psx::gte::kDpcs);
  memory.mem_w32(packet.packet() + ModelPacket::kColour[3], gte.readData(psx::gte::kRgb2));
  return finish(memory, packet, kModelGt4, colours, ot);
}

template <std::uint32_t Entry, bool Quad> void emitAt(Core *core) {
  const ListCall call = ListCall::fromRegisters(*core);
  const EmitterObject object(*core, Entry, call.list);
  core->r[2] = FogModelEmitter::emit(*core, call, horizontal_cull::forDrawWindow(core), Quad);
}

} // namespace

std::uint32_t FogModelEmitter::emit(const psx::present::EmitMemory &memory,
                                    const ListCall &call,
                                    const Visibility &visible,
                                    bool quad) {
  const ModelShape &shape = quad ? kModelGt4 : kModelGt3;
  return emitModelList(memory, call, shape, kOverlayStage, [&](const ModelPacket &packet) {
    return quad ? emitGt4(memory, packet, visible, call.ot) : emitGt3(memory, packet, visible, call.ot);
  });
}

void FogModelEmitter::registerOverrides(native::NativeOverrideCatalog &natives) {
  natives.declareOverlay("A04", kGt3, "FogModelEmitter::gt3", &emitAt<kGt3, false>);
  natives.declareOverlay("A04", kGt4, "FogModelEmitter::gt4", &emitAt<kGt4, true>);
}

} // namespace tomba::render
