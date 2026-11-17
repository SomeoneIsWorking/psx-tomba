#include "hud_anchor.h"

#include "core.h"
#include "hud_anchor_packets.h"
#include "packet_pool.h"
#include "r3000.h"
#include "resumable_guest_call.h"

#include <array>
#include <cstddef>

namespace tomba1 {
namespace {

using psx::cpu::kRa;

// The flip 0x80016940 clears the table backwards (ClearOTagR, 0x328 entries) and draws it from its last entry.
constexpr std::uint32_t kOrderingTable = 0x1F8001E0u;
constexpr std::uint32_t kWalkStart = 0xC9Cu;
// The picture's centre column.
constexpr int kCentreColumn = 160;

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

template <std::size_t Index> void drawAnchored(Core *core) {
  constexpr Element element = kElements[Index];
  Core &c = *core;
  const std::uint32_t first = c.mem_r32(PacketPool::kCursor);
  psx::cpu::callOriginalResumingToReturn(c, element.name, element.address, c.r[kRa]);
  psx::present::anchorHudElement(c,
                                 c.mem_r32(kOrderingTable) + kWalkStart,
                                 {.first = first, .end = c.mem_r32(PacketPool::kCursor)},
                                 kCentreColumn);
}

template <std::size_t... Index> bool installAll(const InstallOverride &install, std::index_sequence<Index...>) {
  return (install(kElements[Index].address, kElements[Index].name, drawAnchored<Index>) && ...);
}

} // namespace

bool installHudAnchor(const InstallOverride &install) {
  return installAll(install, std::make_index_sequence<kElements.size()>{});
}

} // namespace tomba1
