// game/ui/items_tab.h — the Items tab of the field subscreen (page 6): its step FUN_80036DFC, and the lent effects a
// selected suit shows in the hint box, one row of the suit's ModSettings parts.
#pragma once

#include <cstddef>
#include <cstdint>

class Core;

namespace tomba::native {
class NativeOverrideCatalog;
} // namespace tomba::native

namespace tomba {

class HintWriter;

// The subscreen's state block, FUN_8007AC6C zeroes 0x30 bytes of it when the page opens.
namespace subscreen {
inline constexpr std::uint32_t kState = 0x80100660u;
inline constexpr std::uint32_t kPhase = 0x00u;     // 0 opening, 1 running, 2 closing
inline constexpr std::uint32_t kStep = 0x01u;      // the open tab's step; Items: 1 browsing
inline constexpr std::uint32_t kTemplates = 0x04u; // the UI template base FUN_8007E1B8 draws from
inline constexpr std::uint32_t kTab = 0x08u;       // 0 Items
inline constexpr std::uint32_t kEntered = 0x09u;   // the cursor is inside the tab
inline constexpr std::uint32_t kScreenRow = 0x1Cu; // the cursor's row on screen
inline constexpr std::uint32_t kColumn = 0x1Du;    // 0 left, 1 right
inline constexpr std::uint32_t kScroll = 0x1Eu;    // the first row shown
inline constexpr std::uint32_t kRow = 0x1Fu;       // the cursor's row in the list
inline constexpr std::uint32_t kHintKind = 0x20u;  // the hint box's text: the selected item's kind, 4 its note
inline constexpr std::uint32_t kRows = 0x22u;      // the list's rows: equipment, then the other kinds
inline constexpr std::uint32_t kVisibleRows = 7u;
inline constexpr std::uint8_t kRunning = 1u;
inline constexpr std::uint8_t kBrowsing = 1u;
// FUN_800368D0(state, row, column): the item id in that cell, 999 for the Sort cell, negative for none.
inline constexpr std::uint32_t kItemAt = 0x800368D0u;
inline constexpr std::uint32_t kNoItem = 999u;
} // namespace subscreen

class ItemsTab {
public:
  static void registerOverrides(native::NativeOverrideCatalog &natives);

  // The item under the cursor while the Items tab is entered and browsing; negative otherwise.
  static int selectedItem(Core &core);

  // The hint box's text while a suit that lends effects is selected; false leaves the tab's own hint.
  bool hint(Core &core, const HintWriter &writer) const;

private:
  static void ovStep(Core *c);

  // Select opens the selected suit's row; then Left/Right choose an effect, Cross toggles it, Circle or Select close.
  void step(Core &core, std::uint32_t state);

  bool editing_ = false;
  std::size_t part_ = 0;
};

} // namespace tomba
