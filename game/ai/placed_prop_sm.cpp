// game/ai/placed_prop_sm.cpp — see placed_prop_sm.h for WHAT this is and how it was identified.
//
// This is a REBUILD of guest 0x80040558, not a transcription: the four `jr v0` jump tables are named
// switches over the index (their MAIN.EXE contents are quoted in the header), every literal guest
// address is a named constant, every node byte goes through the PlacedProp lens, and each `jal`
// return-address constant is named for the call it follows. The guest MECHANICS are untouched —
// the 24-byte frame with s0@+16 / ra@+20 is mirrored, the node pointer lives in s0 (never a C++
// local) across every call, and the guest-visible store and call sequences are unchanged.
//
// WHY THE STATE-1 AND STATE-2 RENDER TAILS ARE DUPLICATED BELOW: they are duplicated in the guest.
// 0x800407E0..0x80040880 and 0x8004099C..0x80040A38 are two compiled copies of the same source tail
// with different `jal` return addresses, and state 1 additionally clears node[0x29]/[0x5F] on the way
// out while state 2 just returns. Folding them into one helper here would collapse the two copies
// into one and stop matching the guest's operation sequence, so they stay side by side.
#include "placed_prop_sm.h"
#include "core.h"
#include "core/overrides/guest_jal.h" // GuestFrame / GuestReg / guest_call / guest_dispatch
#include "core/overrides/native_override_catalog.h"
#include "game.h"
#include "guest_abi.h"

// Resident MAIN.EXE callees. Declared here (not inline in the body — CLAUDE.md) so guest_call can
// take their address; every one of them routes through its own override thunk.

// ---------------------------------------------------------------------------------------------
// Guest globals this state machine reads.
constexpr uint32_t kGblAreaId = 0x800BF870u;     // u8  current area index
constexpr uint32_t kGblAreaSub = 0x800BF871u;    // u8  area sub-state
constexpr uint32_t kGblAreaGate = 0x800BFA59u;   // u8  per-area "props may tick" flag (0x800BF870+489)
constexpr uint32_t kGblRoomValid = 0x800BF816u;  // u8  the active-room id below is meaningful
constexpr uint32_t kGblRoomId = 0x800BF817u;     // u8  active room / section id
constexpr uint32_t kGblFinishGate = 0x800BFAD1u; // u8  state-2 gate on arming the scene event

// Area indices are FACTS (docs/areas.md); no area NAME is claimed for them here.
constexpr uint8_t kAreaPropsGated = 18; // ticks only while kGblAreaGate != 0
constexpr uint8_t kAreaSubFrozen = 6;   // frozen while its sub-state == kAreaSubFrozenValue
constexpr uint8_t kAreaSubFrozenVal = 19;
constexpr uint8_t kAreaOverlayCull = 8; // uses the overlay cull instead of Actor::boundsCull

// Overlay callees, reached by address (their image may not even be resident).
constexpr uint32_t kFnKind2Init = 0x801286F4u;      // a01 — kind 2 (owner-linked piece) init
constexpr uint32_t kFnKind5Init = 0x80120188u;      // a04 — kind 5 init
constexpr uint32_t kFnKind7Init = 0x801146E8u;      // a05 — kind 7 init (retrying, like kind 6)
constexpr uint32_t kFnSub5Tick = 0x80114934u;       // a05 — state-1 sub 5 tick
constexpr uint32_t kFnKind7PreDraw = 0x8012B118u;   // kind 7's extra pass before the shared gate
constexpr uint32_t kFnOwnedPieceSync = 0x8012866Cu; // kind 2 — copy the owner's pose onto this piece
constexpr uint32_t kFnKind5Draw = 0x801201E0u;      // a04 — kind 5's own render tail
constexpr uint32_t kFnOverlayCull = 0x8012E168u;    // area-8 replacement for Actor::boundsCull

