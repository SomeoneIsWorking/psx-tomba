// game/ai/placed_prop_sm.h — the per-frame behaviour handler for PLACED SCENE PROPS (guest 0x80040558).
//
// WHAT IT IS, AND HOW THAT WAS ESTABLISHED
// ----------------------------------------
// 0x80040558 is not called by name anywhere: it is INSTALLED as a node's per-object handler pointer
// (node[+0x1C]) and reached from the object-list walk (FUN_8007A904 -> ObjectList::walkAllFaithful ->
// BehaviorDispatch::dispatchObj). Four spawn sites install it, and they are the identity evidence:
//
//   * FUN_8004D8D8 (guest 0x8004D8D8, resident) — the AREA PLACEMENT-RECORD walker. It walks the
//     per-area 0x10-byte record table at *(u32*)(0x800A3F00 + 4*area) until the 0xFF terminator, and
//     for every record whose CLASS byte (record[+1]) is 2 it allocates a node (FUN_80072DDC) and does
//         node[+0x1C] = 0x80040558;  node[+2] = 8;
//     (every other class gets 0x8004AAC4 instead). The record then seeds the node's world position
//     (record[+4/+6/+8] -> node[+0x2C/+0x30/+0x34] << 16), its variant (record[+3] -> node[+3]) and —
//     the important one — record[+13] -> node[+0x5E], the KIND byte this state machine dispatches on.
//     So the "kind" is a per-placement designer choice baked into the area's record table.
//   * overlay guest 0x80127E94, overlay guest 0x8011F5FC, overlay guest 0x80114874 — three overlay spawners that
//     install the same handler for their own kinds (a05 hard-codes node[+0x5E] = 7 and pre-seeds the
//     very same defaults this SM's state 0 writes: node[0x80]=64, node[0x82]=128).
//
// WHAT THE PROP DOES (from the sub-behaviours it drives — all Ghidra-decompiled, see the .cpp):
//   * it is a TWO-PIECE prop: state 0 calls FUN_80040410, which allocates 2 child display nodes from
//     the shared pool (needs (s16)*0x800ED098 >= 2, else the prop self-despawns), stores them at
//     node[+0xC0]/[+0xC4], and gives each piece its offset triple from the table at 0x800A3B1C
//     (stride 6) and its model id from the per-variant pair table at 0x800A3B28.
//   * it SITS ON THE TERRAIN: the kind-0/1/6 inits run the grid resolve (FUN_8004766C) plus the
//     slope-angle solve (FUN_80048750) and copy the resulting yaw/pitch (scratchpad 0x1F8001A0/A2)
//     into the node's euler triple node[+0x56]/[+0x58].
//   * it WOBBLES, RELEASES ITS CONTENTS, FALLS AND BURSTS: the state-1 sub-behaviours are, in node[5]
//     order, FUN_8003FD10 (jitter both pieces +-6 on a 16-frame timer), FUN_8003FED8 (same + SFX 0x19),
//     FUN_8003FFCC (SFX 0x1A + spawn a contents object with handler 0x8004AAC4 / 0x8004C238 — the item
//     drop dispatcher), FUN_8004022C (gravity fall: node[0x4A] velocity += node[0x50], clamped 0x3000,
//     grid-resolve each frame, then on landing an 8-particle burst FUN_80027144 + SFX 12 + state := 3),
//     and FUN_80040390 (the same burst, one-shot, gated on the contact byte node[0x29]).
//   That is a destructible placed prop. The finer flavour is per-kind and per-area and is NOT claimed
//   here — the kind byte is data, and naming each kind would be a claim with no source.
//
// THE FOUR JUMP TABLES. The guest does four `jr v0` dispatches through tables in MAIN.EXE's read-only
// data. Dumped straight from scratch/bin/tomba2/MAIN.EXE (load 0x80010000, file offset 0x800):
//   0x800152E0 [8]  state 0 / sub 1, index node[0x5E]:
//       650 660 670 6C0 680 690 6A0 6B0   (kind 3 = "no init")
//   0x80015300 [6]  state 1, index node[5]:      750 760 770 780 790 7A0
//   0x80015318 [8]  state 1, index node[0x5E]:   7E0 7E0 888 7E0 7E0 8C0 7E0 7D8
//   0x80015338 [5]  state 2, index node[5]:      964 904 94C 95C 964
// Per CLAUDE.md ("a table replacing the guest's jump table", game/ui/panel_fill.cpp) the port
// dispatches on the INDEX with named cases instead of re-reading the table and switching on the
// target address. The table contents above are the proof that the case grouping is right.
//
// NODE FIELD NAMES come from docs/engine_re.md's consolidated "ObjectNode" struct (frontier28
// dossier): +0x00 flags(bit1 active), +0x04 SM state, +0x05 SM sub-state, +0x10 partner ptr,
// +0x29 SM gate byte, +0x2B collision-handled, +0x46 flag(bit0), +0x5E dispatcher case selector,
// +0x5F status byte, +0x80 XZ collision radius, +0x84/+0x86 Y-band lo/hi, +0xC0 child-node ptr.
// +0x01 is the visible/submit marker (Cull::enqueueByClass writes it), +0x28 bit 0x80 is the
// "skip the cull, always submit" flag (same gate as game/ai/beh_visibility_gate_dispatch.cpp).
//
// TRAP — v0 IS DEAD AT RETURN. The guest-visible behavior leaves whatever happened to be in v0 there, and the
// value differs per exit path (it is a `void` function; Ghidra types it `void`). The port does NOT
// reproduce those dead delay-slot constants. Proof the caller cannot see them: the only reachable
// caller is the object-list walk FUN_8007A904, which reads `node = c->r[16]` immediately after the
// call and never touches v0 (game/object/object_list.cpp, ObjectList::walkAllFaithful). Everything
// the caller CAN observe — guest memory, sp, and the callee-saved registers r16/ra — is byte-exact.
//
// TRAP — r16 MUST STAY LIVE. The node pointer lives in s0 for the whole body and every callee that
// mirrors its own frame spills its caller's s0. It is held in GuestReg<16>, never in a C++ local.
#pragma once
#include "core.h"
#include <cstdint>

