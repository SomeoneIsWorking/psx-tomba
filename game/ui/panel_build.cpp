// game/ui/panel_build.cpp — Panel::build, guest FUN_8005019C, the 9-slice panel.
// Four 8x8 corner sprites, a DR_MODE for texture page 0x5F, then five fills.
#include "blend.h"
#include "core.h"
#include "core/overrides/native_override_catalog.h"
#include "entry/session.h"
#include "guest_abi.h"
#include "guest_redraw.h"
#include "projected_vertex.h"
#include "render/call_site_owner.h"
#include "render/guest_ordering_table.h"
#include "render/libgpu_draw_mode.h"
#include "render/list_job.h"
#include "ui/panel.h"

#include <array>

namespace tomba {
namespace {

using render::CallSiteOwner;
constexpr std::uint8_t kS0 = 16;
constexpr std::uint8_t kS1 = 17;
// FUN_8005019C's callers and the object each draws for.
constexpr CallSiteOwner kOwners[] = {
    {.returnAddress = 0x8004FD08u, .object = kS1}, // 0x8004FB4C: the window record at 0x800BF554 + i * 0x8C
    {.returnAddress = 0x80073898u},                // 0x800737F8: the save prompt's message box, once a frame
    {.returnAddress = 0x8007399Cu},                // 0x800738B0: the save prompt's choice box, once a frame
    {.returnAddress = 0x8007DA40u, .object = kS0}, // 0x8007D594: the dialog node opening
    {.returnAddress = 0x8007DD90u, .object = kS0}, // 0x8007DC38: the dialog node
    {.returnAddress = 0x8007DEBCu, .object = kS0}, // 0x8007DDE0: the dialog node
};

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

enum Edge : std::uint8_t { Top, Bottom, Left, Right, Centre };

struct Rect {
  std::int16_t x;
  std::int16_t y;
  std::int16_t w;
  std::int16_t h;
};

// Where the 9-slice puts its pieces for a rect: the corner sprites' places and the five fills' x, y, w, h.
template <class T> struct Layout {
  std::array<std::array<T, 2>, 4> corners;
  std::array<std::array<T, 4>, kFills> fills;
};
template <class T> Layout<T> layoutOf(T x, T y, T w, T h) {
  const T border = kBorder;
  const T left = x - border;
  const T top = y - border;
  const T right = x + w;
  const T bottom = y + h;
  return {
      .corners = {{{left, top}, {right, top}, {left, bottom}, {right, bottom}}},
      .fills = {
          {{x, top, w, border}, {x, bottom, w, border}, {left, y, border, h}, {right, y, border, h}, {x, y, w, h}}}};
}

// The packet run from the pool cursor: four corners, the mode, five fills.
constexpr std::uint32_t kModeAt = 4u * kCornerBytes;
constexpr std::uint32_t kFillsAt = kModeAt + kModeBytes;
constexpr std::uint32_t kVertex = 8u;
constexpr std::array<std::uint32_t, 4> kFillVertices{8u, 16u, 24u, 32u};

Rect readRect(const psx::present::EmitMemory &memory, std::uint32_t at) {
  return {static_cast<std::int16_t>(memory.mem_r16s(at)),
          static_cast<std::int16_t>(memory.mem_r16s(at + 2u)),
          static_cast<std::int16_t>(memory.mem_r16s(at + 4u)),
          static_cast<std::int16_t>(memory.mem_r16s(at + 6u))};
}

// The packet attribute from FUN_8005019C's style (a1) and shadow (a2).
std::uint16_t attribute(const render::ListCall &call) {
  const std::uint32_t shadow = call.count;
  const auto bits = static_cast<std::uint16_t>(call.a1);
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
  std::uint16_t attr;
  std::uint32_t bucket;
};

void drawCorner(const psx::present::EmitMemory &memory, const render::OrderingTable &ot, const Corner &corner) {
  const std::uint16_t attr = corner.attr;
  const std::uint32_t bucket = corner.bucket;
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

} // namespace

// FUN_8005019C(rect r4, style r5, shadow r6, otBucket r7)
void Panel::build(const psx::present::EmitMemory &memory, const render::ListCall &call) {
  const render::OrderingTable ot(memory, call.ot);
  const std::uint16_t attr = attribute(call);
  const std::uint32_t bucket = call.a3;
  const Rect rect = readRect(memory, call.list);
  const Layout<int> layout = layoutOf<int>(rect.x, rect.y, rect.w, rect.h);
  constexpr std::array<std::uint8_t, 4> kCornerU{0xB8, 0xC8, 0xE8, 0xF8};
  for (std::size_t corner = 0; corner < kCornerU.size(); ++corner) {
    drawCorner(memory,
               ot,
               {.x = static_cast<std::int16_t>(layout.corners[corner][0]),
                .y = static_cast<std::int16_t>(layout.corners[corner][1]),
                .u = kCornerU[corner],
                .attr = attr,
                .bucket = bucket});
  }

  const std::uint32_t mode = render::PacketPool(memory).allocate(kModeBytes);
  render::setDrawMode(memory,
                      {.packet = mode, .drawOnDisplay = 0, .dither = 0, .texturePage = kTexturePage, .window = 0});
  ot.link({.packet = mode, .words = kModeWords, .bucket = bucket});

  const std::uint32_t edge = call.sp - kFrameBytes + kEdgeRect;
  const auto fillAttr = static_cast<std::uint16_t>(attr & 0xFFu);
  const auto fillEdge = [&](std::uint32_t rectAt, Edge uv) {
    fill(memory, ot, {.rect = rectAt, .uvIndex = uv, .attr = fillAttr, .bucket = bucket});
  };
  for (const Edge uv : {Top, Bottom, Left, Right}) {
    const std::array<int, 4> &piece = layout.fills[uv];
    writeRect(memory,
              edge,
              {.x = static_cast<std::int16_t>(piece[0]),
               .y = static_cast<std::int16_t>(piece[1]),
               .w = static_cast<std::int16_t>(piece[2]),
               .h = static_cast<std::int16_t>(piece[3])});
    fillEdge(edge, uv);
  }
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
  const std::optional<std::uint32_t> object = render::callSiteOwner(*core, kOwners);
  const std::uint32_t returnAddress = core->r[31];
  ::GuestFrame<kFrameBytes, 7> frame(core, kSpills);
  const std::uint32_t packets = render::PacketPool(*core).cursor();
  Panel::build(*core, call);
  if (!object) {
    sessionOf(core).drawReplay.updateDraws().unowned(*core, Panel::kBuild, returnAddress);
    return;
  }
  const Rect rect = readRect(*core, call.list);
  sessionOf(core).drawReplay.updateDraws().note(*core,
                                                {.writer = Panel::kBuild,
                                                 .object = *object,
                                                 .packets = {packets, 0u},
                                                 .values = {static_cast<std::uint32_t>(rect.x),
                                                            static_cast<std::uint32_t>(rect.y),
                                                            static_cast<std::uint32_t>(rect.w),
                                                            static_cast<std::uint32_t>(rect.h)},
                                                 .rebuild = &Panel::rebuild});
}

void Panel::rebuild(const psx::present::EmitMemory & /*memory*/,
                    psx::present::GuestRedraw &redraw,
                    const UpdateBlockDraws::Draw &earlier,
                    const UpdateBlockDraws::Draw &later,
                    float t) {
  std::array<double, 4> rect{};
  for (std::size_t lane = 0; lane < rect.size(); ++lane) {
    rect[lane] = psx::present::lerpExact(
        static_cast<std::int32_t>(earlier.values[lane]), static_cast<std::int32_t>(later.values[lane]), t);
  }
  const Layout<double> layout = layoutOf(rect[0], rect[1], rect[2], rect[3]);
  const std::uint32_t packets = later.packets[0];
  const auto place = [&redraw](std::uint32_t at, double x, double y) {
    redraw.writeVertex(at,
                       psx::gte::ProjectedVertex::nearest({static_cast<float>(x), static_cast<float>(y), 0.0f, 0.0f}));
  };
  for (std::size_t corner = 0; corner < layout.corners.size(); ++corner) {
    place(packets + corner * kCornerBytes + kVertex, layout.corners[corner][0], layout.corners[corner][1]);
  }
  for (std::size_t piece = 0; piece < layout.fills.size(); ++piece) {
    const auto [x, y, w, h] = layout.fills[piece];
    const std::uint32_t fill = packets + kFillsAt + piece * kFillBytes;
    place(fill + kFillVertices[0], x, y);
    place(fill + kFillVertices[1], x + w, y);
    place(fill + kFillVertices[2], x, y + h);
    place(fill + kFillVertices[3], x + w, y + h);
  }
}

} // namespace tomba
