// draw_motion.h — the guest words the field draw reads that move between logic frames.
#pragma once

#include "motion_set.h"
#include "world/render_record_pool.h"

#include <array>
#include <cstdint>
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

private:
  static constexpr std::size_t kTracks = 11;

  psx::present::TransformRule transform_;
  std::array<psx::present::MotionTrack, kTracks> tracks_;
};

} // namespace tomba
