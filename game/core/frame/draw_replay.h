// draw_replay.h — Tomba! 2's gameplay in-betweens: the field or room draw replayed with its moving inputs blended.
#pragma once

#include "frame/draw_motion.h"
#include "frame/draw_section.h"
#include "frame/frame_cut.h"
#include "frame/update_block_draws.h"
#include "render/palette_cycles.h"
#include "render/wave_phases.h"
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
  // Before the draw: a ScreenFade this frame links after its section, which the section's replays link too.
  void noteTrailingFade(const DrawSection::TrailingFade &fade);

  [[nodiscard]] psx::present::ReplayFrame &frame() {
    return frame_;
  }
  // The fade and letterbox packets linked before the section, rebuilt in each replay.
  [[nodiscard]] UpdateBlockDraws &updateDraws() {
    return updateDraws_;
  }
  // The phases A0D's screen wave read in the last real draw, by model list.
  [[nodiscard]] render::WavePhases &wavePhases() {
    return wavePhases_;
  }
  // The camera's look-at and shake tail note the views they build and the eye offsets they leave.
  [[nodiscard]] ShakenView &shakenView() {
    return motion_.shakenView();
  }
  // While a replay runs: how far past its scroll words it scrolls the tile grid, X and Y.
  [[nodiscard]] std::optional<std::array<float, 2>> gridScrollPast() const;

private:
  DrawSection section_;
  DrawMotion motion_;
  UpdateBlockDraws updateDraws_;
  render::WavePhases wavePhases_;
  std::array<psx::present::MotionSource *, 3> sources_{&updateDraws_, &wavePhases_, &motion_.shakenView()};
  render::PaletteCycles paletteCycles_;
  psx::present::ReplayFrame frame_;
  std::optional<DrawSection::Selection> drawn_;
  bool drewThisFrame_ = false;
  std::optional<DrawSection::TrailingFade> trailingFade_; // linked after this frame's section
  std::uint32_t placementsAtSample_ = 0;                  // FrameCut::cameraPlacements when the draw sampled
};

} // namespace tomba
