#include "dev_warp.h"

#include "core.h"
#include "guest_call.h"

#include <charconv>
#include <lucent/log.h>
#include <sstream>
#include <vector>

namespace tomba1 {
namespace {

bool decimal(const std::string &word, std::uint32_t &value) {
  const char *end = word.data() + word.size();
  const auto [last, error] = std::from_chars(word.data(), end, value);
  return error == std::errc{} && last == end;
}

} // namespace

bool DevWarp::fieldRunning(Core &core) {
  const std::uint32_t record = core.mem_r32(kTaskRecordPointer);
  return record != 0u && core.mem_r16(record + kMachineOffset) == kFieldMachine &&
         core.mem_r16(record + kStateOffset) == kFieldRunning;
}

std::string DevWarp::arm(Core &core, const char *line) {
  std::istringstream stream(line);
  std::vector<std::string> words;
  for (std::string word; stream >> word;) {
    words.push_back(word);
  }
  std::uint32_t area = 0;
  std::uint32_t section = 0;
  std::uint32_t entry = 0;
  const bool parsed = words.size() >= 2 && words.size() <= 4 && decimal(words[1], area) &&
                      (words.size() < 3 || decimal(words[2], section)) &&
                      (words.size() < 4 || decimal(words[3], entry));
  if (!parsed) {
    return "usage: warp <area> [section] [entry]";
  }
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
