#include "frame/draw_replay.h"

#include "core.h"

namespace tomba {
namespace {

// 0 in play; 1 paused, the draw running over the lists it kept; 2 and up nothing is drawn.
constexpr std::uint32_t kPauseByte = 0x1F800136u;

} // namespace

DrawReplay::DrawReplay(const world::RenderRecordPool &records) : motion_(records), frame_(section_, motion_.tracks()) {}

void DrawReplay::beforeDraw(Core &core, DrawSection::Variant variant) {
  // The pause screen composes from its producers.
  if (core.mem_r8(kPauseByte) != 0) {
    forget();
    return;
  }
  drewThisFrame_ = true;
  if (drawn_ != variant) {
    frame_.forget();
  }
  drawn_ = variant;
  section_.select(variant);
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
