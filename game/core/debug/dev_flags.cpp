#include "debug/dev_flags.h"

#include "core.h"
#include "scene/scene_flags.h"

#include <lucent/log.h>

namespace tomba {

std::string DevFlags::arm(Core &core, const psx::debug::CommandArgs &args) {
  const std::optional<uint32_t> indexWord = args.number(2);
  const std::optional<uint32_t> valueWord = args.number(3);
  const bool get = args.size() == 3 && args.word(1) == "get" && indexWord;
  const bool set = args.size() == 4 && args.word(1) == "set" && indexWord && valueWord;
  if (!get && !set) {
    return "usage: flag get <index> | flag set <index> <value>   (index 0..255, value 0..255)";
  }
  const uint32_t index = *indexWord;
  if (index >= kFlagCount) {
    return lucent::format("refused: flag {} is out of range (0..{})", index, kFlagCount - 1);
  }
  if (get) {
    return lucent::format("flag {} = {}", index, core.mem_r8(scene_flags::flagAddr(static_cast<int32_t>(index))));
  }
  const uint32_t value = *valueWord;
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
