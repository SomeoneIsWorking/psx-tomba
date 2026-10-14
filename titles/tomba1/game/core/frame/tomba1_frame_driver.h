#pragma once

#include "game_runtime.h"

#include <cstdint>

class Game;

namespace tomba1 {

// The finite per-frame transaction: the frame's own bookkeeping, one pass over the scheduled guest
// tasks (this Core's `GuestTaskSlots`), and per requested display field the VBlank event, the audio
// advance and the disc stream, then one presentation fence. The boot prefix is
// `Tomba1Runtime::bootInit`'s, before the first frame.
class Tomba1FrameDriver final : public FrameDriver {
public:
  explicit Tomba1FrameDriver(Game &game);

  void stepFrame(Core &core, std::uint32_t frame) override;

private:
  void finishMainIteration(Core &core, std::uint32_t fields);

  Game &game_;
  std::uint32_t completedFrames_ = 0;
};

} // namespace tomba1
