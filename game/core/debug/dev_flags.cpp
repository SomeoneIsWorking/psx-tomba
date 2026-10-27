#include "debug/dev_flags.h"

#include "core.h"
#include "scene/scene_flags.h"

#include <lucent/log.h>

namespace tomba {

std::string DevFlags::get(Core &core, uint32_t index) {
  return lucent::format("flag {} = {}", index, core.mem_r8(scene_flags::flagAddr(static_cast<int32_t>(index))));
}

std::string DevFlags::armSet(uint32_t index, uint32_t value) {
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
