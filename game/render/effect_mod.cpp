// Secondary EFFECT modifiers — the five post-passes that re-colour / re-blend an object's packets
// after they have been built.
//
// SHAPE (all five are the same): perObjRenderDispatch snapshots the packet-pool write pointer, calls
// cmdListDispatch to build the object's primitives, snapshots it again, and hands the resulting
// [lo,hi) span to ONE of these. Each walks the span packet-by-packet and rewrites the packets in
// place. Nothing here allocates, links, or reads the OT — it is purely "go back over what was just
// emitted and change its colour/blend/palette".
//
// These mutate guest memory (the packet pool is guest RAM), so they are faithful ports. The full RE — including two
// places where the Ghidra listing is WRONG and authenticated executable/overlay evidence is the ground truth — is in
// docs/findings/render.md, "The secondary- effect handlers". The two facts that most shape the code below:
//
//   * A packet's colour word is a u32 at pkt+4, which SPANS pkt+4..pkt+7 — i.e. it overwrites the
//     command byte at pkt+7. So effectFlatTint must read the command byte BEFORE writing colour and
//     put it back afterwards. (effectColorAdd is safe: it writes the three colour BYTES individually
//     and never touches pkt+7.)
//   * The walk is a real 29-entry jump table indexed by (cmd & 0xFC) - 0x20, not a computed stride,
//     and an unrecognised opcode branches to the loop tail WITHOUT advancing — the guest spins
//     forever. That cannot happen with valid pool data; we FAIL-FAST rather than reproduce a hang or
//     silently skip (either would be a different behaviour).
#include "cfg.h"
#include "core.h"
#include "render.h"
#include <stdlib.h>

namespace {

// ─────────────────────────────────────────────────────────────────────────────────────────────────
// GpuPacket — typed lens over one GP0 packet in the render pool. Same guest addresses as the raw
// `c->mem_rXX(p + 0xNN)` these replace (SBS byte-compares the underlying RAM); the point is that a
// reader sees `pkt.opcode()` / `pkt.setColour(i, rgb)` instead of offset arithmetic.
//
// Layout (the part these passes care about):
//   +0x00  link/tag word
//   +0x04  colour word 0 — bytes R=+4, G=+5, B=+6, and the COMMAND BYTE at +7
//   +0x07  command byte (semi-transparency is bit 1)
//   +0x0E  CLUT id (u16)
//   +0x10 / +0x1C / +0x28  colour words 1..3, for the multi-vertex (gouraud) primitives
struct GpuPacket {
  Core *c;
  uint32_t base;

  static constexpr uint8_t kSemiBit = 0x02;
  // Colour-word offsets, in the order the guest writes them. Index 0 is the one every colour-bearing
  // packet has; a gouraud tri uses 0..2 and a gouraud quad 0..3.
  static constexpr uint32_t kColourWord[4] = {0x04u, 0x10u, 0x1Cu, 0x28u};

  uint8_t command() const {
    return c->mem_r8(base + 0x07u);
  }
  void setCommand(uint8_t v) {
    c->mem_w8(base + 0x07u, v);
  }
  uint8_t opcode() const {
    return command() & 0xFCu;
  }

  void setClut(uint16_t v) {
    c->mem_w16(base + 0x0Eu, v);
  }

  uint32_t colourAddr(int i) const {
    return base + kColourWord[i];
  }
  void setColour(int i, uint32_t rgb) {
    c->mem_w32(colourAddr(i), rgb);
  }
  // Individual channel bytes of colour word `i`: 0=R, 1=G, 2=B. Used by the modulating pass, which
  // must NOT write the whole word (that would clobber the command byte sharing it).
  uint8_t channel(int i, int ch) const {
    return c->mem_r8(colourAddr(i) + (uint32_t)ch);
  }
  void setChannel(int i, int ch, uint8_t v) {
    c->mem_w8(colourAddr(i) + (uint32_t)ch, v);
  }
};

// ─────────────────────────────────────────────────────────────────────────────────────────────────
// EffectParams — typed lens over the effect fields of the object node driving the pass.
struct EffectParams {
  Core *c;
  uint32_t node;

