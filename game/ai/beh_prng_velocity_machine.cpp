// game/ai/beh_prng_velocity_machine.cpp — PC-native per-object BEHAVIOR handler FUN_80117658:
// the seaside WANDERING COLLECTIBLE (the actor Tomba picks the BUCKET up from).
//
// One handler, two variants selected by the object's `variant` byte:
//   variant 0 (GROUND) — a ground collectible that idles, drifts on a re-rolled random timer, and on
//                        collection hands over item 58 and retires.
//   variant 1 (CARRIED) — the BUCKET carrier: it wanders under the steering helper FUN_801141C8,
//                        rides a carrier transform (node[0xC0]), and on collection runs the full
//                        item-get sequence: give item 40, arm cutscene mode, show the get-popup, wait
//                        for the player to come back out of the cutscene pose, then run the end-of-
//                        pickup script and retire.
//
// The outer state is `state` (node[4]) — SPAWN → ACTIVE → COLLECTED → RETIRE; within ACTIVE and
// COLLECTED a `phase` byte (node[5]) sequences the steps. See PickupState/ActivePhase/CollectPhase.
//
// FIDELITY: this reads as game code but is byte-exact against the guest. Every store keeps the guest's
// width, signedness and ORDER (including stores the guest puts in a jal delay slot, which execute
// BEFORE the callee — those are marked). Sub-behaviour leaves that are not yet owned are reached by
// typed runtime address dispatch with the guest's own a0..a2, so they see the identical interface. The handler pushes
// the guest's 32-byte frame via GuestFrame so the guest stack bytes match too (CLAUDE.md "MIRROR THE
// GUEST STACK"). RE: Ghidra decompile of FUN_80117658 off a live RAM dump (scratch/decomp/pickup.c);
// equivalence proven by A/B 2 MB RAM+scratchpad dump diff on replays/bugs/bucket-softlock.pad.

#include "cfg.h"
#include "core.h"
#include "core/entry/game_ctx.h"
#include "graphics_bind.h" // GraphicsBind::setGeom / recordInit / renderUpdate
#include "guest_abi.h"     // GuestFrame — mirror the guest stack frame (CLAUDE.md)
#include "guest_call.h"
#include "inventory.h" // class Inventory — inv(c).give / giveAndFlag
#include "spawn.h"     // class Spawn (eng(c).spawn.despawn)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace {

// ---------------------------------------------------------------------------------------------
// Guest routines this handler drives that are not owned natively yet. Reached through typed runtime address dispatch
// with the guest's own argument registers, so the interface they observe is unchanged.
namespace Guest {
constexpr uint32_t kPlaySfx = 0x80074590u; // FUN_80074590(id, 0, 0) — SFX / song router
constexpr uint32_t kSteerWanderer =
    0x801141C8u;                          // FUN_801141C8(obj) — overlay steering; leaves a turn code in obj.steerResult
constexpr uint32_t kRandom = 0x8009A450u; // FUN_8009A450() — shared PRNG draw
constexpr uint32_t kStepMotion = 0x8007778Cu;      // FUN_8007778C(obj) — integrate the motion block; 0 = not ready
constexpr uint32_t kCommitGroundPos = 0x80077B5Cu; // FUN_80077B5C(obj) — commit position for the ground variant
constexpr uint32_t kSyncCollectible = 0x8004B374u; // FUN_8004B374(obj, variant) — per-frame collectible sync
constexpr uint32_t kBeginCollect = 0x8004B0D8u;    // FUN_8004B0D8(obj) — start the collection reaction
constexpr uint32_t kArmCollectible = 0x8004B354u;  // FUN_8004B354(obj, variant) — finish spawn-time setup
constexpr uint32_t kSpawnGetPopup =
    0x8004BD04u; // FUN_8004BD04(obj, 0, 0) — spawn the "got item" popup; returns its node
constexpr uint32_t kBindGetPopup =
    0x8004BEA8u; // FUN_8004BEA8(itemId, popupNode) — publish the popup to the HUD globals
constexpr uint32_t kEnterCutsceneMode =
    0x80042354u; // FUN_80042354(mode, musicSel) — sets scratchpad cut-mode 0x1F800137
constexpr uint32_t kPlayerLeftCutscene =
    0x8005308Cu; // FUN_8005308C() — nonzero once the player is back out of the cutscene pose
constexpr uint32_t kRunScript = 0x80040CDCu; // FUN_80040CDC(obj, 0, script) — run an event script on the object
constexpr uint32_t kAwaitScript =
    0x80041098u; // FUN_80041098(obj) — pump the running script; clears obj.scriptToken when done
} // namespace Guest

