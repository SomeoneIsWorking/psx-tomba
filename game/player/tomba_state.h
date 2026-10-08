// game/player/tomba_state.h — the guest addresses and the typed lens over Tomba's G block.
//
// G IS TOMBA'S NODE: the master block at 0x800E7E80 that pool_init's sub-init FUN_8007A810 zeroes
// (0x184 bytes). Every Tomba owner — the per-frame driver, the growth and motion steps, the
// interaction pass — reads and writes it, so the lens and the address vocabulary live here once.
// Every accessor is still a direct guest-RAM access at the guest's own address; this is readability,
// not a state migration. The per-field RE is in docs/engine_re.md's "ActorTomba G-block" section.
#pragma once

#include "core.h"

#include <cstdint>

namespace tomba::player {

// -- Shared constants (guest addresses) -------------------------------------------------------
inline constexpr uint32_t kAuxListHead = 0x1F800154u;
inline constexpr uint32_t kAuxListCount = 0x1F80015Cu;
inline constexpr uint32_t kAuxWalkCounter = 0x1F800182u;
inline constexpr uint32_t kGateBF80C = 0x800BF80Du;

inline constexpr uint32_t kLeafIsqrt = 0x80084080u;
inline constexpr uint32_t kLeafAtan2 = 0x80085690u;
inline constexpr uint32_t kLeafCollisionCallback = 0x8004D19Cu;
inline constexpr uint32_t kLeafProximityF04 = 0x80022F04u;

// Sub-hitbox parameter table (u8[16], 2 bytes per hitbox: (size_xz, size_y)) — MAIN.EXE .rodata.
inline constexpr uint32_t kSubHitboxParams = 0x800A29D0u;

// Shared scratchpad outputs.
inline constexpr uint32_t kOutDistanceSlot = 0x1F80008Cu;
inline constexpr uint32_t kOutHeadingSlot = 0x1F80009Cu;

// Growth-mode rescale target (a shared BSS halfword read by scenery/props).
inline constexpr uint32_t kGrowthMirrorHalfword = 0x800E802Au;

inline void markItemConsumed(Core *c, uint32_t item) {
  c->mem_w8(item + 0, 2);
  c->mem_w8(item + 4, 2);
  c->mem_w8(item + 5, 0);
  c->mem_w8(item + 6, 0);
}

// TombaState — named-field lens over Tomba's G block (ActorTomba::G_ADDR / a `G` local passed in
// by the per-frame driver). Guest addresses are UNCHANGED from the raw c->mem_r/w8/16 pokes this
// replaces (see actor_tomba.h's per-function doc comments for the RE of each field); this is a
// readability lens, not a state migration — every accessor is still a direct guest-RAM access.
// Scoped to the fields the frameTick / outer-transition-gate / outer-transition-commit cluster
// touches; a wider pass can grow this lens as more of the file gets ported.
// The typed lens over G. Scoped to the fields the driver, the interaction pass and the
// outer-transition cluster touch; a wider port can grow it accessor by accessor.
struct TombaState {
  Core *c;
  uint32_t base;

  // outerState (+0x4): frameTick's own top-level FSM selector (0=INIT..7=LOAD-WAIT). Also used,
  // within LOAD-WAIT, as the settle target frameTick's case-7 sub-machine writes back to (=1).
  uint8_t outerState() const {
    return c->mem_r8(base + 0x4u);
  }
  void setOuterState(uint8_t v) {
    c->mem_w8(base + 0x4u, v);
  }

  // loadStep/loadSub/loadSub2 (+0x5/+0x6/+0x7): the 3-state LOAD-WAIT sub-machine counter
  // (frameTick case 7) — also the "G+5=1,G+6=0" pair outerTransitionGate/Commit write when they
  // commit a fresh walk-state.
  uint8_t loadStep() const {
    return c->mem_r8(base + 0x5u);
  }
  void setLoadStep(uint8_t v) {
    c->mem_w8(base + 0x5u, v);
  }
  void setLoadSub(uint8_t v) {
    c->mem_w8(base + 0x6u, v);
  }
  void setLoadSub2(uint8_t v) {
    c->mem_w8(base + 0x7u, v);
  }

