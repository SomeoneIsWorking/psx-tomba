// A .GAM file: "GAM\0", the unpacked length, then a byte LZ (tomba1-decomp docs/notes/title-screen.md).
#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace tomba1 {

// The unpacked bytes; nullopt for a file that is not a GAM or does not unpack to its length.
std::optional<std::vector<std::uint8_t>> unpackGam(std::span<const std::uint8_t> file);

} // namespace tomba1