// Guest globals ---------------------------------------------------------------------------------
constexpr uint32_t kGeomTableGround = 0x800ECF80u; // u32: geometry descriptor the ground variant binds
constexpr uint32_t kGeomDefGround = 0x8014C808u;   // geometry definition passed to GraphicsBind::setGeom
constexpr uint32_t kBucketScript = 0x80148574u;    // event script run when the bucket pickup completes
constexpr uint32_t kAreaEventFlags = 0x800BF9DCu;  // u8 bitfield: bit0 = "bucket collected" in this area
constexpr uint32_t kCarrierBusy = 0x1F800207u; // scratchpad u8: carrier pipeline depth; >=5 stalls the carried variant

// Position the motion step publishes for a collectible that has been picked up ------------------
constexpr uint32_t kPickedUpX = 0x1F800160u; // scratchpad u16 x3 — where a collected item snaps to
constexpr uint32_t kPickedUpY = 0x1F800162u;
constexpr uint32_t kPickedUpZ = 0x1F800164u;

// Item ids --------------------------------------------------------------------------------------
constexpr uint32_t kItemGroundPickup = 58; // variant 0's reward
constexpr uint32_t kItemBucket = 40;       // variant 1's reward

// SFX ids ---------------------------------------------------------------------------------------
constexpr uint32_t kSfxCollect = 45; // played with bank 66 when the bucket is collected
constexpr uint32_t kSfxCollectBank = 66;
constexpr uint32_t kSfxLaunch = 146; // played once as the collectible launches

// Angles are PSX 12-bit-per-quadrant: 4096 = 90 degrees.
constexpr int32_t kQuarterTurn = 4096;
constexpr int16_t kHalfQuarter = 2048;

// ---------------------------------------------------------------------------------------------
// Outer state (obj.state, node[4]).
enum PickupState : uint8_t {
  SPAWN = 0,     // one-shot construction, then -> ACTIVE
  ACTIVE = 1,    // idling / wandering, waiting to be collected
  COLLECTED = 2, // the collection sequence (differs per variant)
  RETIRE = 3,    // despawn
};

// obj.variant (node[3]).
enum PickupVariant : uint8_t { GROUND = 0, CARRIED = 1 };

// obj.phase (node[5]) while state == ACTIVE.
enum ActivePhase : uint8_t {
  LAUNCH = 0,   // first frame(s): kick the motion block, or re-roll the wander timer
  SETTLING = 1, // hold timer running down, then drop to the resting height
  RESTING = 2,  // at rest; the motion step publishes the pick-up position
};

// obj.phase (node[5]) while state == COLLECTED, CARRIED variant.
enum CollectPhase : uint8_t {
  HANDOVER = 0,     // give the item, arm cutscene mode, spawn the popup
  AWAIT_PLAYER = 1, // wait for the player to leave the cutscene pose, then start the script
  AWAIT_SCRIPT = 2, // pump the script until it releases obj.scriptToken, then RETIRE
};

constexpr uint8_t kScriptTokenFree = 255; // obj.scriptToken value meaning "no script outstanding"

// The steering helper's verdict, left in obj.steerResult by Guest::kSteerWanderer.
enum SteerResult : uint8_t { NO_TURN = 0, TURN_LEFT = 1, TURN_RIGHT = 2 };

// ---------------------------------------------------------------------------------------------
// MotionBlock — the physics/animation block the object points at through node[0x10]. FUN_8007778C
// integrates it; this handler only seeds and steers it.
struct MotionBlock {
  Core *c;
  uint32_t a;
  uint16_t x() const {
    return c->mem_r16(a + 0);
  }
  uint16_t y() const {
    return c->mem_r16(a + 2);
  }
  uint16_t spin() const {
    return c->mem_r16(a + 12);
  }
  void setAnchor(uint16_t x, uint16_t y, uint16_t z) const { // guest order: X, Y, (…), Z
    c->mem_w16(a + 6, x);
    c->mem_w16(a + 8, y);
    c->mem_w16(a + 10, z);
  }
  void setAnchorX(uint16_t v) const {
    c->mem_w16(a + 6, v);
  }
  void setAnchorY(uint16_t v) const {
    c->mem_w16(a + 8, v);
  }
  void setAnchorZ(uint16_t v) const {
    c->mem_w16(a + 10, v);
  }
  void setSpin(uint16_t v) const {
    c->mem_w16(a + 12, v);
  }

