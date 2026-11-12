// The area overlays native overrides are bound to, by the content identity of their landed read.
#pragma once

#include <cstdint>

namespace tomba1 {

// Area 4 (LBA 7125, 0x4E000 bytes at 0x800E7388), SHA-256 e12a3c27e1dd9ddc...
inline constexpr std::uint64_t kAreaFourOverlay = 0xE12A3C27E1DD9DDCull;

} // namespace tomba1
