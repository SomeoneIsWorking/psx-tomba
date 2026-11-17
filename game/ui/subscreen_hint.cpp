#include "ui/subscreen_hint.h"

#include "core.h"
#include "core/overrides/native_override_catalog.h"
#include "entry/session.h"
#include "guest_abi.h"
#include "guest_call.h"
#include "render/crop_layers.h"
#include "ui/font.h"
#include "ui/items_tab.h"

namespace tomba {
namespace {

constexpr std::uint32_t kDraw = 0x80033FF4u;
constexpr std::uint32_t kTemplateQuads = 0x8007E1B8u; // (place, template, templates, attributes)
constexpr std::uint32_t kTemplateTable = 0x80017334u; // template pointers by index
constexpr std::uint32_t kBoxParts = 0x800A241Cu;      // 3 x {u8 attribute, pad, s16 x, s16 template}
constexpr std::uint32_t kBoxPartStride = 6u;
constexpr std::uint32_t kStripTemplates = 0x800A2430u; // 18 template indexes, 16 pixels apart from x 0x1A
constexpr int kStripCount = 18;
constexpr int kStripLeft = 0x1A;
constexpr int kStripStep = 0x10;
constexpr int kBoxTop = 200;
constexpr std::uint16_t kHelpEntered = 0x103u; // tab 3 with the cursor inside: the box's first part is not drawn
constexpr std::uint32_t kTextInFront = 4u;

// The hint strings, each a pointer to the text.
constexpr std::uint32_t kItemsHint = 0x800A3A68u;
constexpr std::uint32_t kEventsHint = 0x800A3A7Cu;
constexpr std::uint32_t kStatusHint = 0x800A3AC0u;
constexpr std::uint32_t kHelpHint = 0x800A3AC4u;
constexpr std::uint32_t kEventsEnteredHint = 0x800A3A80u;
constexpr std::uint32_t kEquipmentHint = 0x800A3A6Cu; // kind 0
constexpr std::uint32_t kCommandHint = 0x800A3A70u;   // kind 1
constexpr std::uint32_t kOtherHint = 0x800A3A74u;     // kind 2
constexpr std::uint32_t kItemNotes = 0x800A2BF0u;     // g_ItemText[id] + 8: the item's note
constexpr std::uint32_t kItemTextStride = 12u;
constexpr std::uint8_t kNoteKind = 4u;

constexpr GuestFrameSpill kSpills[] = {{31, 0x5C}};
constexpr std::uint32_t kAttributesAt = 0x10u; // u8 attribute, u8 bucket, u16
constexpr std::uint32_t kPlaceAt = 0x18u;      // s16 x, s16 y, s16 w, s16 h
constexpr std::uint32_t kTextAt = 0x20u;

// The hint text's pointer for the open tab, or 0 for none.
std::uint32_t hintText(Core &core, std::uint32_t state) {
  using namespace subscreen;
  const std::uint8_t tab = core.mem_r8(state + kTab);
  if (core.mem_r8(state + kEntered) == 0u) {
    constexpr std::uint32_t kByTab[] = {kItemsHint, kEventsHint, kStatusHint, kHelpHint};
    return tab < 4u ? core.mem_r32(kByTab[tab]) : 0u;
  }
  if (tab == 1u) {
    return core.mem_r32(kEventsEnteredHint);
  }
  if (tab != 0u) {
    return 0u;
  }
  switch (core.mem_r8(state + kHintKind)) {
  case 0u:
    return core.mem_r32(kEquipmentHint);
  case 1u:
    return core.mem_r32(kCommandHint);
  case 2u:
    return core.mem_r32(kOtherHint);
  case kNoteKind: {
    const std::uint32_t item = psx::cpu::callGuestNowResult(
        core, __func__, kItemAt, state, core.mem_r8(state + kRow), core.mem_r8(state + kColumn));
    return core.mem_r32(kItemNotes + item * kItemTextStride);
  }
  default:
    return 0u;
  }
}

} // namespace

void HintWriter::text(Place place, std::uint32_t colour, std::string_view text) const {
  const std::size_t length = text.size() < kMaxText ? text.size() : kMaxText;
  for (std::size_t i = 0; i < length; ++i) {
    core_.mem_w8(target_.buffer + static_cast<std::uint32_t>(i), static_cast<std::uint8_t>(text[i]));
  }
  core_.mem_w8(target_.buffer + static_cast<std::uint32_t>(length), 0u);
  Font::drawTextSmall(&core_,
                      {.x = place.x,
                       .y = place.y,
                       .w = static_cast<std::int32_t>(colour),
                       .str = target_.buffer,
                       .color = target_.bucket});
}

void SubscreenHint::draw(Core *c) {
  const std::uint32_t state = c->r[4];
  const std::uint32_t bucket = c->r[5] & 0xFFFFu;
  GuestFrame<0x60, 1> frame(c, kSpills);
  const std::uint32_t sp = c->r[psx::cpu::kSp];
  const std::uint32_t attributes = sp + kAttributesAt;
  const std::uint32_t place = sp + kPlaceAt;
  const std::uint32_t templates = c->mem_r32(state + subscreen::kTemplates);
  const std::string_view owner = __func__;
  const auto quads = [&](std::uint32_t templateIndex) {
    psx::cpu::callGuestNow(
        *c, owner, kTemplateQuads, place, c->mem_r32(kTemplateTable + templateIndex * 4u), templates, attributes);
  };

  c->mem_w8(attributes + 1u, static_cast<std::uint8_t>(bucket));
  c->mem_w16(attributes + 2u, 0u);
  c->mem_w16(place + 2u, kBoxTop);
  c->mem_w32(place + 4u, 0u);
  for (std::uint32_t i = 0; i < 3u; ++i) {
    if (i == 0u && c->mem_r16(state + subscreen::kTab) == kHelpEntered) {
      continue;
    }
    const std::uint32_t part = kBoxParts + i * kBoxPartStride;
    c->mem_w8(attributes, c->mem_r8(part));
    c->mem_w16(place, c->mem_r16(part + 2u));
    quads(static_cast<std::uint32_t>(static_cast<std::int16_t>(c->mem_r16(part + 4u))));
  }
  c->mem_w8(attributes, 0u);
  for (int i = 0; i < kStripCount; ++i) {
    c->mem_w16(place, static_cast<std::uint16_t>(kStripLeft + i * kStripStep));
    quads(c->mem_r8(kStripTemplates + static_cast<std::uint32_t>(i)));
  }

  const HintWriter writer(*c, {.buffer = sp + kTextAt, .bucket = bucket - kTextInFront});
  if (sessionOf(c).itemsTab.hint(*c, writer)) {
    return;
  }
  const std::uint32_t text = hintText(*c, state);
  if (text != 0u) {
    Font::drawTextSmall(c,
                        {.x = HintWriter::kLeft,
                         .y = HintWriter::kTop,
                         .w = static_cast<std::int32_t>(HintWriter::kColour),
                         .str = text,
                         .color = bucket - kTextInFront});
  }
}

void SubscreenHint::registerOverrides(native::NativeOverrideCatalog &natives) {
  natives.declare(kDraw, "SubscreenHint::draw", render::uiDraw<draw>);
}

} // namespace tomba
