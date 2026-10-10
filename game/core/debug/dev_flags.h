#pragma once

#include <cstdint>
#include <string>

class Core;

namespace tomba {

// The control-channel `flag get <index>` and `flag set <index> <value>` commands over the event-flag table the
// community debug menu's EVENT FLAGS page edits: 256 bytes, the scene flags (scene_flags::kFlagTable). A set is
// armed between frames and applied at the next frame boundary; a get answers at once.
class DevFlags {
public:
  inline static constexpr uint32_t kFlagCount = 256;

  std::string arm(Core &core, const char *line);
  void applyArmed(Core &core, uint32_t frame);

private:
  bool armed_ = false;
  uint32_t index_ = 0;
  uint32_t value_ = 0;
};

} // namespace tomba
