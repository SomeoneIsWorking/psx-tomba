// game/ai/tilt_follower.cpp — TiltFollower::applyHalvedOwnerPartPitch, guest 0x80125FE0 (A00).
//
// WHAT IT DOES, IN GAME TERMS
// An object attached to a larger jointed thing takes its own pitch from one of that thing's moving
// sub-parts — at HALF the angle. So when the part swings through its full arc, the follower leans
// through half of it: it visibly reacts to the motion without matching it, the way something resting
// on or hanging off a moving arm would. Then, if anything is currently in contact with the follower,
// it runs one further sub-behaviour (0x80125C4C); with no contact it does nothing else that frame.
//
// The whole body is four steps:
//   1. read the owner's sub-part SLOT 1 X Euler angle (the part's pitch),
//   2. WRAP it from the unsigned 0..4095 turn into a signed -2048..2047 one,
//   3. HALVE it and store it as this node's own rotX,
//   4. call 0x80125C4C only if the node's contact byte is non-zero.
//
// HOW IT WAS IDENTIFIED (from the caller and from fields other subsystems already named)
//   * ITS CALLER NAMES ITS ROLE. The only non-dispatch-table reference in all of authenticated executable/overlay
//   evidence is
//     overlay guest 0x80125E0C (authenticated executable/overlay evidence), the per-object behaviour handler
//     already owned as game/ai/beh_pure_substate_dispatch.cpp. That file's own banner enumerates the
//     dispatch: "STATE 1 : … then node[5] -> 0/FUN_80125FE0, 1/FUN_801255CC, 2/FUN_80125800". So this
//     is substate 0 of outer state 1 of that behaviour, invoked with a0 = the node.
//   * THE DESTINATION FIELD WAS ALREADY NAMED, INDEPENDENTLY. +0x54 is rotX in game/object/actor.h's
//     Actor lens (`int16_t rotX() const { return (int16_t)c->mem_r16(obj + 0x54); }`), RE'd long
//     before and unrelated to this port. So the store target is the node's X rotation, not a guess.
//   * SO WAS THE SOURCE FIELD. owner+0xC4 is the +0xC0 sub-part pointer table at slot 1 — the same
//     table AssemblyNode::childPtr and NodeXform::propagate walk — and +0x08 of a sub-part record is
//     childEulerX in game/render/node_xform.cpp's CHILD-role lens (line 87), likewise RE'd earlier
//     and for another purpose. Two independently-established names meeting at one function is what
//     makes this reading evidence rather than inference.
//   * WHAT I DID NOT ESTABLISH, and so did not put in the name: WHICH object this is, and whether its
//     owner is specifically a pump assembly. The +0x10 owner / +0x2B contact / +0xC0 sub-part shape
//     is shared by several families here (see game/ai/assembly_rider.h), and no RAM dump was scanned
//     for this node's handler. The name says what the function DOES, which is measured, not what the
//     object IS, which is not.
//
// THE ARITHMETIC, AND WHY IT IS A WRAP
// The guest does `if (!(angle < 2049)) angle |= 0xF000;` and then `(angle << 16) >> 17` arithmetic.
// That reads like bit-twiddling and is actually two ordinary operations:
//   * ORing 0xF000 into a value in [2049, 4095] produces exactly `value - 4096` in two's complement
//     (2049 -> 0xF801 = -2047; 4095 -> 0xFFFF = -1). PSX angles are 4096 units per turn, so this is
//     the standard "past half a turn counts as negative" wrap. Values already negative, and values
//     at or below 2048, take the branch and are left alone.
//   * `<< 16` then arithmetic `>> 17` sign-extends the low 16 bits and shifts right one — i.e. halve
//     the wrapped angle, rounding toward negative infinity as `sra` does.
// Doing the wrap BEFORE the halving is the point: halving an unwrapped 3000 would give 1500 (a
// quarter turn the wrong way) instead of the correct -548.
//
// TRUE EXTENT: [0x80125FE0, 0x80126038). The body's single epilogue is L_80126030 (restore ra from
// sp+16, sp += 24), and the recorded binary evidence's trailing duplicate `return;` after it is the usual
// dead-tail artifact, not a second function — authenticated instruction extents agrees and trims
// nothing here. Deliberately NOT established from "the next gen function in the shard": that test is
// false on this project (the shard split is not address order) and has produced a wrong answer four
// times in this session.
//
// MODULE: defined only by the A00 overlay (`overlay guest 0x80125FE0`, authenticated executable/overlay evidence), so
// it registers with A00 tomba::native::declareOverride. Using the main-module tomba::native::declareOverride would
// leave the direct `overlay guest 0x80125FE0(c)` call site in its caller on the substrate.
//
// GUEST STACK: frame 24, one spill (ra at +16) — mirrored via GuestFrame, per "MIRROR THE GUEST
// STACK". Nothing else is live across the one call, so no GuestReg proxies are needed here.
#include "tilt_follower.h"

#include "core.h"
#include "core/overrides/guest_jal.h"
#include "core/overrides/native_override_catalog.h"
#include "guest_abi.h"

namespace {

// tools/binary ABI evidence 0x80125FE0 --scaffold --guestabi
constexpr GuestFrameSpill kSpills_80125FE0[1] = {
    {31 /*ra*/, 16},
};

// Which of the owner's sub-parts supplies the pitch. Slot 1 = owner+0xC0 + 1*4 = owner+0xC4.
constexpr int kTiltSourceSlot = 1;

// PSX angles are 4096 units per full turn, so anything strictly above half a turn is represented as
// negative. 2049 is the guest's own threshold (`slti 2049`, i.e. "is it at most 2048").
constexpr int32_t kWrapThreshold = 2049;
constexpr uint32_t kWrapToNegative = 0xF000u; // ORs a 12-bit angle down by exactly 4096

// The sub-behaviour that runs only while something is touching this node.
constexpr uint32_t kRaAfterContactBehaviour = 0x80126030u;

} // namespace

// ORACLE: overlay guest 0x80125FE0
void TiltFollower::applyHalvedOwnerPartPitch(Core *c) {
  GuestFrame<24, 1> frame(c, kSpills_80125FE0);

  const TiltFollowerNode self{c, c->r[4]};
  const TiltSourcePart source{c, c->mem_r32(self.owner() + tiltpart::kChildTable + (uint32_t)kTiltSourceSlot * 4u)};

  // Wrap the part's pitch out of the unsigned turn and into a signed one, then take half of it.
  uint32_t angle = (uint32_t)source.childEulerX();
  if (!((int32_t)angle < kWrapThreshold)) {
    angle |= kWrapToNegative;
  }
  const int32_t halfPitch = (int32_t)(angle << 16) >> 17;
  self.setPitch((uint16_t)halfPitch);

  // Only while something is in contact — otherwise the follower just leans and is done.
  if (self.contactState() == 0) {
    return;
  }
  c->r[4] = self.mAt;
  tomba::guest::dispatchJalToReturn(*c, 0x80125C4Cu, kRaAfterContactBehaviour);
}

void TiltFollower::registerOverrides() {
  // A00, from the image: of 23 authenticated MODE images only A00 holds a function at 0x80125FE0
  // (24 words), and the address is inside A00's loaded text range. This is the MODULE note above,
  // applied.
  tomba::native::declareOverlayOverride(
      "A00", 0x80125FE0u, "TiltFollower::applyHalvedOwnerPartPitch", &TiltFollower::applyHalvedOwnerPartPitch);
}