  int16_t yaw() const {
    return c->mem_r16s(a + 14);
  }
  uint16_t yawBits() const {
    return c->mem_r16(a + 14);
  }
  void setYaw(uint16_t v) const {
    c->mem_w16(a + 14, v);
  }

  int16_t vel() const {
    return c->mem_r16s(a + 20);
  }
  void setVel(int16_t v) const {
    c->mem_w16(a + 20, (uint16_t)v);
  }

  uint16_t wanderTimer() const {
    return c->mem_r16(a + 22);
  }
  void setWanderTimer(uint16_t v) const {
    c->mem_w16(a + 22, v);
  }

  uint16_t holdTimer() const {
    return c->mem_r16(a + 24);
  }
  void setHoldTimer(uint16_t v) const {
    c->mem_w16(a + 24, v);
  }

  void setGravity(uint16_t v) const {
    c->mem_w16(a + 26, v);
  }
};

// ---------------------------------------------------------------------------------------------
// PickupObj — typed lens over the guest object node this handler owns.
struct PickupObj {
  Core *c;
  uint32_t a;

  uint8_t drawMode() const {
    return c->mem_r8(a + 0);
  }
  void setDrawMode(uint8_t v) const {
    c->mem_w8(a + 0, v);
  }
  uint8_t variant() const {
    return c->mem_r8(a + 3);
  }
  uint8_t state() const {
    return c->mem_r8(a + 4);
  }
  void setState(uint8_t v) const {
    c->mem_w8(a + 4, v);
  }
  uint8_t phase() const {
    return c->mem_r8(a + 5);
  }
  void setPhase(uint8_t v) const {
    c->mem_w8(a + 5, v);
  }
  void advancePhase() const {
    c->mem_w8(a + 5, (uint8_t)(c->mem_r8(a + 5) + 1));
  }

  MotionBlock motion() const {
    return MotionBlock{c, c->mem_r32(a + 0x10)};
  }

  uint8_t steerResult() const {
    return c->mem_r8(a + 0x2b);
  }
  void clearSteerResult() const {
    c->mem_w8(a + 0x2b, 0);
  }

  // Position: a 16.16 fixed-point triple at +0x2C/+0x30/+0x34 whose HIGH halves (+0x2E/+0x32/+0x36)
  // are the integer world coordinates every other system reads.
  void setPosXFixed(uint32_t v) const {
    c->mem_w32(a + 0x2c, v);
  }
  void setPosYFixed(uint32_t v) const {
    c->mem_w32(a + 0x30, v);
  }
  void setPosZFixed(uint32_t v) const {
    c->mem_w32(a + 0x34, v);
  }
  uint16_t x() const {
    return c->mem_r16(a + 0x2e);
  }
  uint16_t y() const {
    return c->mem_r16(a + 0x32);
  }
  uint16_t z() const {
    return c->mem_r16(a + 0x36);
  }
  void setX(uint16_t v) const {
    c->mem_w16(a + 0x2e, v);
  }
  void setY(uint16_t v) const {
    c->mem_w16(a + 0x32, v);
  }
  void setZ(uint16_t v) const {
    c->mem_w16(a + 0x36, v);
  }

  void setGeomPtr(uint32_t v) const {
    c->mem_w32(a + 0x3c, v);
  }
  void setSpriteYaw(uint16_t v) const {
    c->mem_w16(a + 0x58, v);
  }

  uint8_t spawnPhase() const {
    return c->mem_r8(a + 0x5e);
  }
  void setSpawnPhase(uint8_t v) const {
    c->mem_w8(a + 0x5e, v);
  }

  uint8_t scriptToken() const {
    return c->mem_r8(a + 0x70);
  }
  void setScriptToken(uint8_t v) const {
    c->mem_w8(a + 0x70, v);
  }

  uint32_t carrier() const {
    return c->mem_r32(a + 0xc0);
  }

  // Collision/interaction box, 4 halfwords at +0x80.
  void setBounds(uint16_t x0, uint16_t x1, uint16_t y0, uint16_t y1) const {
    c->mem_w16(a + 0x80, x0);
    c->mem_w16(a + 0x82, x1);
    c->mem_w16(a + 0x84, y0);
    c->mem_w16(a + 0x86, y1);
  }
};

