// game/render/wave_phases.h — the screen-wave phase each model list read in the last real draw (A0D), so a replay
// places the wave's whole-row offsets t of the way from that draw's to its own.
#pragma once

#include "motion_set.h"

#include <cstdint>
#include <optional>
#include <unordered_map>

class Core;

namespace tomba::render {

class WavePhases final : public psx::present::MotionSource {
public:
  // A real draw's list `list` read `phase`.
  void note(std::uint32_t list, std::uint32_t phase);
  // The phase `list` read in the real draw before the one the replay redraws, while it read only one.
  [[nodiscard]] std::optional<std::uint32_t> earlier(std::uint32_t list) const;

  [[nodiscard]] const char *name() const override {
    return "screen wave";
  }
  void sample(Core &core) override;
  psx::present::MotionCount write(psx::present::GuestRedraw &redraw, float t) const override;

private:
  struct Read {
    std::uint32_t phase = 0;
    bool single = true; // false once the list read two phases in one draw
  };
  std::unordered_map<std::uint32_t, Read> noting_;
  std::unordered_map<std::uint32_t, Read> earlier_;
  std::size_t unpaired_ = 0; // lists the last draw read that the one before did not, or read twice
};

} // namespace tomba::render
