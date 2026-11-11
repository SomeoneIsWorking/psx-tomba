#pragma once

#include <cstdint>
#include <string>

class Core;

namespace tomba {

// The AP count that rises over Tomba when he scores (M:80071B44 with no source node): a node whose behaviour
// M:80072520 projects his place and draws the digits through UiSprite::quads in the update block. Armed between
// frames and spawned at the next frame boundary, so the popup can be reached without playing to a score.
class DevApPopup {
public:
  inline static constexpr std::uint32_t kMaxAmount = 99999;

  // The returned text answers the client.
  std::string arm(std::uint32_t amount);
  void applyArmed(Core &core, std::uint32_t frame);

private:
  std::uint32_t amount_ = 0;
};

} // namespace tomba
