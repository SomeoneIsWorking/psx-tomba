#include "debug/dev_flags.h"

#include "core.h"
#include "debug/dev_args.h"
#include "scene/scene_flags.h"

#include <lucent/log.h>

namespace tomba {

std::string DevFlags::arm(Core &core, const char *line) {
  const auto words = DevArgs::words(line);
  uint32_t index = 0;
  uint32_t value = 0;
  const bool get = words.size() == 3 && words[1] == "get" && DevArgs::decimal(words[2], index);
  const bool set =
      words.size() == 4 && words[1] == "set" && DevArgs::decimal(words[2], index) && DevArgs::decimal(words[3], value);
  if (!get && !set) {
    return "usage: flag get <index> | flag set <index> <value>   (index 0..255, value 0..255)";
  }
  if (index >= kFlagCount) {
    return lucent::format("refused: flag {} is out of range (0..{})", index, kFlagCount - 1);
  }
  if (get) {
    return lucent::format("flag {} = {}", index, core.mem_r8(scene_flags::flagAddr(static_cast<int32_t>(index))));
  }
  if (value > 0xffu) {
    return lucent::format("refused: value {} does not fit a byte", value);
  }
  index_ = index;
  value_ = value;
  armed_ = true;
  return lucent::format("ok: flag {} set to {} armed", index_, value_);
}

void DevFlags::applyArmed(Core &core, uint32_t frame) {
  if (!armed_) {
    return;
  }
  armed_ = false;
  core.mem_w8(scene_flags::flagAddr(static_cast<int32_t>(index_)), static_cast<uint8_t>(value_));
  lucent::info("flag", "flag {} = {} written at f{}", index_, value_, frame);
}

} // namespace tomba
