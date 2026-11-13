#include "hud_anchor.h"

#include "core.h"
#include "guest_signed.h"
#include "guest_widescreen_projection.h"
#include "packet_pool.h"
#include "r3000.h"
#include "resumable_guest_call.h"

#include <array>
#include <cstddef>

namespace tomba1 {
namespace {

using psx::cpu::kA1;
using psx::cpu::kA2;
using psx::cpu::kRa;

// The flip 0x80016940 clears the table backwards (ClearOTagR, 0x328 entries) and draws it from its last entry.
constexpr std::uint32_t kOrderingTable = 0x1F8001E0u;
constexpr std::uint32_t kWalkStart = 0xC9Cu;
// An anchor left of the picture's centre column belongs to the left edge.
constexpr std::int32_t kCentreColumn = 160;

// One element drawer the HUD pass calls and the register holding its anchor x.
struct Element {
  std::uint32_t address = 0;
  const char *name = nullptr;
  std::uint32_t anchorRegister = 0;
};

constexpr std::array kElements{
    Element{.address = 0x8004E590u, .name = "HudAnchor::lives", .anchorRegister = kA1},
    Element{.address = 0x8004E714u, .name = "HudAnchor::points", .anchorRegister = kA1},
    Element{.address = 0x8004E900u, .name = "HudAnchor::vitality", .anchorRegister = kA1},
    Element{.address = 0x8004EB10u, .name = "HudAnchor::item", .anchorRegister = kA2},
    Element{.address = 0x8004ED80u, .name = "HudAnchor::weapon", .anchorRegister = kA1},
};

// Each packet the element added to the pool, [first, end), moved `dx` columns in the walk that draws them.
void movePackets(Core &core, std::uint32_t first, std::uint32_t end, int dx) {
  const std::uint32_t root = core.mem_r32(kOrderingTable) + kWalkStart;
  for (std::uint32_t packet = first; packet < end; packet += (core.mem_r8(packet + 3u) + 1u) * 4u) {
    core.hostPackets.moveX(core, root, packet | 0x80000000u, dx);
  }
}

template <std::size_t Index> void drawAnchored(Core *core) {
  constexpr Element element = kElements[Index];
  Core &c = *core;
  const auto anchor = static_cast<std::int32_t>(sext16(c.r[element.anchorRegister]));
  const std::uint32_t first = c.mem_r32(PacketPool::kCursor) & 0x00FFFFFFu;
  psx::cpu::callOriginalResumingToReturn(c, element.name, element.address, c.r[kRa]);
  const psx::present::HorizontalMargins edges = psx::present::frameWindows(c).drawn;
  if (edges.total() == 0) {
    return;
  }
  movePackets(
      c, first, c.mem_r32(PacketPool::kCursor) & 0x00FFFFFFu, anchor < kCentreColumn ? -edges.left : edges.right);
}

template <std::size_t... Index> bool installAll(const InstallOverride &install, std::index_sequence<Index...>) {
  return (install(kElements[Index].address, kElements[Index].name, drawAnchored<Index>) && ...);
}

} // namespace

bool installHudAnchor(const InstallOverride &install) {
  return installAll(install, std::make_index_sequence<kElements.size()>{});
}

} // namespace tomba1
