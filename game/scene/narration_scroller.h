// narration_scroller.h — SOP's narration crawl FUN_8010C79C: its scroll step, then its line placement.
#pragma once

#include <cstdint>
#include <optional>

class Core;

namespace tomba::native {
class NativeOverrideCatalog;
} // namespace tomba::native

namespace tomba {

class NarrationScroller {
public:
  static constexpr std::uint32_t kEntry = 0x8010C79Cu;

  // The override: steps the scroll and draws the lines at it; a replay draws them at its fraction between the scroll
  // the last tick drew and the one this step reaches.
  static void run(Core *c);
  static void registerOverrides(native::NativeOverrideCatalog &natives);

private:
  // Where the lines draw after a step: the scroll and the first line still on screen.
  struct Placement {
    std::int32_t scroll = 0;
    std::int32_t line = 0;
  };
  // The phase machine at sm+0x54 and the scroll; the lines to draw, or the function's result when none draw.
  struct Step {
    std::optional<Placement> placement;
    std::uint32_t result = 0;
  };
  [[nodiscard]] static Step step(Core &core);
  // Each line from `placement.line` down while it has text and its top is above the screen's foot.
  static void place(Core &core, Placement placement);
};

} // namespace tomba
