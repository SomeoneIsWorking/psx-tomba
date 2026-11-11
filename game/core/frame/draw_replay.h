// draw_replay.h — Tomba! 2's gameplay in-betweens: the field or room draw replayed with its moving inputs blended.
#pragma once

#include "frame/draw_motion.h"
#include "frame/draw_section.h"
#include "frame/frame_cut.h"
#include "frame/update_block_draws.h"
#include "replay_frame.h"

#include <array>
#include <cstdint>
#include <optional>

class Core;

namespace tomba {

class DrawReplay {
public:
  explicit DrawReplay(world::RenderRecordPool &records);

  // The field or room draw is about to run, paused or not; in GAME the page step follows it.
  void beforeDraw(Core &core, DrawSection::Variant variant);
  // `selection` is about to draw: paired with the last frame's draw when both drew the same selection.
  void beforeDraw(Core &core, DrawSection::Selection selection);
  // The logic frame ended; one that did not draw, or whose cut may postdate its sample, breaks the pairing.
  void endFrame(const FrameCut &cut);
  // The next draw pairs with nothing (a savestate restore).
  void forget();

  [[nodiscard]] psx::present::ReplayFrame &frame() {
    return frame_;
  }
  // The fade and letterbox packets linked before the section, rebuilt in each replay.
  [[nodiscard]] UpdateBlockDraws &updateDraws() {
    return updateDraws_;
  }
  // While a replay runs: how far past its scroll words it scrolls the tile grid, X and Y.
  [[nodiscard]] std::optional<std::array<float, 2>> gridScrollPast() const;

private:
  DrawSection section_;
  DrawMotion motion_;
  UpdateBlockDraws updateDraws_;
  std::array<psx::present::MotionSource *, 1> sources_{&updateDraws_};
  psx::present::ReplayFrame frame_;
  std::optional<DrawSection::Selection> drawn_;
  bool drewThisFrame_ = false;
  std::uint32_t placementsAtSample_ = 0; // FrameCut::cameraPlacements when the draw sampled
};

} // namespace tomba
