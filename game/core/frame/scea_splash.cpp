#include "frame/scea_splash.h"

#include "frame/scea_asset.h"
#include "vram_pixel.h"

#include <cstddef>

namespace tomba {
namespace {

// The three sprites the boot stub draws: screen position, size and texpage UV.
struct SpriteRect {
  int screenX, screenY, width, height, u0, v0;
};

constexpr SpriteRect kRects[3] = {{536, 200, 64, 32, 0, 128}, {280, 200, 256, 64, 0, 64}, {24, 200, 256, 64, 0, 0}};

constexpr bool rectsFit() {
  for (const SpriteRect &rect : kRects) {
    if ((rect.u0 + rect.width) / 4 > kSceaTextureWords || rect.v0 + rect.height > kSceaTextureRows ||
        rect.screenX + rect.width > kSceaSplashWidth || rect.screenY + rect.height > kSceaSplashHeight) {
      return false;
    }
  }
  return true;
}
static_assert(rectsFit());

// The page at VRAM (0, 0) and the CLUT on the row below it.
constexpr psx::gpu::TexPage kPage{0, 0, 0, psx::gpu::TexDepth::Indexed4};
constexpr psx::gpu::VramPoint kClut{0, kSceaTextureRows};

std::uint16_t halfWordAt(psx::gpu::VramPoint at) {
  if (at.y == kClut.y) {
    return kSceaClut[static_cast<std::size_t>(at.x)];
  }
  return kSceaTexture[static_cast<std::size_t>(at.y * kSceaTextureWords + at.x)];
}

} // namespace

std::vector<std::uint8_t> decodeSceaSplash() {
  std::vector<std::uint8_t> out(static_cast<std::size_t>(kSceaSplashWidth) * kSceaSplashHeight * 4, 0);
  for (const SpriteRect &rect : kRects) {
    for (int row = 0; row < rect.height; row++) {
      for (int column = 0; column < rect.width; column++) {
        const std::uint16_t colour = psx::gpu::fetchTexel(halfWordAt, kPage, kClut, rect.u0 + column, rect.v0 + row);
        if (colour == 0) {
          continue;
        }
        const psx::gpu::VramPixel pixel = psx::gpu::decodeVramPixel(colour);
        const std::size_t at = static_cast<std::size_t>(rect.screenY + row) * kSceaSplashWidth + rect.screenX + column;
        std::uint8_t *rgba = out.data() + at * 4;
        rgba[0] = static_cast<std::uint8_t>(psx::gpu::scale5to8(pixel.red));
        rgba[1] = static_cast<std::uint8_t>(psx::gpu::scale5to8(pixel.green));
        rgba[2] = static_cast<std::uint8_t>(psx::gpu::scale5to8(pixel.blue));
        rgba[3] = 255;
      }
    }
  }
  return out;
}

} // namespace tomba
