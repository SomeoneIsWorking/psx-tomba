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
#include "guest_call.h"

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
  psx::cpu::dispatchGuestToReturn0(*c, 0x8007E1B8u, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);

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
  psx::cpu::dispatchGuestToReturn0(*c,
                                   0x8007E8DCu,
                                   psx::cpu::ExecutionBudget::currentTurn(*c),
                                   __func__); // through the thunk, so the port above runs

  c->r[31] = c->mem_r32(frame + 16);
  c->r[29] += 24;
}

static void ov_compose(Core *c) {
  UiSprite::compose(c);
}
static void ov_draw_from_table(Core *c) {
  UiSprite::drawFromTable(c);
}
static void ov_draw_fixed_def152(Core *c) {
  UiSprite::drawFixedDef152(c);
}

void ui_sprite_install() {
  tomba::native::declareOverride(0x8007E6DCu, "UiSprite::compose", ov_compose);
  tomba::native::declareOverride(0x8007E8DCu, "UiSprite::drawFromTable", ov_draw_from_table);
  tomba::native::declareOverride(0x8007E998u, "UiSprite::drawFixedDef152", ov_draw_fixed_def152);
}
