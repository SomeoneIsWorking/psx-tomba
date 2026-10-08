#pragma once

#include <cstdint>
#include <string>

class Core;

namespace tomba {

// Apply one complete Tomba! 2 cold-area warp. This operation is title state-machine behavior: both
// the title frame driver and the transitional framework-facing hook call this single owner.
void applyColdWarp(Core &core, int area, int sub);

// The control-channel `warp <area> [sub]` request: armed between frames, applied by the frame driver
// at its next frame boundary.
class DevWarp {
public:
  // Parses and arms one command line; the returned text answers the client.
  std::string arm(Core &core, const char *line);
  // Applies an armed request through the engine's own transition owners.
  void applyArmed(Core &core, uint32_t frame);

private:
  bool armed_ = false;
  int area_ = 0;
  int sub_ = 0;
};

} // namespace tomba
