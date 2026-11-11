// draw_motion.h — the guest words the field draw reads that move between logic frames.
#pragma once

#include "frame/image_residency.h"
#include "motion_set.h"
#include "world/render_record_pool.h"

#include <array>
#include <cstdint>
#include <optional>
#include <span>

namespace tomba {

class DrawMotion {
public:
  explicit DrawMotion(world::RenderRecordPool &records);
  DrawMotion(const DrawMotion &) = delete;
  DrawMotion &operator=(const DrawMotion &) = delete;
  DrawMotion(DrawMotion &&) = delete;
  DrawMotion &operator=(DrawMotion &&) = delete;

  [[nodiscard]] std::span<const psx::present::MotionTrack> tracks() const {
    return tracks_;
  }
  // How far past its scroll words the replay at `t` scrolls the tile grid, X and Y, while the grid pairs.
  [[nodiscard]] std::optional<std::array<float, 2>> gridScrollPast(const psx::present::MotionSet &motion,
                                                                   float t) const;

private:
  static constexpr std::size_t kTracks = 21;

  psx::present::TransformRule transform_;
  psx::present::TransformRule view_{psx::present::TransformRule::Rows::Rotation,
                                    psx::present::TransformRule::Translation::View};
  psx::present::RingRule gridScroll_;
  psx::present::RingRule a02Scroll_{{0x100u, 0x180u}};
  psx::present::RingRule a02Band_{{0x800u, 0u}};
  psx::present::RingRule a06Scroll_{{0x100u, 0u}};
  psx::present::RingRule a0fScroll_{{0u, 0x40u}};
  // Each by the scenery walker or emitter that reads the overlay's words.
  ResidentImage a01_{"A01", 0x80132358u};
  ResidentImage a02_{"A02", 0x80124DB8u};
  ResidentImage a06_{"A06", 0x8013CF00u};
  ResidentImage a0f_{"A0F", 0x80115364u};
  std::array<psx::present::MotionTrack, kTracks> tracks_;
  const psx::present::MotionTrack *tileGrid_ = nullptr;
};

} // namespace tomba