// Guest-ABI call helpers: set a0..a2, dispatch, return v0. -------------------------------------
inline uint32_t call0(Core *c, uint32_t fn) {
  psx::cpu::dispatchGuestToReturn0(*c, fn, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
  return c->r[2];
}
inline uint32_t call1(Core *c, uint32_t a0, uint32_t fn) {
  c->r[4] = a0;
  psx::cpu::dispatchGuestToReturn0(*c, fn, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
  return c->r[2];
}
inline uint32_t call2(Core *c, uint32_t a0, uint32_t a1, uint32_t fn) {
  c->r[4] = a0;
  c->r[5] = a1;
  psx::cpu::dispatchGuestToReturn0(*c, fn, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
  return c->r[2];
}
inline uint32_t call3(Core *c, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t fn) {
  c->r[4] = a0;
  c->r[5] = a1;
  c->r[6] = a2;
  psx::cpu::dispatchGuestToReturn0(*c, fn, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
  return c->r[2];
}

// -----------------------------------------------------------------------------------------------
// SPAWN — build the object, then hand off to ACTIVE. Returns nothing; both variants end by bumping
// state and calling kArmCollectible with their variant number.
void spawnGround(Core *c, const PickupObj &obj) {
  const MotionBlock m = obj.motion();
  c->mem_w8(obj.a + 0x0b, 16);
  c->mem_w8(obj.a + 0x08, 248);
  c->mem_w8(obj.a + 0x47, 0);
  c->mem_w16(obj.a + 0x5a, 0);
  c->mem_w8(obj.a + 0x0d, 0);
  obj.setGeomPtr(c->mem_r32(kGeomTableGround)); // jal delay slot: stored before the callee
  c->r[4] = obj.a;
  c->r[5] = kGeomDefGround;
  c->r[6] = 1;
  eng(c).graphicsBind.setGeom(); // FUN_80077B38(obj, def, 1)

  obj.setPosXFixed(0x13d20000u);
  const uint16_t spawnX = obj.x(); // read between the two stores, as the guest does
  obj.setPosYFixed(0xf7e00000u);
  obj.setPosZFixed(0x15aa0000u);
  obj.setBounds(60, 120, 80, 160);
  obj.setDrawMode(4);

  m.setAnchorX(spawnX);
  m.setAnchorY((uint16_t)(obj.y() - 420));
  const uint16_t spawnZ = obj.z();
  m.setWanderTimer(30);
  m.setAnchorZ(spawnZ);
}

// Returns false if the record is not ready yet (the guest bails out and retries next frame).
bool spawnCarried(Core *c, const PickupObj &obj) {
  const MotionBlock m = obj.motion();
  obj.setDrawMode(4);
  obj.setSpawnPhase(0); // jal delay slot: stored before the callee
  c->r[4] = obj.a;
  c->r[5] = 1;
  c->r[6] = 0;
  eng(c).graphicsBind.recordInit(); // FUN_80051B70(obj, 1, 0)
  if (c->r[2] != 0) {
    return false;
  }

  obj.setPosXFixed(0x28d20000u);
  const uint16_t spawnX = obj.x();
  obj.setPosYFixed(0xf9e80000u);
  obj.setPosZFixed(0x0f800000u);
  obj.setBounds(70, 140, 140, 140);

  m.setAnchorX(spawnX);
  m.setAnchorY((uint16_t)(obj.y() - 400));
  const uint16_t spawnZ = obj.z();
  m.setSpin(82);
  m.setAnchorZ(spawnZ);
  return true;
}

// -----------------------------------------------------------------------------------------------
// ACTIVE / LAUNCH — the frame the collectible is kicked into motion, or (once launched) the idle
// wander: when it has come to rest and the wander timer expires, re-roll a new timer and a new
// upward impulse from the shared PRNG.
void activeLaunch(Core *c, const PickupObj &obj) {
  const MotionBlock m = obj.motion();
  if (obj.spawnPhase() == 1) { // launch frame
    call3(c, kSfxLaunch, 0, 0, Guest::kPlaySfx);
    m.setHoldTimer(36);
    m.setGravity(1408);
    m.setVel(10240);
    obj.advancePhase();
    return;
  }
  if (obj.variant() == GROUND) {
    if (m.vel() != 0) {
      return; // still moving — nothing to re-roll
    }
    const uint16_t left = (uint16_t)(m.wanderTimer() - 1);
    m.setWanderTimer(left);
    if ((int16_t)left >= 0) {
      return;
    }
    const uint32_t r1 = call0(c, Guest::kRandom);
    const uint32_t r2 = call0(c, Guest::kRandom);
    m.setWanderTimer((uint16_t)((int32_t)((r1 & 3u) * 30u) + ((int32_t)r2 >> 11) + 30));
    const uint32_t r3 = call0(c, Guest::kRandom);
    m.setVel((int16_t)((-(int32_t)((r3 & 7u) + 10u)) << 8));
    return;
  }
  // CARRIED: the overlay steering helper decides whether to turn, and how.
  obj.clearSteerResult(); // jal delay slot: stored before the callee
  call1(c, obj.a, Guest::kSteerWanderer);
  const uint8_t steer = obj.steerResult();
  if (steer == NO_TURN) {
    return;
  }
  if (obj.variant() == GROUND) { // guest re-tests; unreachable on this path
    m.setVel(steer == TURN_RIGHT ? (int16_t)8192 : (int16_t)-8192);
    return;
  }
  if (obj.variant() != CARRIED) {
    return;
  }
  if (steer == TURN_RIGHT) {
    const int16_t yaw = m.yaw();
    m.setYaw(yaw < kHalfQuarter ? (uint16_t)(m.yawBits() - kQuarterTurn) : (uint16_t)(-(int32_t)m.yawBits()));
    m.setVel(3072);
  } else {
    const int16_t yaw = m.yaw();
    m.setYaw(yaw < -(kHalfQuarter - 1) ? (uint16_t)(-(int32_t)m.yawBits()) : (uint16_t)(m.yawBits() + kQuarterTurn));
    m.setVel(-3072);
  }
}

// ACTIVE / SETTLING — run the hold timer down, then drop the object to its resting height.
void activeSettling(Core *c, const PickupObj &obj) {
  const MotionBlock m = obj.motion();
  const uint16_t hold = m.holdTimer();
  if ((int16_t)hold != 0 && obj.spawnPhase() != 0) {
    m.setHoldTimer((uint16_t)(hold - 1));
    return;
  }
  m.setVel(-10240);
  m.setGravity(512);
  if (obj.variant() == GROUND) {
    obj.setZ(5520);
  } else if (obj.variant() == CARRIED) {
    obj.setZ(3968);
  }
  obj.setDrawMode(1);
  obj.setSpawnPhase(2);
  obj.advancePhase();
}

// ACTIVE tail — integrate the motion block and publish the object's world position. Shared by every
// ACTIVE phase (the guest falls through to it from all of them).
void activeCommitPosition(Core *c, const PickupObj &obj) {
  if (obj.variant() == GROUND) {
    const MotionBlock m = obj.motion();
    if (call1(c, obj.a, Guest::kStepMotion) == 0) {
      return;
    }
    if (obj.phase() == RESTING) {
      obj.setX(c->mem_r16(kPickedUpX));
      obj.setY(c->mem_r16(kPickedUpY));
      obj.setZ(c->mem_r16(kPickedUpZ));
    } else {
      obj.setX((uint16_t)(m.x() - 32));
      obj.setY((uint16_t)(m.y() + 80));
    }
    call1(c, obj.a, Guest::kCommitGroundPos);
    call2(c, obj.a, 0, Guest::kSyncCollectible);
    return;
  }
  if (obj.variant() != CARRIED) {
    return;
  }
  const bool carrierBusy = c->mem_r8(kCarrierBusy) >= 5; // guest reads the gate before the block ptr
  const MotionBlock m = obj.motion();
  if (carrierBusy) {
    return; // carrier pipeline saturated — skip this frame
  }
  if (call1(c, obj.a, Guest::kStepMotion) == 0) {
    return;
  }
  if (obj.phase() == RESTING) {
    obj.setX(c->mem_r16(kPickedUpX));
    obj.setY(c->mem_r16(kPickedUpY));
    obj.setZ(c->mem_r16(kPickedUpZ));
  } else {
    // Offset by the carrier's own sway, scaled 9/256.
    const uint32_t carrier = obj.carrier();
    obj.setX((uint16_t)(m.x() + (((int32_t)c->mem_r16s(carrier + 26) * 9) >> 8)));
    obj.setY((uint16_t)(m.y() + (((int32_t)c->mem_r16s(carrier + 32) * 9) >> 8)));
    obj.setSpriteYaw((uint16_t)(-(int32_t)m.spin()));
  }
  c->r[4] = obj.a;
  eng(c).graphicsBind.renderUpdate(); // FUN_800517F8(obj)
  call2(c, obj.a, 1, Guest::kSyncCollectible);
}

// -----------------------------------------------------------------------------------------------
// COLLECTED — the pick-up sequence.
void collectGround(Core *c, const PickupObj &obj) {
  inv(c).giveAndFlag(kItemGroundPickup, 1); // FUN_8004D4C4(58, 1)
  call1(c, obj.a, Guest::kBeginCollect);
  obj.setState(RETIRE);
}

void collectCarried(Core *c, const PickupObj &obj) {
  switch (obj.phase()) {
  case HANDOVER: {
    inv(c).give(kItemBucket, 1); // FUN_8004D4F4(40, 1)
    call2(c, kSfxCollect, kSfxCollectBank, 0x8004ED94u);
    call1(c, obj.a, Guest::kBeginCollect);
    const uint32_t popup = call3(c, obj.a, 0, 0, Guest::kSpawnGetPopup);
    if (popup != 0) {
      call2(c, kItemBucket, popup, Guest::kBindGetPopup);
      call2(c, 1, 1, Guest::kEnterCutsceneMode); // cut-mode 1: parks the player, opens the get-box
      c->mem_w8(kAreaEventFlags, (uint8_t)(c->mem_r8(kAreaEventFlags) | 1));
    }
    obj.advancePhase();
    return;
  }
  case AWAIT_PLAYER: {
    // The guest compares obj.phase() against obj.variant() here (both 1) — 80117be0 `beq v1,v0`,
    // where v0 is the variant byte loaded at 80117ba0. Transcribing that as a literal 3 (the value
    // 80117bd4 loads in the delay slot of the OTHER branch's jump, which never executes on this
    // path) stranded phase at AWAIT_PLAYER forever: the wait below never ran, the end-of-pickup
    // script never started, and cut-mode 0x1F800137 stayed 1 — the bucket-cutscene softlock.
    if (call0(c, Guest::kPlayerLeftCutscene) == 0) {
      return;
    }
    call3(c, obj.a, 0, kBucketScript, Guest::kRunScript);
    obj.setScriptToken(COLLECTED); // guest stores the outer state byte (2)
    obj.advancePhase();
    return;
  }
  case AWAIT_SCRIPT: {
    // PSXPORT_DEBUG=pickup — the bucket cutscene's progress: the script token the end-of-pickup
    // script hands back, and the script cursor it is parked on.
    cfg_logf("pickup",
             "obj=%08X token=%u scriptPtr=%08X flags71=%02X player=%02X/%02X cut=%u",
             obj.a,
             obj.scriptToken(),
             c->mem_r32(obj.a + 0x6c),
             c->mem_r8(obj.a + 0x71),
             c->mem_r8(0x800E7E84u),
             c->mem_r8(0x800E7E85u),
             c->mem_r8(0x1F800137u));
    if (obj.scriptToken() != kScriptTokenFree) {
      call1(c, obj.a, Guest::kAwaitScript);
      return;
    }
    obj.setState(RETIRE);
    return;
  }
  default:
    return;
  }
}

} // namespace

static constexpr GuestFrameSpill kSpills_80117658[4] = {
    {17, 20},
    {31 /*ra*/, 28},
    {18, 24},
    {16, 16},
}; // frame=32, abi_extract --scaffold --guestabi

void beh_prng_velocity_machine(Core *c) {
  GuestFrame<32, 4> frame(c, kSpills_80117658);
  const PickupObj obj{c, c->r[4]};

  switch (obj.state()) {
  case SPAWN: {
    if (obj.variant() == GROUND) {
      spawnGround(c, obj);
      c->mem_w8(obj.a + 4, (uint8_t)(obj.state() + 1)); // jal delay slot: stored before the callee
      call2(c, obj.a, GROUND, Guest::kArmCollectible);
    } else if (obj.variant() == CARRIED) {
      if (!spawnCarried(c, obj)) {
        return;
      }
      c->mem_w8(obj.a + 4, (uint8_t)(obj.state() + 1)); // jal delay slot: stored before the callee
      call2(c, obj.a, CARRIED, Guest::kArmCollectible);
    }
    return;
  }
  case ACTIVE: {
    switch (obj.phase()) {
    case LAUNCH:
      activeLaunch(c, obj);
      break;
    case SETTLING:
      activeSettling(c, obj);
      break;
    default:
      break; // RESTING and beyond: straight to the tail
    }
    activeCommitPosition(c, obj);
    return;
  }
  case COLLECTED: {
    if (obj.variant() == GROUND) {
      collectGround(c, obj);
    } else if (obj.variant() == CARRIED) {
      collectCarried(c, obj);
    }
    return;
  }
  case RETIRE:
    eng(c).spawn.despawn(obj.a); // FUN_8007A624(obj)
    return;
  default:
    return;
  }
}
