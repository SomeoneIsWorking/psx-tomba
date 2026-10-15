#pragma once

#include "game_runtime.h"
#include "guest_widescreen_projection.h"
#include "stock_libcd.h"
#include "stock_libgpu.h"
#include "stock_libgte.h"
#include "stock_pad_buffers.h"
#include "stock_vsync.h"

namespace tomba {

// libgpu's DrawSync and GPU timeout pair in MAIN.EXE.
inline constexpr psx::hle::StockLibgpu::Entries kStockLibgpu{.drawSync = 0x80080F6Cu,
                                                             .timeoutArm = 0x800834A0u,
                                                             .timeoutCheck = 0x800834D4u,
                                                             .timeoutDeadlineWord = 0x800A5ADCu,
                                                             .timeoutFlagWord = 0x800A5AE0u};

// Process-lifetime owner of Tomba! 2's framework-facing behavior.
class Runtime final : public psx::game::GameRuntime {
public:
  Runtime();

  void startProgram(Core &core) override;
  void registerOverrides(Game &game) override;
  std::unique_ptr<psx::game::TitleSessionState> createSession(Game &game) override;
  std::unique_ptr<psx::game::FrameDriver> createFrameDriver(Game &game) override;
  void deliverPad(Core &core, const psx::input::PadDelivery &delivery) override;
  std::optional<psx::gpu::PacketPoolExtent> packetPool(Core &core) const override;
  bool interpolates() const override;
  const psx::present::StateProducer *stateProducer(Core &core, std::uint32_t producer) const override;
  bool sealedFrameIsCut(Core &core) const override;
  int displayHeight() const override;
  const GuestWidescreenProjection *guestWidescreenProjection() const override;
  bool titleCommand(Core &core, const psx::game::TitleCommandLine &input, std::FILE *reply) override;
  psx::game::TitleAudio *audio(Core &core) const override;
  psx::game::TitleDevWarp *devWarp(Core &core) const override;
  std::string worldReadout(Core &core) const override;
  psx::state::NativeStatePort *nativeState(Core &core) const override;
  const psx::game::HostIdentity *hostIdentity() const override;

private:
  // FUN_80050B08's init prefix without its scheduler loop.
  void bootInit(Core &core);
  void bindLoadedResident(Core &core);

  psx::hle::StockVSync vsync_;
  psx::cd::StockLibcd libcd_;
  psx::hle::StockLibgpu libgpu_;
  psx::hle::StockLibgte libgte_;
  psx::input::StockPadBuffers pad_;
  GuestWidescreenProjection aspectPolicy_;
};

} // namespace tomba
