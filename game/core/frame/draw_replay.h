// draw_replay.h — Tomba! 2's gameplay in-betweens: the field or room draw replayed with its moving inputs blended.
#pragma once

#include "frame/draw_motion.h"
#include "frame/draw_section.h"
#include "frame/frame_cut.h"
#include "replay_frame.h"

#include <optional>

class Core;

namespace tomba {

class DrawReplay {
public:
  explicit DrawReplay(world::RenderRecordPool &records);

  // `variant` is about to draw, paused or not: paired with the last frame's draw when both drew it in one stage.
  void beforeDraw(Core &core, DrawSection::Variant variant);
  // The logic frame ended; one that did not draw, or whose cut may postdate its sample, breaks the pairing.
  void endFrame(const FrameCut &cut);
  // The next draw pairs with nothing (a savestate restore).
  void forget();

  [[nodiscard]] psx::present::ReplayFrame &frame() {
    return frame_;
  }

private:
  DrawSection section_;
  DrawMotion motion_;
  psx::present::ReplayFrame frame_;
  std::optional<DrawSection::Selection> drawn_;
  bool drewThisFrame_ = false;
  bool placedBeforeSample_ = false;
};

} // namespace tomba
