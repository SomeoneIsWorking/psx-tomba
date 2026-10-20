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

std::string DevWarp::arm(Core &core, const psx::debug::CommandArgs &args) {
  const std::optional<std::uint32_t> areaWord = args.number(1);
  const std::optional<std::uint32_t> sectionWord = args.size() < 3 ? 0u : args.number(2);
  const std::optional<std::uint32_t> entryWord = args.size() < 4 ? 0u : args.number(3);
  if (args.size() > 4 || !areaWord || !sectionWord || !entryWord) {
    return "usage: warp <area> [section] [entry]";
  }
  const std::uint32_t area = *areaWord;
  const std::uint32_t section = *sectionWord;
  const std::uint32_t entry = *entryWord;
  const std::uint32_t lastArea = core.mem_r16(kLastArea);
  if (area > lastArea) {
    return lucent::format("refused: area {} is out of range (0..{})", area, lastArea);
  }
  const std::uint32_t sections = core.mem_r16(kSectionCounts + area * 2u);
  if (section >= sections) {
    return lucent::format("refused: area {} has {} sections, not {}", area, sections, section);
  }
  if (entry > kMaxEntry) {
    return lucent::format("refused: entry {} is out of range (0..{})", entry, kMaxEntry);
  }
  if (!fieldRunning(core)) {
    return "refused: a warp is legal only while the field is running";
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
