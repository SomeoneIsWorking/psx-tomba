#include "hud_anchor.h"

#include "core.h"
#include "gp0_primitive_decode.h"
#include "guest_widescreen_projection.h"
#include "packet_pool.h"
#include "r3000.h"
#include "record_geometry.h"
#include "resumable_guest_call.h"

#include <algorithm>
#include <array>
#include <climits>
#include <cstddef>

namespace tomba1 {
namespace {

using psx::cpu::kRa;

// The flip 0x80016940 clears the table backwards (ClearOTagR, 0x328 entries) and draws it from its last entry.
constexpr std::uint32_t kOrderingTable = 0x1F8001E0u;
constexpr std::uint32_t kWalkStart = 0xC9Cu;
// An element wholly left of the picture's centre column belongs to the left edge, wholly right of it to the right.
constexpr int kCentreColumn = 160;
// A packet tag's length byte.
constexpr std::size_t kPacketWords = 0xFF;

// One element drawer the HUD pass calls.
struct Element {
  std::uint32_t address = 0;
  const char *name = nullptr;
};

constexpr std::array kElements{
    Element{.address = 0x8004E590u, .name = "HudAnchor::lives"},
    Element{.address = 0x8004E714u, .name = "HudAnchor::points"},
    Element{.address = 0x8004E900u, .name = "HudAnchor::vitality"},
    Element{.address = 0x8004EB10u, .name = "HudAnchor::item"},
    Element{.address = 0x8004ED80u, .name = "HudAnchor::weapon"},
};

std::uint32_t packetEnd(Core &core, std::uint32_t packet) {
  return packet + (core.mem_r8(packet + 3u) + 1u) * 4u;
}

// The columns [first, last) the element's draw packets in the pool span [first, end) cover, as written.
struct Columns {
  int first = INT_MAX;
  int last = INT_MIN;
};
Columns drawnColumns(Core &core, std::uint32_t first, std::uint32_t end) {
  Columns columns;
  std::array<std::uint32_t, kPacketWords> words{};
  for (std::uint32_t packet = first; packet < end; packet = packetEnd(core, packet)) {
    const std::size_t count = core.mem_r8(packet + 3u);
    for (std::size_t word = 0; word < count; ++word) {
      words[word] = core.mem_r32(packet + 4u + static_cast<std::uint32_t>(word) * 4u);
    }
    const auto primitive = psx::gpu::decodePacketPrimitive({.words = std::span(words.data(), count), .places = {}});
    if (!primitive) {
      continue;
    }
    const psx::gpu::RecordRect bounds = psx::gpu::primitiveBounds(*primitive);
    columns.first = std::min(columns.first, bounds.x0);
    columns.last = std::max(columns.last, bounds.x1);
  }
  return columns;
}

// Each packet the element added to the pool, [first, end), moved `dx` columns in the walk that draws them.
void movePackets(Core &core, std::uint32_t first, std::uint32_t end, int dx) {
  const std::uint32_t root = core.mem_r32(kOrderingTable) + kWalkStart;
  for (std::uint32_t packet = first; packet < end; packet = packetEnd(core, packet)) {
    core.hostPackets.moveX(core, root, packet | 0x80000000u, dx);
  }
}

template <std::size_t Index> void drawAnchored(Core *core) {
  constexpr Element element = kElements[Index];
  Core &c = *core;
  const std::uint32_t first = c.mem_r32(PacketPool::kCursor) & 0x00FFFFFFu;
  psx::cpu::callOriginalResumingToReturn(c, element.name, element.address, c.r[kRa]);
  const psx::present::HorizontalMargins edges = psx::present::frameWindows(c).drawn;
  if (edges.total() == 0) {
    return;
  }
  const std::uint32_t end = c.mem_r32(PacketPool::kCursor) & 0x00FFFFFFu;
  const Columns columns = drawnColumns(c, first, end);
  if (columns.last <= kCentreColumn) {
    movePackets(c, first, end, -edges.left);
  } else if (columns.first >= kCentreColumn) {
    movePackets(c, first, end, edges.right);
  }
}

template <std::size_t... Index> bool installAll(const InstallOverride &install, std::index_sequence<Index...>) {
  return (install(kElements[Index].address, kElements[Index].name, drawAnchored<Index>) && ...);
}

} // namespace

bool installHudAnchor(const InstallOverride &install) {
  return installAll(install, std::make_index_sequence<kElements.size()>{});
}

} // namespace tomba1
