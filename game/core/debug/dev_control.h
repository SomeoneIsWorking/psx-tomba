#pragma once

#include "debug/dev_flags.h"
#include "debug/dev_items.h"
#include "debug/dev_warp.h"

#include <cstdint>
#include <string>

class Core;

namespace tomba {

// The title's developer commands on the control channel: `warp`, `items` and `flag`. Each is parsed and armed
// between frames, and written by the frame driver at its next frame boundary.
class DevControl {
public:
  // True when `command` is one of ours; `reply` then holds the answer for the client.
  bool handle(Core &core, const char *command, const char *line, std::string &reply);
  void applyArmed(Core &core, uint32_t frame);

private:
  static void giveAndFlag(Core &core, uint32_t item, uint32_t amount);

  DevWarp warp_;
  DevItems items_;
  DevFlags flags_;
};

} // namespace tomba
