#pragma once

#include "game_runtime.h"
#include "guest_widescreen_projection.h"
#include "stock_libcd.h"
#include "stock_libgte.h"
#include "stock_pad_buffers.h"
#include "stock_vsync.h"

#include <cstdint>
#include <cstdio>
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
  const GuestWidescreenProjection *guestWidescreenProjection() const override;
  bool titleCommand(Core &core, const psx::game::TitleCommandLine &input, std::FILE *reply) override;

private:
  psx::hle::StockVSync vsync_;
  psx::cd::StockLibcd libcd_;
  psx::hle::StockLibgte libgte_;
  psx::input::StockPadBuffers pad_;
  GuestWidescreenProjection widescreenPolicy_;
};

} // namespace tomba1
