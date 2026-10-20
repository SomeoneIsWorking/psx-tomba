// scea_splash.h — the SCEA license screen as a host image, decoded from scea_asset.h.
#pragma once

#include <cstdint>
#include <vector>

namespace tomba {

inline constexpr int kSceaSplashWidth = 640;
inline constexpr int kSceaSplashHeight = 468;

// RGBA8, kSceaSplashWidth x kSceaSplashHeight: text pixels in their CLUT colour, everything else transparent black.
std::vector<std::uint8_t> decodeSceaSplash();

} // namespace tomba
