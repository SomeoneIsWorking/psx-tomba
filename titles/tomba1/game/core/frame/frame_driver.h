#pragma once

#include "game_runtime.h"
#include "guest_task_slots.h"

#include <cstdint>

class Game;

namespace tomba1 {

class Tomba1Runtime;

// The finite per-frame transaction: the frame's own bookkeeping, one pass over the scheduled guest
// tasks, the measured VBlank delivery and audio advance, and one presentation fence.
//
// The task table itself — one saved R3000 context per slot, the cooperative yield/restart
// transitions, and the bounded resume across display fields — belongs to `GuestTaskSlots`, which this
// composes once per frame.
class Tomba1FrameDriver final : public FrameDriver {
public:
  Tomba1FrameDriver(Game &game, Tomba1Runtime &runtime);

  void stepFrame(Core &core, std::uint32_t frame) override;

private:
  void finishMainIteration(Core &core, std::uint32_t fields);

  Game &game_;
  Tomba1Runtime &runtime_;
  GuestTaskSlots tasks_;
  std::uint32_t completedFrames_ = 0;
  bool booted_ = false;
};

} // namespace tomba1
