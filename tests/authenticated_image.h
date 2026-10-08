#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace tomba::test {

// A diagnostic must authenticate the exact buffer it passes to the shipping loader.
std::optional<std::vector<std::uint8_t>>
readAuthenticatedImage(std::string_view path, std::string_view expectedSha, std::size_t maxBytes);

} // namespace tomba::test