  uint32_t tintRgb() const {
    return c->mem_r32(node + 0x18u);
  } // flat-tint colour word, written whole
  uint8_t addR() const {
    return c->mem_r8(node + 0x18u);
  } // the SAME three bytes, read
  uint8_t addG() const {
    return c->mem_r8(node + 0x19u);
  } // per-channel by the modulating pass
  uint8_t addB() const {
    return c->mem_r8(node + 0x1Au);
  }
  uint16_t clut() const {
    return c->mem_r16(node + 0x5Cu);
  }
};

// ─────────────────────────────────────────────────────────────────────────────────────────────────
// PacketShape — everything the walkers need to know about a packet, keyed by its masked opcode.
// Decoded from the guest jump tables in MAIN.EXE (0x80015088 / 0x800151F0 / 0x80014F48 — all three
// agree). `colourWords` is 0 for the primitives these passes only step over, and otherwise tracks
// the primitive's vertex count.
struct PacketShape {
  uint8_t stride;      // bytes to the next packet
  uint8_t colourWords; // colour words carried (0 = not colour-bearing -> stride only)
};

PacketShape shapeOf(uint8_t opcode) {
  switch (opcode) {
  case 0x20u:
    return {20, 0};
  case 0x24u:
    return {32, 1};
  case 0x28u:
    return {24, 0};
  case 0x2Cu:
    return {40, 1};
  case 0x30u:
    return {28, 0};
  case 0x34u:
    return {40, 3};
  case 0x38u:
    return {36, 0};
  case 0x3Cu:
    return {52, 4};
  default:
    return {0, 0}; // unreachable with valid pool data — see walkPackets
  }
}

// walkPackets — the shared span walk. Calls `apply(pkt, shape)` for every colour-bearing packet and
// steps over the rest. `what` names the pass in the fail-fast diagnostic.
template <class Apply> void walkPackets(Core *c, uint32_t lo, uint32_t hi, const char *what, Apply &&apply) {
  for (uint32_t p = lo; p < hi;) {
    GpuPacket pkt{c, p};
    const PacketShape shape = shapeOf(pkt.opcode());
    if (shape.stride == 0) {
      // The guest's table sends an unrecognised opcode to the loop tail without advancing, i.e. it
      // hangs. Never reachable from a well-formed pool; refuse loudly instead of spinning.
      cfg_loge("effectmod",
               "%s: unrecognised GP0 opcode 0x%02X at packet 0x%08X in span "
               "[0x%08X,0x%08X) — the packet pool is malformed",
               what,
               pkt.opcode(),
               p,
               lo,
               hi);
      abort();
    }
    if (shape.colourWords) {
      apply(pkt, shape);
    }
    p += shape.stride;
  }
}

} // namespace

// FUN_8003F3F4 — turn semi-transparency ON for every colour-bearing packet in the span.
void Render::effectSemiOn(uint32_t, uint32_t lo, uint32_t hi) {
  walkPackets(mCore, lo, hi, "semiOn", [](GpuPacket pkt, PacketShape) {
    pkt.setCommand(pkt.command() | GpuPacket::kSemiBit);
  });
}

// FUN_8003F4C4 — the exact inverse: turn semi-transparency OFF.
void Render::effectSemiOff(uint32_t, uint32_t lo, uint32_t hi) {
  walkPackets(mCore, lo, hi, "semiOff", [](GpuPacket pkt, PacketShape) {
    pkt.setCommand(pkt.command() & (uint8_t)~GpuPacket::kSemiBit);
  });
}

// FUN_8003F344 — stamp the node's CLUT id onto every colour-bearing packet, repointing them at a
// different texture palette. One u16 per packet regardless of vertex count.
void Render::effectClutSwap(uint32_t node, uint32_t lo, uint32_t hi) {
  Core *c = mCore;
  const EffectParams fx{c, node};
  walkPackets(c, lo, hi, "clutSwap", [&](GpuPacket pkt, PacketShape) {
    pkt.setClut(fx.clut()); // re-read per packet, as the guest body does
  });
}

// FUN_8003F594 — overwrite the packet's colour word(s) with one flat colour and force semi on.
// Order matters: the colour write covers the command byte, so save and restore it (see file header).
void Render::effectFlatTint(uint32_t node, uint32_t lo, uint32_t hi) {
  Core *c = mCore;
  const uint32_t rgb = EffectParams{c, node}.tintRgb(); // read ONCE, before the walk
  walkPackets(c, lo, hi, "flatTint", [&](GpuPacket pkt, PacketShape shape) {
    const uint8_t cmd = pkt.command(); // BEFORE the colour write clobbers it
    for (int i = 0; i < shape.colourWords; i++) {
      pkt.setColour(i, rgb);
    }
    pkt.setCommand(cmd | GpuPacket::kSemiBit); // restore, with semi set
  });
}

// FUN_8003D584 — modulate each colour channel by the node's per-channel amount, rather than
// replacing it. The byte encoding is asymmetric and deliberately preserved as-is:
//     t <= 0x7F  ->  signed offset around 0x7F: p + (t - 0x7F), clamped at 0     (darken)
//     t == 0x80  ->  no write at all                                             (identity)
//     t >= 0x81  ->  saturating add: p + t, clamped at 255                       (brighten)
// Writes the three colour BYTES individually, never the whole word, so the command byte is untouched.
void Render::effectColorAdd(uint32_t node, uint32_t lo, uint32_t hi) {
  Core *c = mCore;
  const EffectParams fx{c, node};
  walkPackets(c, lo, hi, "colorAdd", [&](GpuPacket pkt, PacketShape shape) {
    const uint8_t amount[3] = {fx.addR(), fx.addG(), fx.addB()};
    for (int i = 0; i < shape.colourWords; i++) {
      for (int ch = 0; ch < 3; ch++) {
        const uint8_t t = amount[ch];
        if (t == 0x80u) {
          continue; // identity — the guest writes nothing
        }
        const int32_t p = (int32_t)pkt.channel(i, ch);
        const int32_t v = (t < 0x80u) ? (p + ((int32_t)t - 0x7F)) // darken, clamp low
                                      : (p + (int32_t)t);         // brighten, clamp high
        const int32_t clamped = (t < 0x80u) ? (v < 0 ? 0 : v) : (v > 255 ? 255 : v);
        pkt.setChannel(i, ch, (uint8_t)clamped);
      }
    }
  });
}
