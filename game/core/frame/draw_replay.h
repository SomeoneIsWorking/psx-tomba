// draw_replay.h — Tomba! 2's gameplay in-betweens: the field or room draw replayed with its moving inputs blended.
#pragma once

#include "frame/draw_motion.h"
#include "frame/draw_section.h"
#include "replay_frame.h"

#include <optional>

class Core;

namespace tomba {

class DrawReplay {
public:
  explicit DrawReplay(const world::RenderRecordPool &records);

  // `variant` is about to draw: paired with the last frame's draw when both drew it unpaused.
  void beforeDraw(Core &core, DrawSection::Variant variant);
  // The logic frame ended; one that did not draw, or that FrameCut declared a cut, breaks the pairing.
  void endFrame(bool cut);
  // The next draw pairs with nothing (a savestate restore).
  void forget();

  [[nodiscard]] psx::present::ReplayFrame &frame() {
    return frame_;
  }

private:
  DrawSection section_;
  DrawMotion motion_;
  psx::present::ReplayFrame frame_;
  std::optional<DrawSection::Variant> drawn_;
  bool drewThisFrame_ = false;
};

} // namespace tomba
