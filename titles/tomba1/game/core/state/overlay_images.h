// The area overlays the native code knows, by the content identity of their landed read.
#pragma once

#include <cstdint>

namespace tomba1 {

// Area 2 (LBA 4137, 0xD800 bytes at 0x800E7388), SHA-256 0b789d0875eeb235...
inline constexpr std::uint64_t kAreaTwoOverlay = 0x0B789D0875EEB235ull;
// Area 2's painted rooms after the first (LBA 4184, 0x38000 bytes), SHA-256 d3143cfbf82dc86b...
inline constexpr std::uint64_t kAreaTwoRoomsOverlay = 0xD3143CFBF82DC86Bull;
// Area 4 (LBA 7125, 0x4E000 bytes at 0x800E7388), SHA-256 e12a3c27e1dd9ddc...
inline constexpr std::uint64_t kAreaFourOverlay = 0xE12A3C27E1DD9DDCull;
// Area 5 (LBA 8583, 0x21800 bytes at 0x800E7388), SHA-256 a1c1865fa556415b...
inline constexpr std::uint64_t kAreaFiveOverlay = 0xA1C1865FA556415Bull;
// Area 5's sections 1 and 3 (LBA 8650, 0x32800 bytes), SHA-256 3209323ec40e1e74...
inline constexpr std::uint64_t kAreaFiveRoomsOverlay = 0x3209323EC40E1E74ull;

} // namespace tomba1
