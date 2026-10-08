// game/ai/assembly_rider.cpp — AssemblyRider::rideSlotAndReactToStroke, guest 0x80118B10 (A00).
//
// WHAT IT IS, IN GAME TERMS
// The seaside water pumps (the long diagonal beams with an arm and two identical hanging end-parts,
// guest behaviour FUN_8012EB54 — game/ai/assembly_node.h) each carry a small PLACED object perched on
// one of their two arm-ends. This function is that object's ENTIRE per-frame tick, and it is a
// four-state life:
//
//   INIT (0)    — decide which arm-end this rider belongs to. The placement tag at +0x62 is one of
//                 two values and it selects both the sub-part slot the rider will ride (3 or 2) and
//                 a scene tag (10 or 1). Then it raises the node's alive byte and advances to RIDE.
//   RIDE (1)    — sit on the arm-end. Every frame it COPIES THE PUMP'S OWN VISIBILITY BYTE into its
//                 own, snaps its world position to the arm-end's position raised 140 units, and
//                 pushes itself onto cull queue C. It never runs its own visibility test in this
//                 state: it is drawn exactly when the pump is, and dormant when the pump is culled.
//                 While riding it watches the pump's mode byte and reacts to the stroke:
//                   mode 1 (the pump is starting to move)   -> HOP
//                   mode 2 or 3 AND the pump names THIS arm -> FLING
//   HOP (2)     — a two-bounce hop in place, integrated on the rider's own 16.16 Y. Both bounces
//                 launch at the same upward speed (-2048, i.e. 8 units/frame) but the second falls
//                 back twice as hard (gravity 384 then 768), so it is a big hop followed by a
//                 visibly smaller one. Each bounce ends when the rider falls back to its rest height
//                 above the arm-end; after the second it returns to RIDE and re-attaches.
//   FLING (3)   — the rider is thrown off. The pump's mode byte is copied to +0x47 at the moment the
//                 fling starts, and the fling body (guest 0x801189B8) reads ONLY that to choose which
//                 way it spirals: mode 2 ramps its horizontal scalar +256/+4 per frame, anything else
//                 -256/-4, while velY starts at +4096 with gravity 512 — a fast falling spiral out
//                 to one side. So which way the pump strokes is which way the rider is thrown.
//   DORMANT (4) — nothing but the visibility test. The tail forces this state whenever the node's
//                 contact byte +0x2B reads 2 — the "player is bearing on this object" value the
//                 contact producer FUN_80111304 stamps and that the pump's own
//                 SubstateEdgeLeaves::contactWeightApply turns into the beam's weight. Lean on a
//                 rider and it stops riding, hopping and flying.
//
// HOW IT WAS IDENTIFIED — FROM THE DEPENDENT, THE OWNER LINK, AND LIVE RAM; NOT FROM THE ADDRESS
//   * THE DEPENDENT NAMES THE ROLE. Its one and only caller is the already-owned resident dispatcher
//     beh_visibility_gate_dispatch (FUN_8004C238, game/ai/beh_visibility_gate_dispatch.cpp:163),
//     which reaches this address from exactly one place: node sub-type +0x5E == 6, area byte
//     0x800BF870 == 0. So this is "the area-0 body of placed-object class 6" — and what the caller
//     expects back is nothing: it neither passes a second argument nor reads v0; it falls straight
//     into its shared tail, which clears +0x29 and +0x2B. Every effect of this leaf therefore has to
//     be a guest-memory write on the node itself, which is exactly what it is.
//   * THE OWNER LINK IS MEASURED. Scanning the four committed area-0 RAM dumps for +0x1C ==
//     0x8004C238 finds EXACTLY TWO sub-type-6 nodes, byte-identical across all four dumps, and both
//     of their +0x10 owners have 0x8012EB54 in their own +0x1C — the pump assembly. Their arm-end
//     sub-parts resolve the ride exactly (X and Z equal, Y ~140 apart) and are the only two slots of
//     the assembly sharing one model. The full table is in assembly_rider.h; the state-0 init's
//     tag -> (slot, sceneTag) mapping reproduces the live bytes exactly, which is what proves the
//     init means what it reads like rather than merely being consistent with it.
//   * THE SUBMIT CORROBORATES THE CLASS. The nodes' class byte +0x0C is 5; Cull::enqueueByClass
//     routes class 5 to queue C; the only submit this leaf makes is queue C (0x80077EFC). Two
//     independent facts that had to agree and do.
//   * WHAT THE PUMP'S +0x5E AND +0x6C ARE was established by the assembly's OWN leaves, not by this
//     reader — see AssemblyNode::modeByte / angleSelector (game/ai/assembly_node.h), where
//     angleSelector is already documented as "the part being commanded". This leaf is the second
//     consumer of that publication and reads consistently with the first.
//
// TRUE EXTENT: [0x80118B10, 0x80118DB0), 0x2A0 bytes / 168 instructions. Established three ways, and
// deliberately NOT by "the next gen function in the shard" — that test is false here: the shard split
// is not address order, and the function immediately PRECEDING this one by address (0x801189B8, the
// fling body) is emitted in a different file, authenticated executable/overlay evidence, while this one sits in
// ov_a00_shard_1.c. So:
//   1. authenticated instruction extents puts the body at authenticated executable/overlay evidence.
//   2. Every label the body branches to lies in [0x80118B10, 0x80118D98], the last being the
//      epilogue, and the jump table at kRideStateJumpTable holds five entries all inside that range.
//   3. Disassembly of the overlay image confirms it instruction-for-instruction
//      (A00.BIN, base 0x80108F9C, range 0x80118B10..0x80118DC0):
//      `jr ra` at 0x80118DA8 with `addiu sp, sp, 0x20` in its delay slot at 0x80118DAC, and
//      0x80118DB0 opening a fresh prologue (`addiu sp, sp, -0x18` / `sw s0, 0x10(sp)`) — which is
//      also the sub-type-7 leaf the same dispatcher calls, and its own entry in the overlay dispatch
//      table (authenticated executable/overlay evidence).
//
// MODULE: the body is defined ONLY by the A00 overlay — `overlay guest 0x80118B10`, one definition across
// all of authenticated executable/overlay evidence (ov_a00_shard_1.c), one dispatch entry (ov_a00_disp.c index 153).
// Overlay address ranges do overlap in this game, but no other overlay emits code at this address, so the setter is
// unambiguously A00 tomba::native::declareOverride and NOT tomba::native::declareOverride — using the main-module
// setter would leave the direct `overlay guest 0x80118B10(c)` callers on the substrate.
//
// GUEST STACK: frame 32, four spills (r16 +16, ra +28, r18 +24, r17 +20, in program order). The three
// callee-saved registers are NOT C++ locals — r16/r17/r18 hold the node, the pump and the arm-end
// anchor and are live across every nested call (0x801189B8 spills r16 into its own frame, so a stale
// r16 would be written to guest RAM). They are assigned before any call and mirrored through
// GuestFrame, per CLAUDE.md "MIRROR THE GUEST STACK".
#include "assembly_rider.h"
#include "guest_call.h"