// `jal` return-address constants — one per call site, named for the call it follows.
constexpr uint32_t kRaBuildPieces = 0x800405D8u;
constexpr uint32_t kRaKind0Init = 0x80040658u;
constexpr uint32_t kRaKind1Init = 0x80040668u;
constexpr uint32_t kRaKind2Init = 0x80040678u;
constexpr uint32_t kRaKind4Init = 0x80040688u;
constexpr uint32_t kRaKind5Init = 0x80040698u;
constexpr uint32_t kRaKind6Init = 0x800406A8u;
constexpr uint32_t kRaKind7Init = 0x800406B8u;
constexpr uint32_t kRaSub0Tick = 0x80040758u;
constexpr uint32_t kRaSub1Tick = 0x80040768u;
constexpr uint32_t kRaSub2Tick = 0x80040778u;
constexpr uint32_t kRaSub3Tick = 0x80040788u;
constexpr uint32_t kRaSub4Tick = 0x80040798u;
constexpr uint32_t kRaSub5Tick = 0x800407A8u;
constexpr uint32_t kRaKind7PreDraw = 0x800407E0u;
constexpr uint32_t kRaActiveEnqueue = 0x8004082Cu;
constexpr uint32_t kRaActiveOverlayCull = 0x80040860u;
constexpr uint32_t kRaActiveBoundsCull = 0x80040870u;
constexpr uint32_t kRaActiveDraw = 0x80040880u;
constexpr uint32_t kRaOwnedPieceSync = 0x800408B0u;
constexpr uint32_t kRaOwnedPieceEnqueue = 0x800408B8u;
constexpr uint32_t kRaKind5Draw = 0x800408C8u;
constexpr uint32_t kRaArmSceneEvent = 0x8004092Cu;
constexpr uint32_t kRaFinishSub2 = 0x80040954u;
constexpr uint32_t kRaFinishSub3 = 0x80040964u;
constexpr uint32_t kRaFinishOwnedSync = 0x8004098Cu;
constexpr uint32_t kRaFinishOwnedEnqueue = 0x80040994u;
constexpr uint32_t kRaFinishEnqueue = 0x800409E4u;
constexpr uint32_t kRaFinishOverlayCull = 0x80040A18u;
constexpr uint32_t kRaFinishBoundsCull = 0x80040A28u;
constexpr uint32_t kRaFinishDraw = 0x80040A38u;
constexpr uint32_t kRaDespawn = 0x80040A48u;

// SM states (node[4]) and the two sub-state families (node[5]).
constexpr uint8_t kStateSpawn = 0;
constexpr uint8_t kStateActive = 1;
constexpr uint8_t kStateFinish = 2;
constexpr uint8_t kStateDespawn = 3;

constexpr uint8_t kSpawnBuildPieces = 0; // allocate + place the two display pieces
constexpr uint8_t kSpawnRunKindInit = 1; // run the per-kind placement init, then go live

// State-1 sub-states (node[5]) — named for what their tick handler does (RE'd in the .h banner).
constexpr uint8_t kActiveWobble = 0;         // FUN_8003FD10 — jitter both pieces on a 16-frame timer
constexpr uint8_t kActiveWobbleSfx = 1;      // FUN_8003FED8 — same + SFX 0x19
constexpr uint8_t kActiveReleaseDrop = 2;    // FUN_8003FFCC — SFX 0x1A + spawn the contents object
constexpr uint8_t kActiveFall = 3;           // FUN_8004022C — gravity fall, burst + despawn on landing
constexpr uint8_t kActiveBurstOnContact = 4; // FUN_80040390 — one-shot burst gated on node[0x29]
constexpr uint8_t kActiveOverlayTick = 5;    // a05's own tick

// State-2 sub-states (node[5]).
constexpr uint8_t kFinishArmEvent = 1;  // arm scene event 56, then hand the owner its kind
constexpr uint8_t kFinishRestart = 2;   // FUN_8003FE00 — pick a per-variant global, back to state 1
constexpr uint8_t kFinishWobbleSfx = 3; // FUN_8003FED8 — the state-1 wobble+SFX handler again

// node[0x5E] kinds — the placement record's designer-chosen flavour byte.
constexpr uint8_t kKindGroundAligned = 0;
constexpr uint8_t kKindGroundProbeAhead = 1;
constexpr uint8_t kKindOwnedPiece = 2; // slaved to node[0x10]: mirrors its owner's visibility
constexpr uint8_t kKindNoInit = 3;
constexpr uint8_t kKindFixedYaw = 4;
constexpr uint8_t kKindOverlayA04 = 5;
constexpr uint8_t kKindRoomGatedGround = 6; // init retries until this prop's room is active
constexpr uint8_t kKindOverlayA05 = 7;

constexpr uint8_t kFlagSkipCull = 0x80; // node[0x28] bit: submit without the bounds cull
constexpr uint16_t kDefaultXzRadius = 64;
constexpr uint16_t kDefaultHeight = 128;
constexpr uint16_t kDefaultYBand = 150;

