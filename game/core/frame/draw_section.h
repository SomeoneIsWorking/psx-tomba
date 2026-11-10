// draw_section.h — the field draw M:8003F9A8 or the room draw M:8003FA44 as a replay section.
#pragma once

#include "replay_frame.h"

#include <cstdint>
#include <optional>

namespace tomba {

class DrawSection final : public psx::present::ReplaySection {
public:
  enum class Variant : std::uint8_t {
    Field, // 0x8003F9A8
    Room,  // 0x8003FA44: no scenery walker or tile grid
  };

  // The variant the frame is about to draw, replayed until the next.
  void select(Variant variant) {
    variant_ = variant;
  }

  // No preamble: the env, its OT and the packet pool are already the frame's.
  [[nodiscard]] std::optional<std::uint32_t> draw(Core &core, psx::present::GuestRedraw &redraw) const override;
  [[nodiscard]] bool linked(std::uint32_t head) const override;

private:
  Variant variant_ = Variant::Field;
};

} // namespace tomba
