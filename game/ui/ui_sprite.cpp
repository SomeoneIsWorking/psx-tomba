// game/ui/ui_sprite.cpp — the two thin entry points onto the shared 2D sprite emitter.
//
// Guest FUN_8007E8DC and FUN_8007E998, RE'd 2026-07-22 from gen (shard_5 / shard_2). Both were found
// by dispatch-tracing what actually runs while a message box is on screen in the bucket capture.
//
// WHAT THEY DO. Neither draws anything itself. Each marshals a couple of small structs ON ITS OWN
// GUEST STACK FRAME and hands them to FUN_8007E1B8, the real emitter:
//
//     placement (frame+24)   u16 x, u16 y, u32 0
//     attributes (frame+16)  u8 0, u8 attr, u16 0
//
// and passes, alongside them, a SPRITE DEFINITION pointer looked up in the table at 0x80017334
// (`table[defIndex]`, the index arriving as a 16-bit value scaled by 4) plus the model-table pointer
// held at 0x800ECF58.
//
// drawFixedDef152 is simply drawFromTable with defIndex pinned to 152. It has callers in at least
// four shards, so it is a common helper — but what def 152 *is* (it resolves to a four-entry list at
// 0x80017C00) is NOT established, so it is named for what it does rather than given an invented
// meaning. Rename it once the emitter FUN_8007E1B8 is RE'd and the def format is known.
//
// GUEST STACK. Both functions descend sp and spill ra, and the structs above live IN that frame —
// the pointers handed to the emitter point at guest stack memory. So the frames are reproduced
// exactly (CLAUDE.md "MIRROR THE GUEST STACK"): descend, spill ra at its offset, write the locals at
// theirs, restore and unwind. Not doing so would hand the emitter pointers into the wrong place.
#include "ui/ui_sprite.h"
#include "core.h"
#include "core/overrides/native_override_catalog.h"
#include "entry/session.h"
#include "guest_call.h"
#include "render/call_site_owner.h"
#include "render/guest_ordering_table.h"

#include "blend.h"
#include "guest_redraw.h"
#include "projected_vertex.h"
#include "render/crop_layers.h"

#include <array>

