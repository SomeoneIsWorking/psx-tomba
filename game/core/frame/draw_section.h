// draw_section.h — the field draw M:8003F9A8 or the room draw M:8003FA44, then in GAME the page step G:8010810C,
// as a replay section.
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

  // What the frame is about to draw, replayed until the next: GAME's frames follow the draw with the page step,
  // the DEMO attract frame does not.
  struct Selection {
    Variant variant = Variant::Field;
    bool pageStep = true;
    bool operator==(const Selection &) const = default;
  };
  void select(Selection selection) {
    selection_ = selection;
  }

  // No preamble: the env, its OT and the packet pool are already the frame's.
  [[nodiscard]] std::optional<std::uint32_t> draw(Core &core, psx::present::GuestRedraw &redraw) const override;
  [[nodiscard]] bool linked(std::uint32_t head) const override;

private:
  Selection selection_;
};

} // namespace tomba
