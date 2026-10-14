// dev_areas.h — the areas the Debug tab's warp offers, with their sourced names.
#pragma once

#include "game_runtime.h"

class Core;

namespace tomba {

class TombaDevWarp final : public psx::game::TitleDevWarp {
public:
  explicit TombaDevWarp(Core &core) : core_(core) {}

  int areaCount() const override;
  // "" for an area with no sourced name; the selector shows "Area N".
  const char *areaName(int area) const override;
  // Only from the field (GAME stage).
  bool warpAllowed() const override;

private:
  Core &core_;
};

} // namespace tomba
