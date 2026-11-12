// draw_section.h — the draw a frame replays: the field or room draw and GAME's page step, a menu page, A03's ride,
// SOP, its narration or the DEMO title, as a replay section.
#pragma once

#include "replay_frame.h"

#include <cstdint>
#include <optional>

namespace tomba {

class DrawSection final : public psx::present::ReplaySection {
public:
  enum class Variant : std::uint8_t {
    Field,     // 0x8003F9A8
    Room,      // 0x8003FA44: no scenery walker or tile grid
    Sop,       // Sop::fieldUpdate's draw tail (Sop::kSceneryDraw, kListDraw, kGridDraw)
    Title,     // the title sub-machines' Demo::kTitleEdges and kTitleItems
    Page,      // GAME's page step G:8010810C alone, when 0x1F800136 >= 2 holds the field draw off (a menu page)
    Ride,      // A03's draw 0x8010B328, which G:80108CAC (the cart ride) runs in place of the field draw
    Narration, // SOP's narration crawl alone, while Sop::fieldUpdate's startup delay holds its draw tail off
    Lists,     // 0x8003FA1C: Tomba and list C, FieldTransition::main's fade frames
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
  // A03:8010B328, the cart ride's draw: HUD, A03's HUD and Tomba, list C, the ground walker, list D.
  static constexpr std::uint32_t kRideDraw = 0x8010B328u;
  void select(Selection selection) {
    selection_ = selection;
  }
  // A ScreenFade (FUN_8007E9C8) the last frame linked after its section; each replay links it after the steps.
  struct TrailingFade {
    std::uint32_t color = 0;
    std::uint32_t blend = 0;
    std::uint32_t slot = 0;
    bool operator==(const TrailingFade &) const = default;
  };
  void trail(std::optional<TrailingFade> fade) {
    trailingFade_ = fade;
  }

  // No preamble: the env, its OT and the packet pool are already the frame's.
  [[nodiscard]] std::optional<std::uint32_t> draw(Core &core, psx::present::GuestRedraw &redraw) const override;
  [[nodiscard]] bool linked(std::uint32_t head) const override;

private:
  // The selected variant's steps; false when one was refused.
  [[nodiscard]] bool drawSteps(Core &core, psx::present::GuestRedraw &redraw) const;

  Selection selection_;
  std::optional<TrailingFade> trailingFade_;
};

} // namespace tomba
