#pragma once

#include <cstdint>
#include <string>

class Core;

namespace tomba {

// The debris burst a pickup's collection throws (FUN_800741DC case 4): M:80027144 breaks a model into effect-table
// slots whose scale the leaf handler M:80027254 shrinks each frame. Armed between frames, thrown from Tomba's own
// model at his place at the next frame boundary, so the effect scale can be reached without playing to a pickup.
class DevDebris {
public:
  // The returned text answers the client.
  std::string arm();
  void applyArmed(Core &core, std::uint32_t frame);

private:
  bool armed_ = false;
};

} // namespace tomba
