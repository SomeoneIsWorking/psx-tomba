// game/ui/dialog_backdrop.cpp — the flat-colour rectangle behind the dialog/message box.
//
// Guest FUN_8007FCC8, RE'd 2026-07-22 from guest 0x8007FCC8 (the guest instruction stream) with
// Ghidra for shape. Confirmed LIVE on the dialog path: dispatch-tracing the bucket-pickup capture
// shows the dialog-box glyph emitter (0x8007CC00) and this region running together while a message
// box is on screen.
//
// WHAT IT IS. One GPU packet: a flat-shaded rectangle (GP0 0x60), linked into the ordering table so
// it draws behind the glyphs. That is all — it is the panel the text sits on. The guest builds it by
// hand rather than through the 9-slice Panel builders, which is why it looks like nothing else here.
//
// THE PACKET. PSX ordering-table entries are singly-linked lists built back-to-front: word 0 is the
// TAG — the next pointer in its low 24 bits and the packet's word count in its top byte — and the
// rest is the GP0 command. So a size-3 flat rect is:
//
//     +0   tag        : next-pointer | (3 << 24)          (word count in the top byte, +3 as a u8)
//     +4   colour+cmd : 0x00RRGGBB with the GP0 opcode in the top byte (written separately at +7)
//     +8   x, +10 y   : top-left, screen space
//     +12  w, +14 h   : extent
//
// The odd-looking byte pokes at +3 and +7 are how the guest writes the tag's size field and the GP0
// opcode without disturbing the pointer/colour they share a word with; they are kept because they are
// what the packet layout IS, not a transcription artefact.
//
// ARGUMENT NOTE (this is the part that bites): x/y/w/h arrive in r4-r7, but the MODE is a FIFTH
// argument and therefore comes off the CALLER'S STACK at sp+16 under the o32 ABI — not a register.
// Reading it from a register would silently pick up whatever was left there.
//
// MODE bits:
//   0x80  -> link into the FAR ot bucket (0x7FF) instead of the near one (1), i.e. draw behind
//   0x7F  -> any of these set means "no fill colour" (black); all clear means the panel blue.
#include "core.h"
#include "core/overrides/native_override_catalog.h"
#include "game.h"
#include "render/ordering_table.h"
#include "ui/panel.h"

namespace {

constexpr uint32_t PACKET_BYTES = 16u; // tag + 3 command words

constexpr uint8_t GP0_FLAT_RECT = 0x60; // monochrome rectangle, variable size
constexpr uint8_t TAG_WORD_COUNT = 3;
constexpr uint32_t PANEL_FILL_RGB = 0x460000u; // the box blue (BGR order: R=0x00 G=0x00 B=0x46)

constexpr uint32_t MODE_FAR_BUCKET = 0x80u;
constexpr uint32_t MODE_NO_FILL = 0x7Fu;
constexpr uint32_t OT_BUCKET_FAR = 0x7FFu;
constexpr uint32_t OT_BUCKET_NEAR = 1u;

} // namespace

// ORACLE: guest 0x8007FCC8
void Panel::pushDialogBackdrop(Core *c, int16_t x, int16_t y, int16_t w, int16_t h, uint32_t mode) {
  const uint32_t bucket = (mode & MODE_FAR_BUCKET) ? OT_BUCKET_FAR : OT_BUCKET_NEAR;

  // Take a packet off the pool and bump it, exactly as the guest allocator does.
  const uint32_t packet = tomba2::render::PacketPool(*c).allocate(PACKET_BYTES);

  // Written as two branches rather than one ternary because that is the shape of the guest code —
  // it stores the colour word on one path and zero on the other, and the store sequence is part of
  // what port_check proves equivalent.
  if (mode & MODE_NO_FILL) {
    c->mem_w32(packet + 4, 0u);
  } else {
    c->mem_w32(packet + 4, PANEL_FILL_RGB);
  }
  c->mem_w8(packet + 3, TAG_WORD_COUNT); // size field of the tag word
  c->mem_w8(packet + 7, GP0_FLAT_RECT);  // opcode byte of the colour word
  c->mem_w16(packet + 8, (uint16_t)x);
  c->mem_w16(packet + 10, (uint16_t)y);
  c->mem_w16(packet + 12, (uint16_t)w);
  c->mem_w16(packet + 14, (uint16_t)h);

  // Link it at the head of its bucket: the packet inherits whatever was there, and becomes the head.
  tomba2::render::OrderingTable::active(*c).link(packet, TAG_WORD_COUNT, bucket);
}

// Guest-ABI entry: x/y/w/h in r4-r7, mode off the caller's stack (see the ARGUMENT NOTE above).
static void ov_push_dialog_backdrop(Core *c) {
  const int16_t x = (int16_t)c->r[4], y = (int16_t)c->r[5];
  const int16_t w = (int16_t)c->r[6], h = (int16_t)c->r[7];
  const uint32_t mode = c->mem_r32(c->r[29] + 16u);
  Panel::pushDialogBackdrop(c, x, y, w, h, mode);
}

void dialog_backdrop_install() {
  tomba::native::declareOverride(0x8007FCC8u, "Panel::pushDialogBackdrop", ov_push_dialog_backdrop);
}
