#include "authenticated_image.h"

#include <fstream>
#include <lucent/content.h>
#include <lucent/log.h>
#include <span>
#include <string>

namespace tomba::test {

std::optional<std::vector<std::uint8_t>>
readAuthenticatedImage(std::string_view path, std::string_view expectedSha, std::size_t maxBytes) {
  std::ifstream input(std::string(path), std::ios::binary | std::ios::ate);
  if (!input) {
    lucent::error("authentic-image-test", "cannot open image {}", path);
    return std::nullopt;
  }
  auto length = input.tellg();
  if (length <= 0 || length > static_cast<std::streamoff>(maxBytes)) {
    lucent::error("authentic-image-test", "image {} has unreadable or out-of-bounds size", path);
    return std::nullopt;
  }
  std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
  input.seekg(0);
  input.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
  if (!input || input.peek() != std::ifstream::traits_type::eof()) {
    lucent::error("authentic-image-test", "image {} changed size or could not be read completely", path);
    return std::nullopt;
  }
  auto digest = lucent::content::sha256(std::as_bytes(std::span{bytes}));
  if (lucent::content::sha256_hex(digest) != expectedSha) {
    lucent::error("authentic-image-test", "image {} differs from the manifest digest passed by the verifier", path);
    return std::nullopt;
  }
  return bytes;
}

} // namespace tomba::test
