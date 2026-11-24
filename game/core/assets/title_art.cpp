#include "assets/title_art.h"

#include "assets/lz_image.h"
#include "disc_file.h"

#include <lucent/log.h>

#include <cstdint>
#include <cstring>
#include <span>
#include <vector>

namespace tomba {
namespace {

constexpr const char *kGroupHeaders = "/CD/TOMBA2.IDX";
constexpr const char *kGroupArchive = "/CD/TOMBA2.IMG";
constexpr std::uint32_t kHeaderBytes = 0x800;
constexpr std::uint32_t kTitleGroup = 2;
// An archive's images follow its entry table.
constexpr std::uint32_t kImagesStart = 0x800;
constexpr std::uint32_t kEntryBytes = 12;

// The picture: the left 320 x 240 of the 8-bit image at (640, 256), through the palette on row 511.
constexpr int kPictureWidth = 320;
constexpr int kPictureHeight = 240;
constexpr std::uint16_t kPalette = (511u << 6) | (640u / 16u);
constexpr std::uint16_t kLeftPage = (640u / 64u) | (1u << 4) | (1u << 7);
constexpr std::uint16_t kRightPage = (768u / 64u) | (1u << 4) | (1u << 7);

template <class T> T readLe(std::span<const std::uint8_t> bytes, std::size_t at) {
  T value{};
  std::memcpy(&value, bytes.data() + at, sizeof value);
  return value;
}

struct BufferSource {
  std::span<const std::uint8_t> bytes;
  std::size_t at = 0;
  bool more() const {
    return at < bytes.size();
  }
  std::uint8_t next() {
    return bytes[at++];
  }
};

// A back-reference before the image's first byte is a broken stream.
struct BufferOutput {
  std::vector<std::uint8_t> bytes;
  std::int64_t size() const {
    return static_cast<std::int64_t>(bytes.size());
  }
  std::optional<std::uint8_t> at(std::int64_t index) const {
    if (index < 0 || index >= size()) {
      return std::nullopt;
    }
    return bytes[static_cast<std::size_t>(index)];
  }
  void push(std::uint8_t byte) {
    bytes.push_back(byte);
  }
};

} // namespace

std::optional<psx::present::ArtScene> titleArt(psx::cd::Disc &disc) {
  std::vector<std::uint8_t> header;
  if (!psx::cd::readDiscFileRange(disc, kGroupHeaders, kTitleGroup * kHeaderBytes, 8, header)) {
    lucent::warn("picker", "{} holds no header for texture group {}", kGroupHeaders, kTitleGroup);
    return std::nullopt;
  }
  const std::uint32_t start = readLe<std::uint32_t>(header, 0);
  const std::uint32_t end = readLe<std::uint32_t>(header, 4);
  std::vector<std::uint8_t> group;
  if (end <= start || !psx::cd::readDiscFileRange(disc, kGroupArchive, start, end - start, group) ||
      group.size() < kImagesStart) {
    lucent::warn("picker", "{} does not hold texture group {}", kGroupArchive, kTitleGroup);
    return std::nullopt;
  }
  const std::span<const std::uint8_t> bytes(group);
  const std::int32_t count = readLe<std::int32_t>(bytes, 0);
  psx::present::ArtScene scene{.canvas = {0, 0, kPictureWidth, kPictureHeight}};
  std::size_t packed = kImagesStart;
  for (std::int32_t image = 0; image < count; ++image) {
    const std::size_t entry = 4 + (static_cast<std::size_t>(image) * kEntryBytes);
    if (entry + kEntryBytes > kImagesStart) {
      lucent::warn("picker", "texture group {} lists {} images, past its table", kTitleGroup, count);
      return std::nullopt;
    }
    const psx::gpu::VramRect rect{readLe<std::int16_t>(bytes, entry),
                                  readLe<std::int16_t>(bytes, entry + 2),
                                  readLe<std::int16_t>(bytes, entry + 4),
                                  readLe<std::int16_t>(bytes, entry + 6)};
    const std::uint32_t length = readLe<std::uint32_t>(bytes, entry + 8);
    if (packed + length > bytes.size() || rect.width <= 0 || rect.height <= 0) {
      lucent::warn("picker", "texture group {} image {} runs past the group", kTitleGroup, image);
      return std::nullopt;
    }
    BufferSource source{bytes.subspan(packed, length)};
    BufferOutput output;
    const std::size_t wanted = static_cast<std::size_t>(rect.width) * rect.height * 2;
    if (!decodeLzImage(source, output, rect.width) || output.bytes.size() < wanted) {
      lucent::warn("picker", "texture group {} image {} does not unpack", kTitleGroup, image);
      return std::nullopt;
    }
    psx::present::ArtUpload upload{.rect = rect, .words = std::vector<std::uint16_t>(wanted / 2)};
    std::memcpy(upload.words.data(), output.bytes.data(), wanted);
    scene.uploads.push_back(std::move(upload));
    packed += length;
  }
  // A sprite reads at most one page; the picture spans two.
  scene.sprites = {{.x = 0, .y = 0, .width = 256, .height = kPictureHeight, .texPage = kLeftPage, .clut = kPalette},
                   {.x = 256,
                    .y = 0,
                    .width = kPictureWidth - 256,
                    .height = kPictureHeight,
                    .texPage = kRightPage,
                    .clut = kPalette}};
  return scene;
}

} // namespace tomba