namespace tomba {

namespace {

constexpr uint32_t SPRITE_DEF_TABLE = 0x80017334u; // u32[]: index -> sprite definition
constexpr uint32_t MODEL_TABLE_PTR = 0x800ECF58u;  // u32 -> model table the emitter needs
constexpr int16_t DEF_FIXED_152 = 152;             // the index drawFixedDef152 pins

} // namespace

// FUN_8007E8DC(x r4, y r5, attr r6, defIndex r7)
// ORACLE: guest 0x8007E8DC
void UiSprite::drawFromTable(Core *c) {
  const uint32_t x = c->r[4];
  const uint32_t y = c->r[5];
  const uint32_t attr = c->r[6];
  const int32_t defIndex = (int32_t)(c->r[7] << 16) >> 16;

  c->r[29] -= 40;
  const uint32_t frame = c->r[29];

  const uint32_t placement = frame + 24;  // u16 x, u16 y, u32 0
  const uint32_t attributes = frame + 16; // u8 0, u8 attr, u16 0

  c->mem_w16(placement + 0, (uint16_t)x);
  c->mem_w32(frame + 32, c->r[31]); // ra spill
  c->mem_w8(attributes + 1, (uint8_t)attr);
  c->mem_w8(attributes + 0, 0);
  c->mem_w16(attributes + 2, 0);
  c->mem_w16(placement + 2, (uint16_t)y);
  c->mem_w32(placement + 4, 0);

  c->r[4] = placement;
  c->r[5] = c->mem_r32(SPRITE_DEF_TABLE + (uint32_t)(defIndex * 4));
  c->r[6] = c->mem_r32(MODEL_TABLE_PTR);
  c->r[7] = attributes;
  c->r[31] = 0x8007E928u; // jal-site ra
  psx::cpu::callGuestNow(*c, __func__, kQuads);

  c->r[31] = c->mem_r32(frame + 32);
  c->r[29] += 40;
}

// FUN_8007E998(x r4, y r5, attr r6) — drawFromTable with the definition index pinned to 152.
// ORACLE: guest 0x8007E998
void UiSprite::drawFixedDef152(Core *c) {
  c->r[29] -= 24;
  const uint32_t frame = c->r[29];

  c->r[4] = (uint32_t)((int32_t)(c->r[4] << 16) >> 16); // both coords arrive as 16-bit
  c->r[5] = (uint32_t)((int32_t)(c->r[5] << 16) >> 16);
  c->mem_w32(frame + 16, c->r[31]); // ra spill

  c->r[31] = 0x8007E9B8u; // jal-site ra
  c->r[7] = (uint32_t)DEF_FIXED_152;
  psx::cpu::callGuestNow(*c, __func__, 0x8007E8DCu); // through the thunk, so the port above runs

  c->r[31] = c->mem_r32(frame + 16);
  c->r[29] += 24;
}

namespace {

// Each emitter's packet run: one packet per piece, its vertex words.
struct PacketShape {
  std::uint32_t bytes;
  std::array<std::uint32_t, 4> vertices;
  std::uint32_t corners;
};
constexpr PacketShape kQuadShape{.bytes = UiSprite::kQuadBytes, .vertices = UiSprite::kQuadVertices, .corners = 4u};
constexpr PacketShape kSpriteShape{
    .bytes = UiSprite::kSpriteBytes, .vertices = {UiSprite::kSpriteVertex, 0u, 0u, 0u}, .corners = 1u};

template <std::uint32_t Entry> constexpr PacketShape kShape = Entry == UiSprite::kQuads ? kQuadShape : kSpriteShape;

// A replay's rewrite of a sprite the update block drew: every piece moved by the anchor's blended offset.
template <std::uint32_t Entry>
void rebuild(const psx::present::EmitMemory &memory,
             psx::present::GuestRedraw &redraw,
             const UpdateBlockDraws::Draw &earlier,
             const UpdateBlockDraws::Draw &later,
             float t) {
  if (earlier.values[2] != later.values[2] || earlier.values[3] != later.values[3]) {
    return;
  }
  constexpr PacketShape shape = kShape<Entry>;
  std::array<double, 2> offset{};
  for (std::size_t axis = 0; axis < offset.size(); ++axis) {
    const auto to = static_cast<std::int32_t>(later.values[axis]);
    offset[axis] = psx::present::lerpExact(static_cast<std::int32_t>(earlier.values[axis]), to, t) - to;
  }
  for (std::uint32_t packet = later.packets[0]; packet < later.packets[1]; packet += shape.bytes) {
    for (std::uint32_t corner = 0; corner < shape.corners; ++corner) {
      const std::uint32_t at = packet + shape.vertices[corner];
      redraw.writeVertex(at,
                         psx::gte::ProjectedVertex::nearest({static_cast<float>(memory.mem_r16s(at) + offset[0]),
                                                             static_cast<float>(memory.mem_r16s(at + 2u) + offset[1]),
                                                             0.0f,
                                                             0.0f}));
    }
  }
}

using render::CallSiteOwner;
constexpr std::uint8_t kS0 = 16;
constexpr std::uint8_t kS1 = 17;
constexpr std::uint8_t kS3 = 19;
// The update block's calls: the popups node behaviour 0x80072520 draws and the area label 0x80072898.
constexpr CallSiteOwner kOwners[] = {
    {.returnAddress = 0x800720CCu, .object = kS1, .element = kS0}, // 0x80071DFC: the AP popup node's digit s0
    {.returnAddress = 0x800724BCu, .object = kS3},                 // 0x80072308: the popup node's left sprite
    {.returnAddress = 0x80072500u, .object = kS3},                 // 0x80072308: its right or only sprite
    {.returnAddress = 0x80072A34u, .object = kS1, .element = kS0}, // 0x80072898: the label node's glyph s0 - 1
};

// A call in the update block is noted, as its caller's object and element, with its anchor and definition, so a
// replay places its pieces at t.
template <std::uint32_t Entry, void (*Body)(Core *)> void notedEntry(Core *c) {
  const std::optional<std::uint32_t> object = render::callSiteOwner(*c, kOwners);
  const std::uint32_t placement = c->r[4];
  const auto definition = static_cast<std::uint32_t>(c->mem_r16s(c->r[5]));
  const std::uint32_t defBase = c->r[6];
  const std::uint32_t first = render::PacketPool(*c).cursor();
  Body(c);
  if (!object) {
    sessionOf(c).drawReplay.updateDraws().unowned(*c, Entry, c->r[31]);
    return;
  }
  const std::uint32_t pieces =
      first + (static_cast<std::uint32_t>(c->mem_r16s(defBase + (definition * 4u))) * kShape<Entry>.bytes);
  sessionOf(c).drawReplay.updateDraws().note(*c,
                                             {.writer = Entry,
                                              .object = *object,
                                              .packets = {first, pieces},
                                              .values = {static_cast<std::uint32_t>(c->mem_r16s(placement)),
                                                         static_cast<std::uint32_t>(c->mem_r16s(placement + 2u)),
                                                         definition,
                                                         defBase},
                                              .rebuild = &rebuild<Entry>});
}

} // namespace

void UiSprite::registerOverrides(native::NativeOverrideCatalog &natives) {
  natives.declare(kQuads, "UiSprite::quads", notedEntry<kQuads, &UiSprite::quads>);
  natives.declare(kCompose, "UiSprite::compose", notedEntry<kCompose, &UiSprite::compose>);
  natives.declare(0x8007E8DCu, "UiSprite::drawFromTable", render::uiDraw<&UiSprite::drawFromTable>);
  natives.declare(0x8007E998u, "UiSprite::drawFixedDef152", render::uiDraw<&UiSprite::drawFixedDef152>);
}

} // namespace tomba
