#pragma once
#include <cstdint>

class Core;

// The community debug menu's GRANT ALL ITEMS: raw count bytes, 1 for the unique items and 9 for the
// consumables, and the quest-pass counter at 0xFF. No ring or quest cross-reference upkeep.
class ItemGrant {
public:
  static void all(Core &core);
};
