#include "frame/draw_replay.h"

#include "core.h"
#include "entry/session.h"

namespace tomba {

DrawReplay::DrawReplay(world::RenderRecordPool &records) : motion_(records), frame_(section_, motion_.tracks()) {}

void DrawReplay::beforeDraw(Core &core, DrawSection::Variant variant) {
  drewThisFrame_ = true;
  placedBeforeSample_ = eng(&core).frameCut().cameraPlacing();
  const DrawSection::Selection selection{.variant = variant, .pageStep = DrawSection::pageStepResident(core)};
  if (drawn_ != selection) {
    frame_.forget();
  }
  drawn_ = selection;
  section_.select(selection);
  frame_.beforeDraw(core);
}

void DrawReplay::endFrame(const FrameCut &cut) {
  // A camera re-placed before the draw sampled is already in that sample; a scene change may not be.
  const bool staleSample = cut.sceneChanged() || (cut.isCut() && !placedBeforeSample_);
  if (!drewThisFrame_ || staleSample) {
    forget();
  }
  drewThisFrame_ = false;
  placedBeforeSample_ = false;
}

void DrawReplay::forget() {
  frame_.forget();
  drawn_.reset();
}

} // namespace tomba