  // statusFlags (+0x0): walk-state / cutscene-lock byte. Bit 4 and the 0xC mask gate the
  // outer-transition commit path; literal 3 is the "reset to walk" stamp.
  uint8_t statusFlags() const {
    return c->mem_r8(base + 0x0u);
  }
  void setStatusFlags(uint8_t v) {
    c->mem_w8(base + 0x0u, v);
  }

  // latchFlags (+0xD): stop-motion / facing-lock bitfield (0x80 busy, 0x50 lock mask, 0x82 armed).
  uint8_t latchFlags() const {
    return c->mem_r8(base + 0xDu);
  }
  void setLatchFlags(uint8_t v) {
    c->mem_w8(base + 0xDu, v);
  }

  // stopMotionAux (+0x61): companion byte cleared alongside latchFlags on a walk-state reset.
  void setStopMotionAux(uint8_t v) {
    c->mem_w8(base + 0x61u, v);
  }

  // facing (+0x140, s16): Tomba's current heading — turnBiasCompute's `facing` arg source.
  int16_t facing() const {
    return (int16_t)c->mem_r16(base + 0x140u);
  }

  // turnSuppressGate (+0x146): "already turn-suppressed" flag frameTick checks post-turnBias.
  uint8_t turnSuppressGate() const {
    return c->mem_r8(base + 0x146u);
  }
  void setTurnSuppressGate(uint8_t v) {
    c->mem_w8(base + 0x146u, v);
  }

  // transitionSlot (+0x164): the interaction-slot state outerTransitionGate branches on (1 = a
  // specific slot -> a stop-motion spawn without the busy-latch check; else gated by 0x800BF80D).
  uint8_t transitionSlot() const {
    return c->mem_r8(base + 0x164u);
  }

  void setExtraClear(uint8_t v) {
    c->mem_w8(base + 0x16Au, v);
  }

  // turnCurrent/turnTarget (+0x16E/+0x170, s16): the pending-frame turn counter and its commit
  // target. outerTransitionGate bails while turnCurrent is still positive; outerTransitionCommit
  // arms turnTarget = turnCurrent when they differ.
  int16_t turnCurrent() const {
    return (int16_t)c->mem_r16(base + 0x16Eu);
  }
  void setTurnCurrent(uint16_t v) {
    c->mem_w16(base + 0x16Eu, v);
  }
  int16_t turnTarget() const {
    return (int16_t)c->mem_r16(base + 0x170u);
  }
  void setTurnTarget(uint16_t v) {
    c->mem_w16(base + 0x170u, v);
  }

  // settleCounter (+0x172, s16): outerTransitionCommit's decrement-and-settle counter; reaching 0
  // either commits walk-state 1 or re-arms to 1 depending on statusFlags.
  int16_t settleCounter() const {
    return (int16_t)c->mem_r16(base + 0x172u);
  }
  void setSettleCounter(uint16_t v) {
    c->mem_w16(base + 0x172u, v);
  }

  // committing (+0x17B): frameTick case-2 (COMMITTING) latch, set on entry to that state.
  void setCommitting(uint8_t v) {
    c->mem_w8(base + 0x17Bu, v);
  }

  // posAddr(): +0x2C — Tomba's position triple, passed BY ADDRESS to the stop-motion spawn call
  // (guest FUN_800312D4 takes a dest pointer, not a value).
  uint32_t posAddr() const {
    return base + 0x2Cu;
  }

  // -- Extended 2026-07-15 (2nd code-quality pass): interactWalk/proximityCheck/subHitboxCheck/
  // postInteractWalk/postFrameWaterCheck/type8Interact/type7Interact cluster. TombaState is
  // constructed over G_ADDR for Tomba himself, but proximityCheck/subHitboxCheck/stepModeInteract/
  // type8Interact all read an ITEM node at the SAME field offsets (0x2E/0x32/0x36/0x80/0x84/0x86)
  // — items share Tomba's node layout, so this lens doubles as a generic actor-node view: wrap it
  // over `item` too (`TombaState other{c, item}`) rather than reading item+0xNN raw.

