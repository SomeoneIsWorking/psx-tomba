#include "frame/draw_replay.h"

#include "core.h"
#include "entry/session.h"

#include <lucent/log.h>

namespace tomba {

DrawReplay::DrawReplay(world::RenderRecordPool &records) : motion_(records), frame_(section_, motion_.tracks()) {}

void DrawReplay::beforeDraw(Core &core, DrawSection::Variant variant) {
  beforeDraw(core, {.variant = variant, .pageStep = DrawSection::pageStepResident(core)});
}

void DrawReplay::beforeDraw(Core &core, DrawSection::Selection selection) {
  drewThisFrame_ = true;
  placementsAtSample_ = eng(&core).frameCut().cameraPlacements();
  if (drawn_ != selection) {
    lucent::debug("draw-section",
                  "variant {} page step {} grid {} scroller {} menu {} item {}",
                  static_cast<int>(selection.variant),
                  selection.pageStep,
                  selection.grid,
                  selection.scroller,
                  selection.menu,
                  selection.item);
    frame_.forget();
  }
  drawn_ = selection;
  section_.select(selection);
  frame_.beforeDraw(core);
}

void DrawReplay::endFrame(const FrameCut &cut) {
  // A camera re-placed before the draw sampled is already in that sample; a scene change may not be.
  const bool staleSample = cut.sceneChanged() || cut.endedPassPlacements() > placementsAtSample_;
  if (!drewThisFrame_ || staleSample) {
    forget();
  }
  drewThisFrame_ = false;
  placementsAtSample_ = 0;
}

std::optional<std::array<float, 2>> DrawReplay::gridScrollPast() const {
  const std::optional<float> t = frame_.replaying();
  return t ? motion_.gridScrollPast(frame_.motion(), *t) : std::nullopt;
}

void DrawReplay::forget() {
  frame_.forget();
  drawn_.reset();
}

} // namespace tomba
