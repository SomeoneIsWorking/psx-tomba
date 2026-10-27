#include "dev_warp.h"

#include "core.h"
#include "guest_call.h"

#include <lucent/log.h>

namespace tomba1 {
bool DevWarp::fieldRunning(Core &core) {
  const std::uint32_t record = core.mem_r32(kTaskRecordPointer);
  return record != 0u && core.mem_r16(record + kMachineOffset) == kFieldMachine &&
         core.mem_r16(record + kStateOffset) == kFieldRunning;
}

int DevWarp::areaCount(Core &core) {
  return static_cast<int>(core.mem_r16(kLastArea)) + 1;
}

std::string DevWarp::arm(Core &core, std::uint32_t area, std::uint32_t section, std::uint32_t entry) {
  const std::uint32_t sections = core.mem_r16(kSectionCounts + area * 2u);
  if (section >= sections) {
    return lucent::format("refused: area {} has {} sections, not {}", area, sections, section);
  }
  area_ = area;
  section_ = section;
  entry_ = entry;
  armed_ = true;
  return lucent::format("ok: warp armed for area {} section {} entry {}", area_, section_, entry_);
}

void DevWarp::applyArmed(Core &core, std::uint32_t frame) {
  if (!armed_) {
    return;
  }
  armed_ = false;
  if (!fieldRunning(core)) {
    lucent::error("warp", "area {} section {} dropped at f{}: the field is no longer running", area_, section_, frame);
    return;
  }
  core.mem_w16(kDestinationArea, static_cast<std::uint16_t>(area_));
  core.mem_w16(kDestinationSection, static_cast<std::uint16_t>(section_));
  core.mem_w16(kDestinationEntry, static_cast<std::uint16_t>(entry_));
  if (core.mem_r16(kCurrentArea) != area_) {
    psx::cpu::callGuestNow(core, "DevWarp::applyArmed", kStopBgm, 1u);
  }
  const std::uint32_t record = core.mem_r32(kTaskRecordPointer);
  core.mem_w16(record + kMachineOffset, kTransitionMachine);
  core.mem_w16(record + kStateOffset, 0);
  lucent::info("warp", "area {} section {} entry {} requested at f{}", area_, section_, entry_, frame);
}

} // namespace tomba1
