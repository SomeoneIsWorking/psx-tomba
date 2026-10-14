#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace tomba::test {

struct ImageRequest {
  std::string_view path;
  std::string_view sha256;
  std::size_t maxBytes;
};
// A diagnostic must authenticate the exact buffer it passes to the shipping loader.
std::optional<std::vector<std::uint8_t>> readAuthenticatedImage(const ImageRequest &request);

} // namespace tomba::test