namespace {

// Head of both render tails (@7E0 and @99C in step() below): is the prop's own room the active one?
// GOTCHA: the guest compares a ZERO-extended byte against the SIGN-extended halfword node[0x6A]
// over the full 32 bits — same predicate as game/ai/beh_visibility_gate_dispatch.cpp's state1_gate.
bool roomIsActive(Core *c, const PlacedProp &prop) {
  if (c->mem_r8(kGblRoomValid) == 0) {
    return false;
  }
  return (uint32_t)(uint8_t)c->mem_r8(kGblRoomId) == (uint32_t)prop.roomId();
}

} // namespace

// Guest-stack frame contract, from `binary ABI evidence 80040558 --scaffold --guestabi`.
static constexpr GuestFrameSpill kSpills_80040558[2] = {
    {16, 16},
    {31 /*ra*/, 20},
};

// ORACLE: guest 0x80040558
void PlacedPropSm::step(Core *c) {
  GuestFrame<24, 2> frame(c, kSpills_80040558);
  GuestReg<16> node(c); // s0 = the node, live across every call (see header TRAP)
  node = c->r[4];
  PlacedProp prop(c, node);

  switch (prop.state()) {

  // ======================= STATE 0 — SPAWN (0x800405AC) =======================
  case kStateSpawn:
    switch (prop.sub()) {

    case kSpawnBuildPieces: { // 0x800405CC
      // Allocate + place the prop's two display pieces. Returns 0 while the shared child pool is
      // short (it then parks the prop in state 3 itself), in which case we stay in sub 0 and retry.
      c->r[5] = prop.variant();
      c->r[4] = node;
      tomba::guest::dispatchJalToReturn(*c, 0x80040410u, kRaBuildPieces);
      if (c->r[2] != 0) {
        prop.setPropSub((uint8_t)(prop.sub() + 1));
      }
      // 0x800405F4 — seed the collision box + clear the per-frame flags. Runs on BOTH paths.
      prop.setPropXzRadius(kDefaultXzRadius);
      prop.setPropHeight(kDefaultHeight);
      prop.setPropGate(0);
      prop.setPropCollDone(0);
      prop.setPropStatus(0);
      prop.setPropYBandLo(kDefaultYBand);
      prop.setPropYBandHi(kDefaultYBand);
      prop.setPropFlag46(0);
      break;
    }

    case kSpawnRunKindInit: { // 0x80040620
      // JT @0x800152E0 indexed by node[0x5E]. Kinds 0..5 ignore their init's return value; kinds 6
      // and 7 are RETRYING inits — they return 0 while their precondition (the prop's room being
      // the active one) is unmet, and the prop then stays in state 0 / sub 1 for another frame.
      bool initDone = true;
      switch (prop.kind()) {
      case kKindGroundAligned: // JT[0] = 0x80040650
        c->r[4] = node;
        tomba::guest::dispatchJalToReturn(*c, 0x8003FBC4u, kRaKind0Init);
        break;
      case kKindGroundProbeAhead: // JT[1] = 0x80040660
        c->r[4] = node;
        tomba::guest::dispatchJalToReturn(*c, 0x8003FC00u, kRaKind1Init);
        break;
      case kKindOwnedPiece: // JT[2] = 0x80040670
        c->r[4] = node;
        tomba::guest::dispatchJalToReturn(*c, kFnKind2Init, kRaKind2Init);
        break;
      case kKindFixedYaw: // JT[4] = 0x80040680
        c->r[4] = node;
        tomba::guest::dispatchJalToReturn(*c, 0x8003FC78u, kRaKind4Init);
        break;
      case kKindOverlayA04: // JT[5] = 0x80040690
        c->r[4] = node;
        tomba::guest::dispatchJalToReturn(*c, kFnKind5Init, kRaKind5Init);
        break;
      case kKindRoomGatedGround: // JT[6] = 0x800406A0
        c->r[4] = node;
        tomba::guest::dispatchJalToReturn(*c, 0x8003FC8Cu, kRaKind6Init);
        initDone = (c->r[2] != 0);
        break;
      case kKindOverlayA05: // JT[7] = 0x800406B0
        c->r[4] = node;
        tomba::guest::dispatchJalToReturn(*c, kFnKind7Init, kRaKind7Init);
        initDone = (c->r[2] != 0);
        break;
      default: // JT[3], and kind >= 8 -> 0x800406C0
        break;
      }
      if (!initDone) {
        break; // 0x800406B8 — retry next frame
      }
      prop.setPropState(kStateActive); // 0x800406C4 — go live
      prop.setPropSub(0);
      prop.setPropActive(1);
      prop.setPropGate(0);
      break;
    }

    default:
      break; // node[5] >= 2 in state 0: nothing
    }
    break;

  // ======================= STATE 1 — ACTIVE (0x800406D8) ======================
  case kStateActive: {
    // Two whole-area freezes: area 18 only ticks its props while the gate byte is set, and area 6
    // stops them entirely while it is in sub-state 19.
    const uint8_t area = c->mem_r8(kGblAreaId);
    if (area == kAreaPropsGated) {
      if (c->mem_r8(kGblAreaGate) == 0) {
        break; // 0x800406FC
      }
    } else if (area == kAreaSubFrozen && c->mem_r8(kGblAreaSub) == kAreaSubFrozenVal) {
      break; // 0x8004071C
    }

    // 0x80040720 — per-sub-state tick, JT @0x80015300 indexed by node[5].
    switch (prop.sub()) {
    case kActiveWobble: // JT[0] = 0x80040750
      c->r[4] = node;
      tomba::guest::dispatchJalToReturn(*c, 0x8003FD10u, kRaSub0Tick);
      break;
    case kActiveWobbleSfx: // JT[1] = 0x80040760
      c->r[4] = node;
      tomba::guest::dispatchJalToReturn(*c, 0x8003FED8u, kRaSub1Tick);
      break;
    case kActiveReleaseDrop: // JT[2] = 0x80040770
      c->r[4] = node;
      tomba::guest::dispatchJalToReturn(*c, 0x8003FFCCu, kRaSub2Tick);
      break;
    case kActiveFall: // JT[3] = 0x80040780
      c->r[4] = node;
      tomba::guest::dispatchJalToReturn(*c, 0x8004022Cu, kRaSub3Tick);
      break;
    case kActiveBurstOnContact: // JT[4] = 0x80040790
      c->r[4] = node;
      tomba::guest::dispatchJalToReturn(*c, 0x80040390u, kRaSub4Tick);
      break;
    case kActiveOverlayTick: // JT[5] = 0x800407A0
      c->r[4] = node;
      tomba::guest::dispatchJalToReturn(*c, kFnSub5Tick, kRaSub5Tick);
      break;
    default:
      break; // node[5] >= 6 -> straight to 0x800407A8
    }

    // 0x800407A8 — per-kind render tail, JT @0x80015318 indexed by node[0x5E].
    switch (prop.kind()) {
    case kKindOverlayA05: // JT[7] = 0x800407D8
      c->r[4] = node;
      tomba::guest::dispatchJalToReturn(*c, kFnKind7PreDraw, kRaKind7PreDraw);
      [[fallthrough]];
    case kKindGroundAligned: // JT[0,1,3,4,6] = 0x800407E0
    case kKindGroundProbeAhead:
    case kKindNoInit:
    case kKindFixedYaw:
    case kKindRoomGatedGround: {
      // The shared VISIBILITY GATE. In the prop's own active room it submits whenever the
      // skip-cull flag is set; anywhere else it must survive the bounds cull first.
      if (roomIsActive(c, prop)) {
        if ((prop.flags28() & kFlagSkipCull) == 0) {
          goto activeClearGate;
        }
        prop.setPropVisible(1);
        c->r[4] = node;
        tomba::guest::dispatchJalToReturn(*c, 0x80077E7Cu, kRaActiveEnqueue);
      } else { // 0x80040834
        if ((prop.flags28() & kFlagSkipCull) != 0) {
          goto activeClearGate;
        }
        if (c->mem_r8(kGblAreaId) == kAreaOverlayCull) {
          c->r[4] = node;
          tomba::guest::dispatchJalToReturn(*c, kFnOverlayCull, kRaActiveOverlayCull);
        } else {
          c->r[4] = node;
          tomba::guest::dispatchJalToReturn(*c, 0x8007778Cu, kRaActiveBoundsCull);
        }
        if (c->r[2] == 0) {
          goto activeClearGate; // 0x80040870 — culled
        }
      }
      c->r[4] = node; // 0x80040878 — visible: transform + draw
      tomba::guest::dispatchJalToReturn(*c, 0x800517F8u, kRaActiveDraw);
      prop.setPropGate(0);
      goto activeEndOfFrame;
    }
    case kKindOwnedPiece: { // JT[2] = 0x80040888
      // A piece slaved to node[0x10]: it is visible exactly when its owner is, never culled itself.
      const uint8_t ownerVisible = c->mem_r8(prop.owner() + PlacedProp::kOffVisible);
      prop.setPropVisible(ownerVisible);
      if (ownerVisible == 0) {
        goto activeClearGate;
      }
      c->r[4] = node;
      tomba::guest::dispatchJalToReturn(*c, kFnOwnedPieceSync, kRaOwnedPieceSync);
      c->r[4] = node;
      tomba::guest::dispatchJalToReturn(*c, 0x80077E7Cu, kRaOwnedPieceEnqueue);
      prop.setPropGate(0);
      goto activeEndOfFrame;
    }
    case kKindOverlayA04: // JT[5] = 0x800408C0
      c->r[4] = node;
      tomba::guest::dispatchJalToReturn(*c, kFnKind5Draw, kRaKind5Draw);
      [[fallthrough]];
    default: // kind >= 8 lands here too
      break;
    }
  activeClearGate: // 0x800408C8
    prop.setPropGate(0);
  activeEndOfFrame: // 0x800408CC
    prop.setPropStatus(0);
    break;
  }

  // ======================= STATE 2 — FINISH (0x800408D4) ======================
  case kStateFinish: {
    // JT @0x80015338 indexed by node[5]; slots 0 and 4 go straight to the mirror/render tail.
    switch (prop.sub()) {
    case kFinishArmEvent: // JT[1] = 0x80040904
      if (prop.variant() == 0 && c->mem_r8(kGblFinishGate) == 0) {
        c->r[4] = 56; // scene-event id 56
        tomba::guest::dispatchJalToReturn(*c, 0x80040B48u, kRaArmSceneEvent);
      }
      if (prop.kind() != kKindOwnedPiece) {
        goto finishRender; // 0x8004092C
      }
      PlacedProp(c, prop.owner()).setPropKind(1); // hand the owner its "piece done" kind
      break;
    case kFinishRestart: // JT[2] = 0x8004094C
      c->r[4] = node;
      tomba::guest::dispatchJalToReturn(*c, 0x8003FE00u, kRaFinishSub2);
      break;
    case kFinishWobbleSfx: // JT[3] = 0x8004095C
      c->r[4] = node;
      tomba::guest::dispatchJalToReturn(*c, 0x8003FED8u, kRaFinishSub3);
      break;
    default:
      break; // JT[0], JT[4], node[5] >= 5
    }

    // 0x80040964 — an owner-linked piece mirrors its owner and submits; everything else renders below.
    if (prop.kind() != kKindOwnedPiece) {
      goto finishRender;
    }
    prop.setPropVisible(c->mem_r8(prop.owner() + PlacedProp::kOffVisible));
    c->r[4] = node;
    tomba::guest::dispatchJalToReturn(*c, kFnOwnedPieceSync, kRaFinishOwnedSync);
    c->r[4] = node;
    tomba::guest::dispatchJalToReturn(*c, 0x80077E7Cu, kRaFinishOwnedEnqueue);
    break;

  finishRender: // 0x8004099C
    // The state-1 gate again — the guest's second compiled copy (see the file banner).
    if (roomIsActive(c, prop)) {
      if ((prop.flags28() & kFlagSkipCull) == 0) {
        break;
      }
      prop.setPropVisible(1);
      c->r[4] = node;
      tomba::guest::dispatchJalToReturn(*c, 0x80077E7Cu, kRaFinishEnqueue);
    } else { // 0x800409EC
      if ((prop.flags28() & kFlagSkipCull) != 0) {
        break;
      }
      if (c->mem_r8(kGblAreaId) == kAreaOverlayCull) {
        c->r[4] = node;
        tomba::guest::dispatchJalToReturn(*c, kFnOverlayCull, kRaFinishOverlayCull);
      } else {
        c->r[4] = node;
        tomba::guest::dispatchJalToReturn(*c, 0x8007778Cu, kRaFinishBoundsCull);
      }
      if (c->r[2] == 0) {
        break; // 0x80040A28 — culled
      }
    }
    c->r[4] = node; // 0x80040A30
    tomba::guest::dispatchJalToReturn(*c, 0x800517F8u, kRaFinishDraw);
    break;
  }

  // ======================= STATE 3 — DESPAWN (0x80040A40) =====================
  case kStateDespawn:
    c->r[4] = node;
    tomba::guest::dispatchJalToReturn(*c, 0x8007A624u, kRaDespawn);
    break;

  default:
    break; // node[4] >= 4: nothing
  }
}

void PlacedPropSm::registerOverrides(Game *) {
  tomba::native::declareOverride(0x80040558u, "PlacedPropSm::step", &PlacedPropSm::step);
}
