#pragma once

#include "game_runtime.h"
#include "guest_widescreen_projection.h"
#include "stock_libcd.h"
#include "stock_libgte.h"
#include "stock_pad_buffers.h"
#include "stock_vsync.h"
#include "tomba1/game/core/debug/debug_menu.h"

#include <cstdint>
#include <memory>

namespace tomba1 {

class Runtime final : public psx::game::GameRuntime {
public:
  Runtime();

  void startProgram(Core &core) override;
  void registerOverrides(Game &game) override;
  std::unique_ptr<psx::game::TitleSessionState> createSession(Game &game) override;
  std::unique_ptr<psx::game::FrameDriver> createFrameDriver(Game &game) override;
  void deliverPad(Core &core, const psx::input::PadDelivery &delivery) override;
  void stockCdReadLanded(Core &core, const psx::cd::StockReadLanding &landing) override;
  const psx::present::GuestWidescreenProjection *guestWidescreenProjection() const override;
  const psx::game::TitleDebugMenu &debugMenu() const override {
    return debugMenu_;
  }

private:
  DebugMenu debugMenu_;
  psx::hle::StockVSync vsync_;
  psx::cd::StockLibcd libcd_;
  psx::hle::StockLibgte libgte_;
  psx::input::StockPadBuffers pad_;
  psx::present::GuestWidescreenProjection widescreenPolicy_;
};

} // namespace tomba1
