#pragma once

#include "core.h"
#include "guest_call.h"
#include "resumable_guest_call.h"

#include <cstdint>
#include <string_view>

namespace tomba::guest {

// A guest `jal`: the callee and the $ra the guest's own call site leaves.
struct GuestJal {
  std::uint32_t target;
  std::uint32_t returnAddress;
};

template <typename... Arguments> std::uint32_t dispatchJalToReturn(Core &core, GuestJal jal, Arguments... arguments) {
  core.r[31] = jal.returnAddress;
  return psx::cpu::callGuestNowResult(core, "guest jal", jal.target, arguments...);
}

// A guest handler whose work can exceed one display field (the area handlers' first state clears the
// object pools): it is resumed across host turns until it returns to the caller's $ra.
inline std::uint32_t dispatchHandlerToReturnResuming(Core &core, std::uint32_t address, std::string_view owner) {
  return psx::cpu::callGuestToReturnResuming(core, owner, address, core.r[31]);
}

} // namespace tomba::guest
