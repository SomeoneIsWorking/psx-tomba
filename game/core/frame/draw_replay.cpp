#include "frame/draw_replay.h"

#include "core.h"

namespace tomba {
namespace {

// 0 in play; 1 paused, the draw running over the lists it kept; 2 and up nothing is drawn.
constexpr std::uint32_t kPauseByte = 0x1F800136u;

} // namespace

DrawReplay::DrawReplay(const world::RenderRecordPool &records) : motion_(records), frame_(section_, motion_.tracks()) {}

void DrawReplay::beforeDraw(Core &core, DrawSection::Variant variant) {
  // A paused draw is the pause menu's: it is composed from its producers, and play pairs from scratch after it.
  if (core.mem_r8(kPauseByte) != 0) {
    frame_.forget();
    drawn_.reset();
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

void DrawReplay::endFrame() {
  if (!drewThisFrame_) {
    frame_.forget();
    drawn_.reset();
  }
  drewThisFrame_ = false;
}

} // namespace tomba
