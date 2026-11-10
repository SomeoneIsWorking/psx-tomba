#include "frame/draw_replay.h"

#include "core.h"
#include "frame/frame_cut.h"
#include "sched/task_slot_layout.h"

namespace tomba {

DrawReplay::DrawReplay(world::RenderRecordPool &records) : motion_(records), frame_(section_, motion_.tracks()) {}

void DrawReplay::beforeDraw(Core &core, DrawSection::Variant variant) {
  drewThisFrame_ = true;
  const DrawSection::Selection selection{.variant = variant,
                                         .pageStep = core.mem_r32(FrameCut::kStagePointer) == sched::kStageGameEntry};
  if (drawn_ != selection) {
    frame_.forget();
  }
  drawn_ = selection;
  section_.select(selection);
  frame_.beforeDraw(core);
}

void DrawReplay::endFrame(bool cut) {
  // The cut is known only after the draw sampled, so that sample may predate the scene change.
  if (!drewThisFrame_ || cut) {
    forget();
  }
  drewThisFrame_ = false;
}

void DrawReplay::forget() {
  frame_.forget();
  drawn_.reset();
}

} // namespace tomba
