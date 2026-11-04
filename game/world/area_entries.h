// game/world/area_entries.h — MAIN's per-area entry records: where an area entry places Tomba.
#pragma once

#include "core.h"

#include <lucent/log.h>

#include <array>
#include <cstdint>
#include <cstdlib>
#include <string>

namespace tomba {

// 0x800A54A8 holds one word per area pointing at its 8-byte records {s16 x, y, z; u16 flags}, entry n at +8n.
// The records lie in area order and the last area's end where the pointer table begins.
class AreaEntries {
public:
  static constexpr std::uint32_t kTables = 0x800A54A8u;
  static constexpr std::uint32_t kRecordBytes = 8u;
  // Areas 0..21: one A0* overlay each on the disc, 22 entries in each of MAIN's per-area handler tables, and
  // GAME.BIN's next-table zeroed at 22. Area load is file `area + 3` of the table at 0x800BE118, so 22 would load
  // START.BIN.
  static constexpr std::uint32_t kAreaCount = 22u;
  // Record flag: the entry is inside a room (0x800BF816 set, room id in the top nibble).
  static constexpr std::uint16_t kRoomEntry = 0x800u;
  static constexpr unsigned kRoomShift = 12u;
  // Map screen names: area -> region byte (0x800A55C8) -> region name pointer (0x800A2B64); area -> first place
  // (0x800A55B0) plus record flag bits 9-10 -> place name pointer (0x800A2B84).
  static constexpr std::uint32_t kRegionOfArea = 0x800A55C8u;
  static constexpr std::uint32_t kRegionNames = 0x800A2B64u;
  static constexpr std::uint32_t kFirstPlaceOfArea = 0x800A55B0u;
  static constexpr std::uint32_t kPlaceNames = 0x800A2B84u;
  static constexpr std::uint16_t kPlaceBits = 0x600u;
  static constexpr unsigned kPlaceShift = 9u;

  static std::uint32_t record(Core &core, std::uint32_t area, std::uint32_t entry) {
    return core.mem_r32(kTables + area * 4u) + entry * kRecordBytes;
  }

  static std::uint16_t flags(Core &core, std::uint32_t area, std::uint32_t entry) {
    return core.mem_r16(record(core, area, entry) + kFlagsOffset);
  }

  // The map screen's place of an entry (FUN_80078798's index); also its bit in the visited-places register.
  static std::uint32_t placeIndex(Core &core, std::uint32_t area, std::uint32_t entry) {
    return core.mem_r8(kFirstPlaceOfArea + area) + ((flags(core, area, entry) & kPlaceBits) >> kPlaceShift);
  }

  // FUN_80078798: the place name the map screen shows for an entry.
  static std::string placeName(Core &core, std::uint32_t area, std::uint32_t entry) {
    return nameAt(core, core.mem_r32(kPlaceNames + placeIndex(core, area, entry) * 4u));
  }

  // FUN_800787F8: the region name the map screen shows for an area.
  static std::string regionName(Core &core, std::uint32_t area) {
    return nameAt(core, core.mem_r32(kRegionNames + core.mem_r8(kRegionOfArea + area) * 4u));
  }

  // How many entries `area` has; a table out of area order is a corrupt image and fatal.
  static std::uint32_t count(Core &core, std::uint32_t area) {
    const std::uint32_t begin = core.mem_r32(kTables + area * 4u);
    const std::uint32_t end = area + 1u < kAreaCount ? core.mem_r32(kTables + (area + 1u) * 4u) : kTables;
    if (end < begin || (end - begin) % kRecordBytes != 0u) {
      lucent::error("warp", "area {} entry records [{:08X},{:08X}) are not in area order", area, begin, end);
      std::abort();
    }
    return (end - begin) / kRecordBytes;
  }

private:
  static constexpr std::uint32_t kFlagsOffset = 6u;
  static constexpr std::size_t kNameBytes = 64u;

  // The map's two-line names break with '\n'; a label reads them as one line.
  static std::string nameAt(Core &core, std::uint32_t address) {
    std::array<char, kNameBytes> text{};
    core.readCString(address, text.data(), text.size());
    std::string name(text.data());
    for (char &c : name) {
      if (c == '\n') {
        c = ' ';
      }
    }
    return name;
  }
};

} // namespace tomba