class Game;

// ---------------------------------------------------------------------------------------------
// Typed lens over the guest ObjectNode block this state machine owns. Read/write accessors only —
// no state of its own, so it costs nothing and every guest store in the port reads as a field write.
struct PlacedProp {
  Core *c;
  uint32_t n; // guest address of the node
  PlacedProp(Core *c_, uint32_t n_) : c(c_), n(n_) {}

  static constexpr uint32_t kOffActive = 0x00;      // u8  flags, bit0 = active
  static constexpr uint32_t kOffVisible = 0x01;     // u8  visible / submitted-this-frame marker
  static constexpr uint32_t kOffVariant = 0x03;     // u8  placement-record variant
  static constexpr uint32_t kOffState = 0x04;       // u8  SM state    (0 spawn / 1 active / 2 finish / 3 despawn)
  static constexpr uint32_t kOffSub = 0x05;         // u8  SM sub-state
  static constexpr uint32_t kOffOwner = 0x10;       // u32 partner / owner node ptr
  static constexpr uint32_t kOffFlags28 = 0x28;     // u8  bit 0x80 = skip cull, always submit
  static constexpr uint32_t kOffGate = 0x29;        // u8  per-frame SM gate / contact byte
  static constexpr uint32_t kOffCollHandled = 0x2B; // u8  collision-handled state
  static constexpr uint32_t kOffFlag46 = 0x46;      // u8  flag(bit0)
  static constexpr uint32_t kOffKind = 0x5E;        // u8  dispatcher case selector (the placement "kind")
  static constexpr uint32_t kOffStatus = 0x5F;      // u8  status byte
  static constexpr uint32_t kOffRoomId = 0x6A;      // s16 room / section id, vs global 0x800BF817
  static constexpr uint32_t kOffXzRadius = 0x80;    // u16 XZ collision radius
  static constexpr uint32_t kOffHeight = 0x82;      // u16 paired with the radius (collision height)
  static constexpr uint32_t kOffYBandLo = 0x84;     // u16 Y-band lo  (grid probe uses hi - lo)
  static constexpr uint32_t kOffYBandHi = 0x86;     // u16 Y-band hi

  uint8_t state() const {
    return c->mem_r8(n + kOffState);
  }
  uint8_t sub() const {
    return c->mem_r8(n + kOffSub);
  }
  uint8_t variant() const {
    return c->mem_r8(n + kOffVariant);
  }
  uint8_t kind() const {
    return c->mem_r8(n + kOffKind);
  }
  uint8_t flags28() const {
    return c->mem_r8(n + kOffFlags28);
  }
  uint32_t owner() const {
    return c->mem_r32(n + kOffOwner);
  }
  int32_t roomId() const {
    return c->mem_r16s(n + kOffRoomId);
  } // guest compares the SIGN-EXTENDED s16

  void setPropActive(uint8_t v) {
    c->mem_w8(n + kOffActive, v);
  }
  void setPropVisible(uint8_t v) {
    c->mem_w8(n + kOffVisible, v);
  }
  void setPropState(uint8_t v) {
    c->mem_w8(n + kOffState, v);
  }
  void setPropSub(uint8_t v) {
    c->mem_w8(n + kOffSub, v);
  }
  void setPropGate(uint8_t v) {
    c->mem_w8(n + kOffGate, v);
  }
  void setPropCollDone(uint8_t v) {
    c->mem_w8(n + kOffCollHandled, v);
  }
  void setPropFlag46(uint8_t v) {
    c->mem_w8(n + kOffFlag46, v);
  }
  void setPropKind(uint8_t v) {
    c->mem_w8(n + kOffKind, v);
  }
  void setPropStatus(uint8_t v) {
    c->mem_w8(n + kOffStatus, v);
  }
  void setPropXzRadius(uint16_t v) {
    c->mem_w16(n + kOffXzRadius, v);
  }
  void setPropHeight(uint16_t v) {
    c->mem_w16(n + kOffHeight, v);
  }
  void setPropYBandLo(uint16_t v) {
    c->mem_w16(n + kOffYBandLo, v);
  }
  void setPropYBandHi(uint16_t v) {
    c->mem_w16(n + kOffYBandHi, v);
  }
};

// The per-frame behaviour handler itself. Guest ABI: node in a0 (c->r[4]), no return value.
class PlacedPropSm {
public:
  static void step(Core *c);
  static void registerOverrides(Game *game);
};
