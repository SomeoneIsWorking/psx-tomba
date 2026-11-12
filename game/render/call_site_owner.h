// game/render/call_site_owner.h — the guest object an update-block 2D call draws for, read from its caller's registers.
#pragma once

#include "core.h"

#include <cstdint>
#include <optional>
#include <span>

namespace tomba::render {

// A writer's call site and the caller registers that hold, at the call, the object it draws and its element.
struct CallSiteOwner {
  static constexpr std::uint8_t kNone = 0;
  std::uint32_t returnAddress = 0;
  std::uint8_t object = kNone;  // register holding the object's address
  std::uint8_t element = kNone; // register holding the caller's loop index over the object's parts
};

// `core`'s call by its return address in `sites`: the site, the object's main-RAM word and the element; nullopt at a
// site the table does not know.
inline std::optional<std::uint32_t> callSiteOwner(const Core &core, std::span<const CallSiteOwner> sites) {
  constexpr std::uint32_t kRamWords = 0x1FFFFCu;
  constexpr std::uint32_t kElementShift = 19u;
  constexpr std::uint32_t kElementMask = 0x1Fu;
  constexpr std::uint32_t kSiteShift = 24u;
  const std::uint32_t returnAddress = core.r[31];
  for (std::uint32_t site = 0; site < sites.size(); ++site) {
    const CallSiteOwner &owner = sites[site];
    if (owner.returnAddress != returnAddress) {
      continue;
    }
    const std::uint32_t object = owner.object == CallSiteOwner::kNone ? 0u : (core.r[owner.object] & kRamWords) >> 2;
    const std::uint32_t element = owner.element == CallSiteOwner::kNone ? 0u : core.r[owner.element] & kElementMask;
    return (site << kSiteShift) | (element << kElementShift) | object;
  }
  return std::nullopt;
}

} // namespace tomba::render
