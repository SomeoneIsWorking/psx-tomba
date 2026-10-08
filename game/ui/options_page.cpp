// class OptionsPage — implementation. See options_page.h.
#include "options_page.h"
#include "core.h"
#include "core/overrides/native_override_catalog.h"
#include "render/guest_ordering_table.h"

namespace {

// ---- FUN_8007FC24's guest state (the same pool/table the dialog backdrop uses) ------------------
constexpr uint32_t kPacketBytes = 36u; // tag + 8 command words (POLY_G4)
constexpr uint32_t kOtBucketNear = 1u; // the bucket the backdrop links into
constexpr uint8_t kGp0PolyG4 = 0x38u;  // shaded four-point polygon, opaque
constexpr uint8_t kTagWordCount = 8u;

// The gradient's four vertex colours and corners, verbatim from the packet the guest builds: three
// corners the page blue, the bottom-right one darker.
constexpr uint8_t kBlueBright = 70u;
constexpr uint8_t kBlueDark = 16u;
constexpr uint16_t kScreenW = 320u;
constexpr uint16_t kScreenH = 240u;

} // namespace

// ORACLE: guest 0x8007FC24
void OptionsPage::pushBackdrop(Core *c) {
  // Take a packet off the pool and bump it, exactly as the guest allocator does.
  const uint32_t packet = tomba2::render::PacketPool(*c).allocate(kPacketBytes);

  // POLY_G4 body: per-vertex colour word (RGB in the low three bytes, the GP0 opcode riding in the
  // top byte of the FIRST one) followed by that vertex's screen XY. The 4th byte of the other three
  // colour words is left alone — the guest never writes it, so neither does this.
  c->mem_w8(packet + 7, kGp0PolyG4);
  c->mem_w8(packet + 6, kBlueBright);  // v0 blue
  c->mem_w8(packet + 14, kBlueBright); // v1 blue
  c->mem_w8(packet + 22, kBlueBright); // v2 blue
  c->mem_w8(packet + 30, kBlueDark);   // v3 blue (the darkened bottom-right corner)
  c->mem_w8(packet + 4, 0);
  c->mem_w8(packet + 5, 0); // v0 red/green
  c->mem_w8(packet + 12, 0);
  c->mem_w8(packet + 13, 0); // v1
  c->mem_w8(packet + 20, 0);
  c->mem_w8(packet + 21, 0); // v2
  c->mem_w8(packet + 28, 0);
  c->mem_w8(packet + 29, 0); // v3
  c->mem_w16(packet + 8, 0);
  c->mem_w16(packet + 10, 0); // v0 (0,0)
  c->mem_w16(packet + 16, kScreenW);
  c->mem_w16(packet + 18, 0); // v1 (320,0)
  c->mem_w16(packet + 24, 0);
  c->mem_w16(packet + 26, kScreenH); // v2 (0,240)
  c->mem_w16(packet + 32, kScreenW);
  c->mem_w16(packet + 34, kScreenH); // v3 (320,240)

  // Link it at the head of its bucket: the packet inherits whatever was there, and becomes the head.
  tomba2::render::OrderingTable::active(*c).link(packet, kTagWordCount, kOtBucketNear);
}

namespace {

void backdropEmit(Core *c) {
  OptionsPage::pushBackdrop(c);
}

} // namespace

void OptionsPage::install() {
  static bool done = false;
  if (done) {
    return;
  }
  done = true;
  tomba::native::declareOverride(0x8007FC24u, "OptionsPage::backdrop", backdropEmit);
}