  // posX/posY/posZ (+0x2E/+0x32/+0x36, s16): world position triple interactWalk's proximity math
  // and postFrameWaterCheck's off-map check read/write. NOT the same triple as posAddr() (+0x2C) —
  // RE unresolved why the two don't coincide; posAddr() is only ever used as a spawn dest pointer.
  int16_t posX() const {
    return (int16_t)c->mem_r16(base + 0x2Eu);
  }
  void setPosX(int16_t v) {
    c->mem_w16(base + 0x2Eu, (uint16_t)v);
  }
  int16_t posY() const {
    return (int16_t)c->mem_r16(base + 0x32u);
  }
  void setPosY(int16_t v) {
    c->mem_w16(base + 0x32u, (uint16_t)v);
  }
  int16_t posZ() const {
    return (int16_t)c->mem_r16(base + 0x36u);
  }
  void setPosZ(int16_t v) {
    c->mem_w16(base + 0x36u, (uint16_t)v);
  }

  // boundXZ (+0x80, s16): horizontal (X/Z) cylinder-proximity radius. boundYUp (+0x84, READ
  // UNSIGNED — ground truth never sign-extends this field) / boundYDown (+0x86, s16): the two
  // halves of the vertical proximity band, summed between two nodes in proximityCheck/
  // subHitboxCheck's Y-band test.
  int16_t boundXZ() const {
    return (int16_t)c->mem_r16s(base + 0x80u);
  }
  uint16_t boundYUp() const {
    return c->mem_r16(base + 0x84u);
  }
  int16_t boundYDown() const {
    return (int16_t)c->mem_r16s(base + 0x86u);
  }

  // growthFlags (+0x17E, u16): bit 0x200 = "paused/frozen" (interactWalk/stepModeInteract/
  // type8Interact all early-out or branch on it), bit 0x8000 = "grown" (growthStep toggles it;
  // stepModeInteract/type8Interact branch on it to route to the grown-state delegate leaves).
  uint16_t growthFlags() const {
    return c->mem_r16(base + 0x17Eu);
  }

  // justTransitioned (+0x144, u8): "just entered this interaction state" latch — stepModeInteract/
  // type8Interact both special-case `==1 && v0<2` as the just-triggered transition frame.
  uint8_t justTransitioned() const {
    return c->mem_r8(base + 0x144u);
  }

  // groundedGate (+0x145, u8) bit 0: postFrameWaterCheck/type8Interact/growthYSnap check bit 0
  // clear before snapping the position to a water/growth-offset target.
  uint8_t groundedGate() const {
    return c->mem_r8(base + 0x145u);
  }
  void setGroundedGate(uint8_t v) {
    c->mem_w8(base + 0x145u, v);
  }

  // frozenFlag (+0x78, u8): "not frozen" gate type8Interact/growthYSnap check before a niladic
  // cue / a growth-offset re-snap.
  uint8_t frozenFlag() const {
    return c->mem_r8(base + 0x78u);
  }

  // flag95 (+0x5F, u8) / groundContactFlag (+0x60, u8): the header's own names (see actor_tomba.h
  // "settleStep"/"type8Interact" doc comments) for the ground-probe response pair type8Interact's
  // proximity-hit branch stamps.
  void setFlag95(uint8_t v) {
    c->mem_w8(base + 0x5Fu, v);
  }
  void setGroundContactFlag(uint8_t v) {
    c->mem_w8(base + 0x60u, v);
  }

  // physOffsetY (+0x62, s16): the "physics constant" growthStep rescales (header: "G+0x62/64/66/
  // 68 (physics constants)") — postFrameWaterCheck reuses it as the water-surface-to-feet offset.
  int16_t physOffsetY() const {
    return (int16_t)c->mem_r16s(base + 0x62u);
  }

  // committing (+0x17B, u8): frameTick case-2 (COMMITTING) latch — see setCommitting() above;
  // postFrameWaterCheck's off-map trigger gates on it being clear.
  uint8_t committing() const {
    return c->mem_r8(base + 0x17Bu);
  }
};

} // namespace tomba::player
