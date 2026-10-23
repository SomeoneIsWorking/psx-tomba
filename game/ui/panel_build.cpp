// game/ui/panel_build.cpp — Panel::build, guest FUN_8005019C, the 9-slice panel.
// Four 8x8 corner sprites, a DR_MODE for texture page 0x5F, then five fills; each call is its own object.
#include "core.h"
#include "core/overrides/native_override_catalog.h"
#include "entry/session.h"
#include "guest_abi.h"
#include "render/guest_ordering_table.h"
#include "render/libgpu_draw_mode.h"
#include "render/list_job.h"
#include "render/list_state_producer.h"
#include "ui/panel.h"

#include <memory>

namespace tomba {
namespace {

constexpr std::uint32_t kFrameBytes = 0x40u;
constexpr GuestFrameSpill kSpills[7] = {
    {20, 0x30}, {21, 0x34}, {31 /*ra*/, 0x38}, {19, 0x2C}, {18, 0x28}, {17, 0x24}, {16, 0x20}};
constexpr std::uint32_t kEdgeRect = 0x18u; // the frame's s16 x, y, w, h

constexpr std::uint16_t kStyleAlt = 0x40;
constexpr std::uint16_t kStyleAltAttr = 0x0D;
constexpr std::uint16_t kStyleAttr = 6;
constexpr std::uint16_t kAttrSemi = 0x80;
constexpr std::uint16_t kAttrClutAlt = 0x40;
constexpr std::uint16_t kAttrFlatShade = 0x20;
constexpr std::uint16_t kAttrClutRow = 0x1F;
constexpr std::uint16_t kClutRowBase = 0x1F0;

constexpr std::int16_t kBorder = 8;
constexpr std::uint32_t kCornerBytes = 16u;
constexpr std::uint32_t kCornerWords = 3u;
constexpr std::uint8_t kSprite8 = 0x74;
constexpr std::uint8_t kSprite8Semi = 0x76;
constexpr std::uint8_t kRawTexture = 0x01;
constexpr std::uint8_t kFlatShadeLevel = 0x40;
constexpr std::uint8_t kCornerV = 0x88;
constexpr std::uint32_t kModeBytes = 12u;
constexpr std::uint32_t kModeWords = 2u;
constexpr std::uint32_t kTexturePage = 0x5Fu;
constexpr std::uint32_t kFills = 5u;
constexpr std::uint32_t kFillBytes = 40u;
constexpr std::uint32_t kPacketBytes = 4u * kCornerBytes + kModeBytes + kFills * kFillBytes;

enum Edge : std::uint8_t { Top, Bottom, Left, Right, Centre };

struct Rect {
  std::int16_t x;
  std::int16_t y;
  std::int16_t w;
  std::int16_t h;
};

Rect readRect(const psx::present::EmitMemory &memory, std::uint32_t at) {
  return {static_cast<std::int16_t>(memory.mem_r16s(at)),
          static_cast<std::int16_t>(memory.mem_r16s(at + 2u)),
          static_cast<std::int16_t>(memory.mem_r16s(at + 4u)),
          static_cast<std::int16_t>(memory.mem_r16s(at + 6u))};
}

std::uint16_t attribute(std::uint32_t style, std::uint32_t shadow) {
  const auto bits = static_cast<std::uint16_t>(style);
  if ((bits & kStyleAlt) != 0) {
    return static_cast<std::uint16_t>(bits + kStyleAltAttr);
  }
  const auto attr = static_cast<std::uint16_t>(bits + kStyleAttr);
  return (shadow & 0xFFu) != 0 ? static_cast<std::uint16_t>(attr | kAttrSemi) : attr;
}

struct Corner {
  std::int16_t x;
  std::int16_t y;
  std::uint8_t u;
};

void drawCorner(const psx::present::EmitMemory &memory,
                const render::OrderingTable &ot,
                std::uint16_t attr,
                std::uint32_t bucket,
                const Corner &corner) {
  const std::uint32_t packet = render::PacketPool(memory).allocate(kCornerBytes);
  memory.mem_w8(packet + 7u, (attr & kAttrSemi) != 0 ? kSprite8Semi : kSprite8);
  const auto clut = static_cast<std::uint16_t>((((attr & kAttrClutRow) + kClutRowBase) << 6) |
                                               ((attr & kAttrClutAlt) != 0 ? 0x3Fu : 0x3Eu));
  memory.mem_w16(packet + 0xEu, clut);
  if ((attr & kAttrFlatShade) == 0) {
    memory.mem_w8(packet + 7u, static_cast<std::uint8_t>(memory.mem_r8(packet + 7u) | kRawTexture));
  } else {
    memory.mem_w8(packet + 6u, kFlatShadeLevel);
    memory.mem_w8(packet + 5u, kFlatShadeLevel);
    memory.mem_w8(packet + 4u, kFlatShadeLevel);
  }
  memory.mem_w16(packet + 8u, static_cast<std::uint16_t>(corner.x));
  memory.mem_w8(packet + 0xCu, corner.u);
  memory.mem_w8(packet + 0xDu, kCornerV);
  memory.mem_w16(packet + 0xAu, static_cast<std::uint16_t>(corner.y));
  ot.link({.packet = packet, .words = kCornerWords, .bucket = bucket});
}

void writeRect(const psx::present::EmitMemory &memory, std::uint32_t at, const Rect &rect) {
  memory.mem_w16(at, static_cast<std::uint16_t>(rect.x));
  memory.mem_w16(at + 2u, static_cast<std::uint16_t>(rect.y));
  memory.mem_w16(at + 4u, static_cast<std::uint16_t>(rect.w));
  memory.mem_w16(at + 6u, static_cast<std::uint16_t>(rect.h));
}

class PanelProducer final : public render::ListStateProducer {
public:
  using ListStateProducer::ListStateProducer;

protected:
  void emit(const psx::present::EmitMemory &memory, std::uint32_t, const render::ListCall &call) const override {
    Panel::build(memory, call);
  }
};

} // namespace

// FUN_8005019C(rect r4, style r5, shadow r6, otBucket r7)
void Panel::build(const psx::present::EmitMemory &memory, const render::ListCall &call) {
  const render::OrderingTable ot(memory, call.ot);
  const std::uint16_t attr = attribute(call.a1, call.count);
  const std::uint32_t bucket = call.a3;
  const Rect rect = readRect(memory, call.list);
  const auto right = static_cast<std::int16_t>(rect.x + rect.w);
  const auto bottom = static_cast<std::int16_t>(rect.y + rect.h);
  const auto left = static_cast<std::int16_t>(rect.x - kBorder);
  const auto top = static_cast<std::int16_t>(rect.y - kBorder);
  drawCorner(memory, ot, attr, bucket, {.x = left, .y = top, .u = 0xB8});
  drawCorner(memory, ot, attr, bucket, {.x = right, .y = top, .u = 0xC8});
  drawCorner(memory, ot, attr, bucket, {.x = left, .y = bottom, .u = 0xE8});
  drawCorner(memory, ot, attr, bucket, {.x = right, .y = bottom, .u = 0xF8});

  const std::uint32_t mode = render::PacketPool(memory).allocate(kModeBytes);
  render::setDrawMode(memory,
                      {.packet = mode, .drawOnDisplay = 0, .dither = 0, .texturePage = kTexturePage, .window = 0});
  ot.link({.packet = mode, .words = kModeWords, .bucket = bucket});

  const std::uint32_t edge = call.sp - kFrameBytes + kEdgeRect;
  const auto fillAttr = static_cast<std::uint16_t>(attr & 0xFFu);
  const auto fillEdge = [&](std::uint32_t rectAt, Edge uv) {
    fill(memory, ot, {.rect = rectAt, .uvIndex = uv, .attr = fillAttr, .bucket = bucket});
  };
  writeRect(memory, edge, {.x = rect.x, .y = top, .w = rect.w, .h = kBorder});
  fillEdge(edge, Top);
  writeRect(memory, edge, {.x = rect.x, .y = bottom, .w = rect.w, .h = kBorder});
  fillEdge(edge, Bottom);
  writeRect(memory, edge, {.x = left, .y = rect.y, .w = kBorder, .h = rect.h});
  fillEdge(edge, Left);
  writeRect(memory, edge, {.x = right, .y = rect.y, .w = kBorder, .h = rect.h});
  fillEdge(edge, Right);
  fillEdge(call.list, Centre);
}

// FUN_8005019C as the guest calls it.
void Panel::drawPanel(Core *core) {
  const render::ListCall call{.list = core->r[4],
                              .ot = core->mem_r32(render::OrderingTable::kBasePointer),
                              .count = core->r[6],
                              .a3 = core->r[7],
                              .sp = core->r[29],
                              .a1 = core->r[5]};
  ::GuestFrame<kFrameBytes, 7> frame(core, kSpills);
  const render::EmitterObject object(
      *core, Panel::kBuild, render::OwnObject{sessionOf(core).callSites.object(core->r[31], call.list)});
  render::ListJobWriter job(*core, 0, call, kPacketBytes);
  job.input(*core, call.list, sizeof(Rect), render::InputBlend::Lanes16);
  Panel::build(*core, call);
  job.save(*core);
}

void Panel::registerStateRenders(psx::present::StateProducerTable &renders, Core &core) {
  renders.install(kBuild, std::make_unique<PanelProducer>(core));
}

} // namespace tomba
