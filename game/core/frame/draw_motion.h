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

// Tomba's pan +0x14E, a 12-bit angle FUN_800597AC turns him by only while +0x177 bit 0 is set, and by 0 otherwise.
class TombaPan final : public psx::present::MotionRule {
public:
  void blend(const Samples &samples, float t, std::span<std::uint32_t> out) const override;
};

// The view the camera's look-at FUN_8006D02C built each tick, and how far the shake tail FUN_8006C988 had moved the eye
// it read from the shake anchor cam+0x86/88/8A. The tail moves the eye after the tick's look-at, so a drawn view
// carries the previous tick's jitter: a shaken interval's in-between is rebuilt through the same look-at at the blended
// unshaken eye plus the later tick's offset, a shake that tick drew rather than a blend of two.
class ShakenView final : public psx::present::MotionSource {
public:
  // Whole units of the eye X, Y and Z.
  using Units = std::array<std::int16_t, 3>;
  // The shake tail moved the eye to `eye`, `offset` from its anchor.
  void jittered(const Units &eye, const Units &offset);
  // The camera's look-at built the view from the eye and target words.
  void built(Core &core);

  [[nodiscard]] const char *name() const override {
    return "shaken view";
  }
  void sample(Core &core) override;
  psx::present::MotionCount write(psx::present::GuestRedraw &redraw, float t) const override;

private:
  static constexpr std::size_t kInputs = 6; // eye X, Y, Z and target X, Y, Z, 16.16
  using Inputs = std::array<std::uint32_t, kInputs>;
  using Words = std::array<std::uint32_t, psx::present::TransformRule::kWords>;
  struct Jitter {
    Units eye{};
    Units offset{};
  };
  struct Build {
    Inputs inputs{};
    Units offset{};
    Words view{};
  };
  // A drawn tick: its look-at's build, what moved the drawn translation from it after (FUN_8006E0F0's dolly) and the
  // eye and target words the draw read.
  struct Tick {
    Build build;
    std::array<std::int32_t, 3> after{};
    Inputs drawnInputs{};
  };

  std::optional<Jitter> jitter_;
  std::optional<Build> build_; // since the last sample
  std::optional<Tick> earlier_;
  std::optional<Tick> later_;
  Core *core_ = nullptr;
};

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
  [[nodiscard]] ShakenView &shakenView() {
    return shakenView_;
  }
  // How far past its scroll words the replay at `t` scrolls the tile grid, X and Y, while the grid pairs.
  [[nodiscard]] std::optional<std::array<float, 2>> gridScrollPast(const psx::present::MotionSet &motion,
                                                                   float t) const;

private:
  static constexpr std::size_t kTracks = 22;

  psx::present::TransformRule transform_;
  TombaPan tombaPan_;
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
  ShakenView shakenView_;
  const psx::present::MotionTrack *tileGrid_ = nullptr;
};

} // namespace tomba
