// draw_motion.h — what the field draw reads that moves between logic frames: the view, Tomba, every object node,
// every render record's world transform and part angles, and the effect table (docs/plans/replay-interpolation.md).
#pragma once

#include "motion_set.h"
#include "world/render_record_pool.h"

#include <array>
#include <cstdint>
#include <span>

namespace tomba {

// A render record's life: despawn pushes it back on the free stack, so the next spawn is another object.
class RecordLives final : public psx::present::MotionIdentity {
public:
  explicit RecordLives(const world::RenderRecordPool &pool) : pool_(pool) {}
  [[nodiscard]] std::uint32_t generation(std::uint32_t record) const override {
    return pool_.object(record);
  }

private:
  const world::RenderRecordPool &pool_;
};

class DrawMotion {
public:
  explicit DrawMotion(const world::RenderRecordPool &records);
  DrawMotion(const DrawMotion &) = delete;
  DrawMotion &operator=(const DrawMotion &) = delete;
  DrawMotion(DrawMotion &&) = delete;
  DrawMotion &operator=(DrawMotion &&) = delete;

  [[nodiscard]] std::span<const psx::present::MotionTrack> tracks() const {
    return tracks_;
  }

private:
  static constexpr std::size_t kTracks = 11;

  RecordLives lives_;
  psx::present::TransformRule transform_;
  std::array<psx::present::MotionTrack, kTracks> tracks_;
};

} // namespace tomba
