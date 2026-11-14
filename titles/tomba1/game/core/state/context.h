// The one per-Core owner of Tomba! 1's native subsystem state.
#pragma once

#include "dev_life_loss.h"
#include "dev_warp.h"
#include "game_runtime.h"
#include "guest_task_slots.h"
#include "screen_skip.h"

class Core;

namespace tomba1 {

// Created with each Game (Runtime::createSession); every per-game native owner lives here.
class Context final : public psx::game::TitleSessionState {
public:
  explicit Context(Core &core);

  GuestTaskSlots tasks;
  DevWarp warp;
  DevLifeLoss lifeLoss;
  ScreenSkip screenSkip;
};

Context &context(Core &core);
const Context &context(const Core &core);

} // namespace tomba1
