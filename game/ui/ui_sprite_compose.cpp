// game/ui/ui_sprite_compose.cpp — UiSprite::compose, the multi-piece 2D sprite emitter.
//
// Guest FUN_8007E6DC, RE'd 2026-07-22 from guest 0x8007E6DC (shard_2). This is what
// UiSprite::drawFromTable/drawFixedDef152 hand their marshalled structs to, and it is what actually
// puts sprite packets in the ordering table.
//
// WHAT IT DOES. A 2D sprite here is not one quad: a DEFINITION names a run of PIECES, and this walks
// that run emitting one textured-sprite packet each, then closes with a draw-mode packet.
//
//   definition entry (defBase + index*4):   s16 pieceCount, s16 pieceOffset
//   piece (16 bytes, walked in step):       u32 uv+clut word at +0
//                                           u16 tpage at +6   (read once, for the tail packet)
//                                           u8  width at +10, u8 height at +11
//                                           s8  dx at +14,    s8  dy at +15
//
// (The piece fields are reached through a cursor at piece+11, which is why the offsets below read
// -1/0/3/4 off it — that is the guest's own addressing, kept so the walk steps identically.)
//
// STAGING. Rather than build each packet in place, the guest fills a five-word TEMPLATE in scratchpad
// at 0x1F800004 and block-copies it into the packet. So the writes below go to the template first and
// only then to the pool — that ordering is the function's shape, not an artefact.
//
// ATTRIBUTES (the little struct the callers marshalled):
//   +0 u8  flags     — low nibble kept and written BACK; high nibble is a flat shade level
//   +1 u8  otBucket  — which ordering-table bucket the packets link into
//   +2 u16 attr      — bit 15 selects the semi-transparent sprite opcode; low 15 bits, if any,
//                      override the CLUT half of the uv word
#include "core.h"
#include "guest_call.h"
#include "render/guest_ordering_table.h"
#include "ui/ui_sprite.h"

namespace {

constexpr uint32_t STAGE = 0x1F800000u; // scratchpad packet template lives at +4..+18

constexpr uint8_t GP0_SPRITE = 100;      // 0x64 textured sprite
constexpr uint8_t GP0_SPRITE_SEMI = 102; // 0x66 same, semi-transparent
constexpr uint8_t RAW_TEXTURE_BIT = 1;   // set when no flat shade is applied

constexpr uint32_t SPRITE_TAG_WORDS = 4u;
constexpr uint32_t MODE_TAG_WORDS = 2u;
constexpr uint32_t SPRITE_PACKET_BYTES = 20u;
constexpr uint32_t MODE_PACKET_BYTES = 12u;

} // namespace

// FUN_8007E6DC(placement r4, indexPtr r5, defBase r6, attrs r7)
// ORACLE: guest 0x8007E6DC
void UiSprite::compose(Core *c) {
  const uint32_t placement = c->r[4];
  const uint32_t indexPtr = c->r[5];
  const uint32_t defBase = c->r[6];
  const uint32_t attrs = c->r[7];

  c->r[29] -= 40;
  const uint32_t frame = c->r[29];
  c->mem_w32(frame + 24, c->r[16]);
  c->mem_w32(frame + 32, c->r[18]);
  c->mem_w32(frame + 36, c->r[31]);
  c->mem_w32(frame + 28, c->r[17]);

  const uint8_t flags = c->mem_r8(attrs + 0);
  const uint16_t attrWord = c->mem_r16(attrs + 2);
  const uint32_t shade = flags & 0xF0u; // flat shade level, or 0 for raw texture
  const bool semi = (attrWord & 0x8000u) != 0;
  const uint32_t clutOverride = attrWord & 0x7FFFu;
  c->mem_w8(attrs + 0, (uint8_t)(flags & 0x0Fu)); // the guest keeps only the low nibble

  const uint32_t entry = defBase + (uint32_t)(((int32_t)(int16_t)c->mem_r16(indexPtr)) * 4);
  int32_t count = (int16_t)c->mem_r16(entry + 0);
  uint32_t piece = defBase + (uint32_t)(int32_t)(int16_t)c->mem_r16(entry + 2);
  uint32_t cursor = piece + 11;
  const uint16_t tpage = c->mem_r16(piece + 6);

  do {
    // position: the caller's x/y pair, nudged by this piece's signed offsets
    c->mem_w32(STAGE + 8, c->mem_r32(placement));
    c->mem_w16(STAGE + 8, (uint16_t)(c->mem_r16(STAGE + 8) + (int8_t)c->mem_r8(cursor + 3)));
    c->mem_w16(STAGE + 10, (uint16_t)(c->mem_r16(STAGE + 10) + (int8_t)c->mem_r8(cursor + 4)));

    c->mem_w32(STAGE + 12, c->mem_r32(piece));     // uv + clut
    c->mem_w16(STAGE + 16, c->mem_r8(cursor - 1)); // width
    c->mem_w8(STAGE + 7, GP0_SPRITE);
    c->mem_w16(STAGE + 18, c->mem_r8(cursor + 0)); // height
    if (semi) {
      c->mem_w8(STAGE + 7, GP0_SPRITE_SEMI);
    }

    if (shade) { // flat-shaded: same level on r,g,b
      c->mem_w8(STAGE + 6, (uint8_t)shade);
      c->mem_w8(STAGE + 5, (uint8_t)shade);
      c->mem_w8(STAGE + 4, (uint8_t)shade);
    } else { // untinted: raw texture
      c->mem_w8(STAGE + 7, (uint8_t)(c->mem_r8(STAGE + 7) | RAW_TEXTURE_BIT));
    }
    if (clutOverride) {
      c->mem_w16(STAGE + 14, c->mem_r16(attrs + 2));
    }

    // link the packet, then copy the template into it
    const tomba2::render::PacketPool packets(*c);
    const uint32_t packet = packets.cursor();
    tomba2::render::OrderingTable::active(*c).link(packet, SPRITE_TAG_WORDS, c->mem_r8(attrs + 1));

    cursor += 16;
    piece += 16;
    count -= 1;
    c->mem_w32(packet + 4, c->mem_r32(STAGE + 4));
    c->mem_w32(packet + 8, c->mem_r32(STAGE + 8));
    c->mem_w32(packet + 12, c->mem_r32(STAGE + 12));
    c->mem_w32(packet + 16, c->mem_r32(STAGE + 16));
    packets.setCursor(packet + SPRITE_PACKET_BYTES);
  } while (count != 0);

  // tail: one draw-mode packet carrying the definition's tpage, linked in front of the sprites
  const tomba2::render::PacketPool packets(*c);
  const uint32_t modePacket = packets.cursor();
  c->r[16] = modePacket;
  c->mem_w32(frame + 16, 0); // 5th arg of the builder, passed on the stack
  c->r[4] = modePacket;
  c->r[5] = 0;
  c->r[6] = 0;
  c->r[7] = (uint32_t)(int32_t)(int16_t)tpage;
  c->r[31] = 0x8007E890u; // jal-site ra
  psx::cpu::dispatchGuestToReturn0(*c, 0x80083DE0u, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);

  tomba2::render::OrderingTable::active(*c).link(modePacket, MODE_TAG_WORDS, c->mem_r8(attrs + 1));
  packets.setCursor(packets.cursor() + MODE_PACKET_BYTES);

  c->r[31] = c->mem_r32(frame + 36);
  c->r[18] = c->mem_r32(frame + 32);
  c->r[17] = c->mem_r32(frame + 28);
  c->r[16] = c->mem_r32(frame + 24);
  c->r[29] += 40;
}
