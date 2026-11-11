// image_residency.h — whether a named image (GAME, an area overlay) holds a guest address now.
#pragma once

#include "motion_set.h"

#include <cstdint>
#include <string_view>

class Core;

namespace tomba {

[[nodiscard]] bool imageHolds(const Core &core, std::uint32_t address, std::string_view image);

// A track's records hold an object only while `image` holds `code`: an overlay's words mean nothing in another area.
class ResidentImage final : public psx::present::MotionLiveness {
public:
  constexpr ResidentImage(std::string_view image, std::uint32_t code) : image_(image), code_(code) {}
  void sample(Core &core) override;
  [[nodiscard]] bool live(std::uint32_t) const override {
    return resident_;
  }

private:
  std::string_view image_;
  std::uint32_t code_;
  bool resident_ = false;
};

} // namespace tomba
