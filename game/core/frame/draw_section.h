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
    Sop,   // Sop::fieldUpdate's draw tail (Sop::kSceneryDraw, kListDraw, kGridDraw)
    Title, // the title sub-machines' Demo::kTitleEdges and kTitleItems
    Page,  // GAME's page step G:8010810C alone, when 0x1F800136 >= 2 holds the field draw off (a menu page)
  };

  // What the frame is about to draw, replayed until the next: every GAME caller follows the field or room draw with
  // the page step, the DEMO attract frame does not (issue 0055).
  struct Selection {
    Variant variant = Variant::Field;
    bool pageStep = false;
    bool grid = false;      // Sop: the beat draws the parallax grid
    bool scroller = false;  // Sop: the text scroller follows the draw
    std::uint32_t menu = 0; // Title: kTitleItems' a0 and a1
    std::uint32_t item = 0;
    std::uint8_t page = 0; // Page: the stage task's page byte +0x6B
    bool operator==(const Selection &) const = default;
  };
  // GAME and DEMO share the stage slot, so GAME holding the page step means a GAME frame is drawing.
  [[nodiscard]] static bool pageStepResident(const Core &core);
  // G:8010810C, after either draw in GAME, or alone on a menu page.
  static constexpr std::uint32_t kPageStep = 0x8010810Cu;
  void select(Selection selection) {
    selection_ = selection;
  }

  // No preamble: the env, its OT and the packet pool are already the frame's.
  [[nodiscard]] std::optional<std::uint32_t> draw(Core &core, psx::present::GuestRedraw &redraw) const override;
  [[nodiscard]] bool linked(std::uint32_t head) const override;

private:
  // The selected variant's steps; false when one was refused.
  [[nodiscard]] bool drawSteps(Core &core, psx::present::GuestRedraw &redraw) const;

  Selection selection_;
};

} // namespace tomba
