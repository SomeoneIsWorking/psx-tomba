#pragma once

#include "core.h"

#include <cstdint>

namespace tomba {

// Continue the current scheduler-owned guest task at a specific address after
// a native override returns. PSXPort's dynamic executor consumes this request
// at the next guest boundary.
inline void requestGuestContinuation(Core &core, std::uint32_t address) {
  core.pending_guest_redirect = address;
}

} // namespace tomba
