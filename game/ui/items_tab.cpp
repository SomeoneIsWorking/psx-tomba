#include "ui/items_tab.h"

#include "core.h"
#include "core/overrides/native_override_catalog.h"
#include "entry/mod_settings.h"
#include "entry/session.h"
#include "game.h"
#include "guest_call.h"
#include "input/pad_edges.h"
#include "native_dispatch.h"
#include "ui/subscreen_hint.h"

#include <lucent/log.h>

#include <array>
#include <span>
#include <string_view>

namespace tomba {
namespace {

constexpr std::uint32_t kTabStep = 0x80036DFCu; // (state): the Items tab's step, entered from FUN_800346BC

constexpr std::uint32_t kSound = 0x80074590u; // (id, 0, 0)
constexpr std::uint32_t kConfirmSound = 0x11u;
constexpr std::uint32_t kRefuseSound = 0x12u;
constexpr std::uint32_t kCancelSound = 0x14u;
constexpr std::uint32_t kCursorSound = 0x15u;

constexpr std::uint8_t kEquipmentKind = 0u;

// The hint font's CLUT rows: white for an effect that is on, dark brown for one that is off, yellow for the chooser's
// mark; the words around them keep the hint's own grey.
constexpr std::uint32_t kOnColour = 0x16u;
constexpr std::uint32_t kOffColour = 0x13u;
constexpr std::uint32_t kMarkColour = 0x11u;
constexpr std::string_view kLends = "Lends:";
constexpr std::string_view kMark = ">";
constexpr std::string_view kOpen = "SELECT:change";
// Bytes 0x01 and 0x02 draw the cross and circle buttons.
constexpr std::string_view kChoose = "\x01:on/off \x02:done";

constexpr std::size_t kMaxParts = 8;

struct Effects {
  std::array<const psx::game::TitleIntSetting *, kMaxParts> parts{};
  std::size_t count = 0;

  void add(std::span<const psx::game::TitleIntSetting> row) {
    for (const psx::game::TitleIntSetting &part : row) {
      if (count < parts.size()) {
        parts[count++] = &part;
      }
    }
  }
};

// The effects an item lends: its own row, then the row every heavier suit shares.
Effects effectsOf(int item) {
  Effects effects;
  if (item < 0) {
    return effects;
  }
  effects.add(ModSettings::suitEffects(static_cast<std::uint8_t>(item)));
  effects.add(ModSettings::heavierSuitEffects(static_cast<std::uint8_t>(item)));
  return effects;
}

void playSound(Core &core, std::uint32_t id) {
  psx::cpu::callGuestNow(core, __func__, kSound, id, 0u, 0u);
}

int width(std::string_view text) {
  return static_cast<int>(text.size()) * HintWriter::kGlyph;
}

} // namespace

int ItemsTab::selectedItem(Core &core) {
  using namespace subscreen;
  if (core.mem_r8(kState + kPhase) != kRunning || core.mem_r8(kState + kTab) != 0u ||
      core.mem_r8(kState + kEntered) == 0u || core.mem_r8(kState + kStep) != kBrowsing) {
    return -1;
  }
  const std::uint32_t item = psx::cpu::callGuestNowResult(
      core, __func__, kItemAt, kState, core.mem_r8(kState + kRow), core.mem_r8(kState + kColumn));
  return item == kNoItem ? -1 : static_cast<int>(item);
}

bool ItemsTab::hint(Core &core, const HintWriter &writer) const {
  if (core.mem_r8(subscreen::kState + subscreen::kHintKind) != kEquipmentKind) {
    return false;
  }
  const Effects effects = effectsOf(selectedItem(core));
  if (effects.count == 0) {
    return false;
  }
  // Words flow one glyph apart and wrap at the box's right edge, indented one glyph: the chooser's mark sits in the
  // space before its effect.
  HintWriter::Place at{.x = HintWriter::kLeft, .y = HintWriter::kTop};
  const auto place = [&at](std::string_view text) {
    if (at.x != HintWriter::kLeft && at.x + width(text) > HintWriter::kRight) {
      at = {.x = HintWriter::kLeft + HintWriter::kGlyph, .y = at.y + HintWriter::kLineHeight};
    }
    const HintWriter::Place placed = at;
    at.x += width(text) + HintWriter::kGlyph;
    return placed;
  };
  writer.text(place(kLends), HintWriter::kColour, kLends);
  for (std::size_t i = 0; i < effects.count; ++i) {
    const psx::game::TitleIntSetting &part = *effects.parts[i];
    const std::string_view label = part.label;
    const HintWriter::Place placed = place(label);
    writer.text(placed, part.on() ? kOnColour : kOffColour, label);
    if (editing_ && i == part_) {
      writer.text({.x = placed.x - HintWriter::kGlyph, .y = placed.y}, kMarkColour, kMark);
    }
  }
  const std::string_view prompt = editing_ ? kChoose : kOpen;
  writer.text(place(prompt), HintWriter::kColour, prompt);
  return true;
}

void ItemsTab::step(Core &core, std::uint32_t state) {
  const Effects effects = effectsOf(selectedItem(core));
  const std::uint16_t pressed = core.mem_r16(PadEdges::kPressed);
  if (!editing_) {
    if (effects.count == 0 || (pressed & PadEdges::kSelect) == 0u) {
      core.r[4] = state;
      psx::cpu::callOriginalToReturn(core, kTabStep, psx::cpu::ExecutionBudget::currentTurn(core), __func__);
      return;
    }
    editing_ = true;
    part_ = 0;
    playSound(core, kConfirmSound);
    return;
  }
  if (effects.count == 0 || (pressed & (PadEdges::kSelect | PadEdges::kCircle)) != 0u) {
    editing_ = false;
    playSound(core, kCancelSound);
    return;
  }
  if ((pressed & PadEdges::kCross) != 0u) {
    const psx::game::TitleIntSetting &part = *effects.parts[part_];
    part.cycle();
    core.game->mods.save();
    lucent::info("suit-effects", "{} {}", part.id, part.text());
    playSound(core, kConfirmSound);
    return;
  }
  const int dir = (pressed & PadEdges::kRight) != 0u ? 1 : (pressed & PadEdges::kLeft) != 0u ? -1 : 0;
  if (dir == 0) {
    return;
  }
  const std::ptrdiff_t next = static_cast<std::ptrdiff_t>(part_) + dir;
  if (next < 0 || next >= static_cast<std::ptrdiff_t>(effects.count)) {
    playSound(core, kRefuseSound);
    return;
  }
  part_ = static_cast<std::size_t>(next);
  playSound(core, kCursorSound);
}

void ItemsTab::ovStep(Core *c) {
  Session &session = sessionOf(c);
  // A replayed page step must not take the frame's presses a second time.
  if (session.drawReplay.frame().replaying()) {
    if (!session.itemsTab.editing_) {
      psx::cpu::callOriginalToReturn(*c, kTabStep, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
    }
    return;
  }
  session.itemsTab.step(*c, c->r[4]);
}

void ItemsTab::registerOverrides(native::NativeOverrideCatalog &natives) {
  natives.declare(kTabStep, "ItemsTab::step", ovStep);
}

} // namespace tomba
