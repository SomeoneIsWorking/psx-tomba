#pragma once

#include "game_iface.h"
#include "guest_widescreen_projection.h"

namespace tomba {

// Process-lifetime owner of Tomba! 2's framework-facing behavior. The legacy base is temporary:
// measured compatibility facts and callbacks remain reachable while psxport replaces their generic
// consumers with narrow typed interfaces.
class TombaRuntime final : public LegacyGameRuntimeAdapter {
public:
  TombaRuntime();

  void *createContext(Core &core) override;
  void destroyContext(void *context) override;
  void registerOverrides(Game &game) override;
  void bootInit(Core &core) override;
  std::unique_ptr<FrameDriver> createFrameDriver(Game &game) override;
  RenderCapabilities renderCapabilities() const override;
  bool guestVramIsPicture(const Game &game) const override;
  bool sealedFrameIsCut(Core &core) const override;
  const GuestWidescreenProjection *guestWidescreenProjection() const override;
  bool controlCommand(Core &core, const char *cmd, const char *line, FILE *out) override;

private:
  void bindLoadedResident(Core &core);

  GuestWidescreenProjection aspectPolicy_;
};

} // namespace tomba
