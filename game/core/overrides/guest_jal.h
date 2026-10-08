#pragma once

#include "core.h"
#include "guest_call.h"

#include <array>
#include <cstdint>
#include <span>

namespace tomba::guest {

template <typename... Arguments>
std::uint32_t
dispatchJalToReturn(Core &core, std::uint32_t address, std::uint32_t returnAddress, Arguments... arguments) {
  static_assert(sizeof...(Arguments) <= 4, "PSX register-call boundary supports a0..a3");
  const std::array<std::uint32_t, sizeof...(Arguments)> values{static_cast<std::uint32_t>(arguments)...};
  core.r[31] = returnAddress;
  psx::cpu::dispatchGuestWithArgumentsToReturn(
      core, address, std::span<const std::uint32_t>{values}, psx::cpu::ExecutionBudget::currentTurn(core), __func__);
  return core.r[2];
}

template <typename... Arguments>
std::uint32_t dispatchLeafToReturn(Core &core, std::uint32_t address, Arguments... arguments) {
  static_assert(sizeof...(Arguments) <= 4, "PSX register-call boundary supports a0..a3");
  const std::array<std::uint32_t, sizeof...(Arguments)> values{static_cast<std::uint32_t>(arguments)...};
  psx::cpu::dispatchGuestWithArgumentsToReturn(
      core, address, std::span<const std::uint32_t>{values}, psx::cpu::ExecutionBudget::currentTurn(core), __func__);
  return core.r[2];
}

} // namespace tomba::guest
