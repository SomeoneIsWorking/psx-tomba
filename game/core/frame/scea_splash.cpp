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

} // namespace

std::vector<std::uint8_t> decodeSceaSplash() {
  std::vector<std::uint8_t> out(static_cast<std::size_t>(kSceaSplashWidth) * kSceaSplashHeight * 4, 0);
  for (const SpriteRect &rect : kRects) {
    for (int row = 0; row < rect.height; row++) {
      for (int column = 0; column < rect.width; column++) {
        const int u = rect.u0 + column;
        const int v = rect.v0 + row;
        // 4bpp: one halfword holds four texels, selected by u & 3.
        const int wordColumn = u >> 2;
        if (wordColumn >= kSceaTextureWords || v >= kSceaTextureRows) {
          continue;
        }
        const std::uint16_t word = kSceaTexture[static_cast<std::size_t>(v * kSceaTextureWords + wordColumn)];
        const std::uint16_t colour = kSceaClut[(word >> ((u & 3) * 4)) & 0xFu];
        if (colour == 0) {
          continue; // a 0x0000 CLUT entry is transparent
        }
        const int x = rect.screenX + column;
        const int y = rect.screenY + row;
        if (x >= kSceaSplashWidth || y >= kSceaSplashHeight) {
          continue;
        }
        const psx::gpu::VramPixel pixel = psx::gpu::decodeVramPixel(colour);
        std::uint8_t *rgba = out.data() + (static_cast<std::size_t>(y) * kSceaSplashWidth + x) * 4;
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
