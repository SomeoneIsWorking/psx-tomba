#include "tomba1/game/assets/title_art.h"

#include "tomba1/game/assets/gam_file.h"

#include "disc_file.h"

#include <lucent/log.h>

#include <array>
#include <cstring>
#include <utility>
#include <vector>

namespace tomba1 {
namespace {

constexpr int kPictureWidth = 320;
constexpr int kPictureHeight = 240;

// A file holding one VRAM rectangle's halfwords, GAM-packed or raw.
struct FileImage {
  const char *path;
  psx::gpu::VramRect rect;
  bool packed;
};

// Background (A00010.GAM, palette A00011), logo (A00012, palette A00013).
constexpr FileImage kImages[] = {
    {"/SYSTEM/A00010.GAM;1", {0, 0, 256, 256}, true},
    {"/SYSTEM/A00011.000;1", {128, 511, 256, 1}, false},
    {"/SYSTEM/A00012.000;1", {256, 0, 128, 256}, false},
    {"/SYSTEM/A00013.000;1", {128, 510, 256, 1}, false},
};

constexpr std::uint16_t clutWord(int x, int y) {
  return static_cast<std::uint16_t>((y << 6) | (x / 16));
}

// OPTSUB00's background list 0x800E9E44, then its logo list 0x800E9E14 at rest, as FUN_800E73C0 draws them.
constexpr std::array<psx::present::ArtSprite, 4> kSprites{{
    {.x = 0, .y = 0, .width = 256, .height = 256, .texPage = 0x80, .clut = clutWord(128, 511)},
    {.x = 256, .y = 0, .width = 64, .height = 256, .texPage = 0x82, .clut = clutWord(128, 511)},
    {.x = 4, .y = 24, .width = 256, .height = 112, .texPage = 0x84, .clut = clutWord(128, 510)},
    {.x = 252, .y = 48, .u = 8, .v = 112, .width = 72, .height = 88, .texPage = 0x84, .clut = clutWord(128, 510)},
}};

} // namespace

std::optional<psx::present::ArtScene> titleArt(psx::cd::Disc &disc) {
  psx::present::ArtScene scene{.canvas = {512, 0, kPictureWidth, kPictureHeight}};
  for (const FileImage &image : kImages) {
    std::vector<std::uint8_t> file;
    if (!psx::cd::readDiscFile(disc, image.path, file)) {
      lucent::warn("picker", "the disc does not hold {}", image.path);
      return std::nullopt;
    }
    if (image.packed) {
      std::optional<std::vector<std::uint8_t>> unpacked = unpackGam(file);
      if (!unpacked) {
        lucent::warn("picker", "{} does not unpack", image.path);
        return std::nullopt;
      }
      file = std::move(*unpacked);
    }
    const std::size_t words = static_cast<std::size_t>(image.rect.width) * image.rect.height;
    if (file.size() != words * 2) {
      lucent::warn("picker",
                   "{} holds {} bytes, not a {}x{} image",
                   image.path,
                   file.size(),
                   image.rect.width,
                   image.rect.height);
      return std::nullopt;
    }
    psx::present::ArtUpload upload{.rect = image.rect, .words = std::vector<std::uint16_t>(words)};
    std::memcpy(upload.words.data(), file.data(), file.size());
    scene.uploads.push_back(std::move(upload));
  }
  scene.sprites.assign(kSprites.begin(), kSprites.end());
  return scene;
}

} // namespace tomba1