#include "assembly_node.h"
#include "core.h"
#include "core/overrides/guest_jal.h"
#include "core/overrides/native_override_catalog.h"
#include "guest_abi.h"

namespace {

// ---- guest stack (tools/binary ABI evidence 0x80118B10 --scaffold --guestabi) ----------------------
constexpr GuestFrameSpill kSpills_80118B10[4] = {
    {16, 16},
    {31 /*ra*/, 28},
    {18, 24},
    {17, 20},
};

// ---- the rider's own state machine -------------------------------------------------------------
// The guest dispatches it through a five-entry jump table; the addresses below ARE that table's
// contents, so the switch reproduces the `jr v0` exactly rather than re-deriving the mapping.
constexpr uint32_t kRideStateJumpTable = 0x8010970Cu; // lui 0x8011 / addiu -0x68F4
constexpr uint32_t kRideStateCount = 5;
constexpr uint8_t kRideStateRide = 1;
constexpr uint8_t kRideStateHop = 2;
constexpr uint8_t kRideStateFling = 3;
constexpr uint8_t kRideStateDormant = 4;
constexpr uint32_t kEntryInit = 0x80118B7Cu;
constexpr uint32_t kEntryRide = 0x80118BC8u;
constexpr uint32_t kEntryHop = 0x80118C6Cu;
constexpr uint32_t kEntryFling = 0x80118D5Cu;
constexpr uint32_t kEntryDormant = 0x80118D7Cu;

// ---- the two rider variants (see the measured table in assembly_rider.h) -----------------------
constexpr int32_t kVariantTagSlot3 = 200; // -> rides the assembly's sub-part slot 3 (owner+0xCC)
constexpr int32_t kVariantTagSlot2 = 204; // -> rides the assembly's sub-part slot 2 (owner+0xC8)
constexpr int32_t kArmEndSlotForTag200 = 3;
constexpr int32_t kArmEndSlotForTag204 = 2;
constexpr uint8_t kSceneTagSlot3 = 10;
constexpr uint8_t kSceneTagSlot2 = 1;

// ---- riding, hopping, and being thrown ---------------------------------------------------------
// How far above the arm-end the rider sits. The RIDE state writes it into the position and the HOP
// state uses the same height as the ground it lands back on, which is why one constant serves both.
constexpr int32_t kRiderHeightAboveArmEnd = 140;
// Upward launch speed of BOTH bounces, in 1/256 world units per frame (negative Y is up).
constexpr uint16_t kHopLaunchVelY = (uint16_t)-2048;
// Gravity for bounce 1 and bounce 2. Doubling it halves the height and the airtime — the little
// second hop.
constexpr uint16_t kHopGravityFirstBounce = 384;
constexpr uint16_t kHopGravitySecondBounce = 768;
constexpr uint8_t kHopPhaseLaunch = 0;
constexpr uint8_t kHopPhaseFirstBounce = 1;
constexpr uint8_t kHopPhaseSecondBounce = 2;

// The pump's mode byte value that means "the stroke is starting"; modes 2 and 3 are the two
// commanded-arm modes, which is why the fling test is a 2-wide window rather than two comparisons.
constexpr uint32_t kModeStrokeStarting = 1;
constexpr uint32_t kModeCommandedFirst = 2;
constexpr uint32_t kModeCommandedWindow = 2;
// The contact byte value that parks the rider (the same "player bearing on it" value the pump's own
// contactWeightApply consumes).
constexpr uint32_t kContactPlayerBearing = 2;

// ---- call sites (jal-site return addresses; abi_extract 0x80118B10 --contract) ------------------
constexpr uint32_t kCullEnqueueQueueC = 0x80077EFCu; // Cull::enqueueQueueC — the class-5 submit
constexpr uint32_t kActorBoundsCull = 0x8007778Cu;   // Actor::boundsCull
constexpr uint32_t kRaAfterSubmit = 0x80118C64u;
constexpr uint32_t kRaAfterHopCull = 0x80118C74u;
constexpr uint32_t kRaAfterFlingCull = 0x80118D64u;
constexpr uint32_t kRaAfterFling = 0x80118D74u;
constexpr uint32_t kRaAfterDormantCull = 0x80118D84u;

// v0 on every reachable exit: the guest loads 4 into it in the delay slot of the tail's branch, so
// it leaves the same value whether or not the contact test fires. Mirrored so a caller that spills
// v0 spills the same byte.
constexpr uint32_t kExitV0 = 4;

} // namespace

