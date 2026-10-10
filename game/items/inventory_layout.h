#pragma once
#include <cstdint>

// Guest addresses of the inventory fields in the save/state block, shared by Inventory and ItemGrant.
namespace inventory_layout {
inline constexpr uint32_t kBase = 0x800BF870u;
inline constexpr uint32_t kQuestPassCounter = kBase + 0x31u; // 0x800BF8A1
inline constexpr uint32_t kCounts = kBase + 0x244u;          // 0x800BFAB4, one byte per item id
} // namespace inventory_layout
