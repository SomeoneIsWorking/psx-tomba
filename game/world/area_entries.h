// game/world/area_entries.h — MAIN's per-area entry records: where an area entry places Tomba.
#pragma once

#include "core.h"

#include <cstdint>

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

  static std::uint32_t record(Core &core, std::uint32_t area, std::uint32_t entry) {
    return core.mem_r32(kTables + area * 4u) + entry * kRecordBytes;
  }

  // How many entries `area` has.
  static std::uint32_t count(Core &core, std::uint32_t area) {
    const std::uint32_t end = area + 1u < kAreaCount ? core.mem_r32(kTables + (area + 1u) * 4u) : kTables;
    return (end - core.mem_r32(kTables + area * 4u)) / kRecordBytes;
  }
};

} // namespace tomba