// ORACLE: overlay guest 0x80118B10
// GUEST_ADDRESS: 80118B10 authenticated executable/overlay evidence
void AssemblyRider::rideSlotAndReactToStroke(Core *c) {
  GuestFrame<32, 4> frame(c, kSpills_80118B10);

  c->r[16] = c->r[4]; // s0 — the rider node
  const AssemblyRider rider(c, c->r[16]);
  c->r[17] = rider.owner(); // s1 — the pump assembly it is perched on
  const RiddenAssembly assembly(c, c->r[17]);
  // s2 — the arm-end sub-part it rides. Which one is fixed at placement time by the variant tag.
  c->r[18] = assembly.childPtr(rider.variantTag() == kVariantTagSlot3 ? kArmEndSlotForTag200 : kArmEndSlotForTag204);
  const AssemblyPartAnchor anchor(c, c->r[18]);

  const uint32_t state = rider.rideState();
  if (state >= kRideStateCount) {
    goto tail;
  }
  {
    const uint32_t entry = c->mem_r32(kRideStateJumpTable + state * 4u);
    switch (entry) {
    case kEntryInit:
      goto state_init;
    case kEntryRide:
      goto state_ride;
    case kEntryHop:
      goto state_hop;
    case kEntryFling:
      goto state_fling;
    case kEntryDormant:
      goto state_dormant;
    // The guest's `jr v0` into an entry outside its own five-word table — unreachable unless that
    // table is corrupt, and modelled by the recorded binary evidence as a tail dispatch with no epilogue.
    default:
      psx::cpu::dispatchGuestToReturn0(*c, entry, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
      return;
    }
  }

  // ── INIT: bind this rider to one of the pump's two arm-ends ────────────────────────────────────
state_init: {
  rider.setAlive(1);
  rider.setRideState((uint8_t)(rider.rideState() + 1u)); // -> RIDE
  const int32_t tag = rider.variantTag();
  if (tag == kVariantTagSlot3) {
    rider.setSceneTag(kSceneTagSlot3);
    rider.setSlot(kArmEndSlotForTag200);
    goto tail;
  }
  if (tag != kVariantTagSlot2) {
    goto tail; // an unknown tag binds to nothing
  }
  rider.setSceneTag(kSceneTagSlot2);
  rider.setSlot(kArmEndSlotForTag204);
  goto tail;
}

  // ── RIDE: inherit the pump's visibility, watch its stroke, then re-seat and submit ─────────────
state_ride: {
  const uint32_t pumpVisible = assembly.visible();
  rider.setVisible((uint8_t)pumpVisible);
  if (pumpVisible == 0) {
    goto tail; // pump culled -> rider does nothing
  }
  const uint32_t mode = assembly.modeByte();
  if (mode == kModeStrokeStarting) {
    rider.setRideState(kRideStateHop);
    rider.setMotionPhase(kHopPhaseLaunch);
    goto submit; // still re-seated + submitted today
  }
  if (((mode - kModeCommandedFirst) & 0xffu) < kModeCommandedWindow &&
      assembly.angleSelector() == (int32_t)rider.slot()) { // the pump names THIS arm
    rider.setRideState(kRideStateFling);
    rider.setMotionPhase(kHopPhaseLaunch);
    rider.setFlingSide((uint8_t)assembly.modeByte()); // which way it gets thrown
  }
}
  // Re-seat on the arm-end and push onto cull queue C. Reached from the ride tick and from the frame
  // the hop is armed, so the rider is drawn at its seat on that frame either way.
submit: {
  rider.setPosX((uint16_t)anchor.x16());
  rider.setPosY((uint16_t)(anchor.y16() - (uint32_t)kRiderHeightAboveArmEnd));
  rider.setPosZ((uint16_t)anchor.z16());
  c->r[4] = c->r[16];
  tomba::guest::dispatchJalToReturn(*c, kCullEnqueueQueueC, kRaAfterSubmit);
  goto tail;
}

  // ── HOP: two bounces in place above the arm-end ────────────────────────────────────────────────
state_hop: {
  c->r[4] = c->r[16];
  tomba::guest::dispatchJalToReturn(*c, kActorBoundsCull, kRaAfterHopCull);
  const uint32_t phase = rider.motionPhase();
  if (phase == kHopPhaseFirstBounce) {
    goto hop_bounce_one;
  }
  if ((int32_t)phase >= 2) {
    if (phase == kHopPhaseSecondBounce) {
      goto hop_bounce_two;
    }
    goto tail;
  }
  if (phase != kHopPhaseLaunch) {
    goto tail;
  }
  rider.setVelY(kHopLaunchVelY); // launch, then fall through and move
  rider.setMotionPhase(kHopPhaseFirstBounce);
  rider.setAccelY(kHopGravityFirstBounce);
}
hop_bounce_one: {
  rider.setPosYFixed(rider.posYFixed() + (((uint32_t)rider.velY()) << 8));
  rider.setVelY((uint16_t)(rider.velY_u() + rider.accelY_u()));
  // Landed once the rider has fallen back BELOW its rest height above the arm-end (Y grows
  // downward), which is exactly the guest's `slt (anchorY - 140), posY`.
  if ((anchor.y32() - kRiderHeightAboveArmEnd) >= rider.posY()) {
    goto tail; // still airborne
  }
  rider.setVelY(kHopLaunchVelY); // landed -> the smaller second hop
  rider.setAccelY(kHopGravitySecondBounce);
  rider.setMotionPhase((uint8_t)(rider.motionPhase() + 1u));
  goto tail;
}
hop_bounce_two: {
  rider.setPosYFixed(rider.posYFixed() + (((uint32_t)rider.velY()) << 8));
  rider.setVelY((uint16_t)(rider.velY_u() + rider.accelY_u()));
  if ((anchor.y32() - kRiderHeightAboveArmEnd) >= rider.posY()) {
    goto tail; // still airborne
  }
  rider.setRideState(kRideStateRide); // landed -> re-attach to the arm-end
  rider.setMotionPhase(kHopPhaseLaunch);
  goto tail;
}

  // ── FLING: thrown off, spiralling to the side the pump stroked ─────────────────────────────────
state_fling: {
  c->r[4] = c->r[16];
  tomba::guest::dispatchJalToReturn(*c, kActorBoundsCull, kRaAfterFlingCull);
  c->r[4] = c->r[16];
  c->r[5] = c->r[17];
  c->r[6] = c->r[18];
  tomba::guest::dispatchJalToReturn(*c, 0x801189B8u, kRaAfterFling);
  goto tail;
}

  // ── DORMANT: visibility only ───────────────────────────────────────────────────────────────────
state_dormant: {
  c->r[4] = c->r[16];
  tomba::guest::dispatchJalToReturn(*c, kActorBoundsCull, kRaAfterDormantCull);
}

tail:
  c->r[2] = kExitV0;
  if (rider.contactState() == kContactPlayerBearing) {
    rider.setRideState(kRideStateDormant);
  }
}

void AssemblyRider::registerOverrides() {
  // A00, from the image: of 23 authenticated MODE images only A00 holds a function at 0x80118B10
  // (168 words), and the address is inside A00's loaded text range. This is the MODULE note above,
  // applied: the body is A00-only, so the declaration carries the A00 identity.
  tomba::native::declareOverlayOverride(
      "A00", 0x80118B10u, "AssemblyRider::rideSlotAndReactToStroke", &AssemblyRider::rideSlotAndReactToStroke);
}
