// game/object/actor.h — Actor: a NAMED-FIELD view over a guest object node.
//
// The engine's per-object BEHAVIOR handlers (game/ai/beh_*) were transcribed from Ghidra output as free
// functions indexed by magic byte offsets — `obj[+0x42]`, `obj[+0x60]`, `obj[+0x80]`. That's
// black-box-transcription shape (per [[feedback_re_first_not_black_box]]): reading the code doesn't tell
// you what field 0x42 IS, so every diagnostic on it has to re-derive the semantic model. This class is the
// first move toward FIXING that: an Actor holds a Core* and a guest object address, and exposes typed
// getters/setters for the fields we've RE'd — so a handler reads `a.state()` / `a.setOscPhase(0)` /
// `a.setBoxX(0x1e)` instead of `mem_r8(obj + 4)` / `mem_w16(obj + 0x42, 0)` / `mem_w16(obj + 0x80, 0x1e)`.
//
// USAGE: create one at the top of a beh_ handler: `Actor a(c, c->r[4]);` and use its methods. The wrapper
// is trivially cheap (two pointer-sized fields, no allocation); no state is mirrored — every access goes
// through c->mem_r*/mem_w* so guest state stays authoritative and no synchronization issue exists.
//
// FIELD COVERAGE: this file grows FIELD BY FIELD as more beh_ handlers get RE'd. Each field addition
// documents WHERE the semantic was learned (which handler / which sub-behavior / what SBS finding named
// it). Do NOT define fields speculatively — only when a handler's RE actually resolved the semantic.
#pragma once
#include "core.h"
#include "core/entry/game_ctx.h"
#include <cstdint>

class Actor {
public:
  Actor(Core *core, uint32_t addr) : c(core), obj(addr) {}

  // The raw guest node pointer (for legacy inline `mem_r*(obj+X)` in code paths that still index unnamed
  // fields, and for `typed runtime address dispatch` sub-behavior calls that take the node in c->r[4]). Prefer named
  // accessors — this is the escape hatch for the not-yet-RE'd offsets.
  uint32_t addr() const {
    return obj;
  }
  // Core back-pointer for helper ports that need to reach rngOf(c).next() / trigOf(c).rcos / typed runtime address
  // dispatch — any per-actor tick natively porting one of the beh_ handlers' sub-behaviors.
  Core *core() const {
    return c;
  }

  // ── Lifecycle & state ────────────────────────────────────────────────────────────────────────────
  // state (obj+4, u8): per-object state machine byte. Common convention observed across the beh_
  //   handlers: 0 = init (allocate render record, seed fields, advance to 1), 1 = active (per-frame
  //   tick), 2/3 = despawn (call spawn.despawn(node)), >=4 = no-op. Named from
  //   beh_typed_table_seed_gate (FUN_80133C14) state-machine RE: sibling handlers FUN_800739AC /
  //   FUN_80073CD8 share this convention.
  uint8_t state() const {
    return c->mem_r8(obj + 0x04);
  }
  void setState(uint8_t v) {
    c->mem_w8(obj + 0x04, v);
  }

  // alive (obj+0, u8): live/render-enable flag. Handlers set this to 1 alongside state=1 (Active) in
  //   the init state. Semantics beyond "set-when-active" not yet fully RE'd — treat as "handler-set
  //   live bit" for now.
  uint8_t alive() const {
    return c->mem_r8(obj + 0x00);
  }
  void setAlive(uint8_t v) {
    c->mem_w8(obj + 0x00, v);
  }

  // renderMode (obj+1, u8): a secondary state byte written by beh_typed_table_seed_gate in the
  //   "scene gate == 0x22" branch (state 1) — set to the current state value (1). Purpose beyond the
  //   handler's local use not yet RE'd; kept named because the write site is coupled to renderMode
  //   semantics (see below).
  void setRenderMode(uint8_t v) {
    c->mem_w8(obj + 0x01, v);
  }

  // type (obj+3, u8): per-object TYPE index used as key into per-type data tables. In
  //   beh_typed_table_seed_gate this indexes the halfword TBL_A6E4 (per-type seed for posY).
  uint8_t type() const {
    return c->mem_r8(obj + 0x03);
  }

  // ── Handler-local counters/echoes (u8) ───────────────────────────────────────────────────────────
  // counterA (obj+0x29, u8): "trigger" flag read by FUN_801337E4's sub-state machine: when non-zero,
  //   advance the sub-state directly (case 1 → case 2 via bne, case 4 → case 1 via bne). Zero-cleared
  //   at init by beh_typed_table_seed_gate; a writer (still un-RE'd) sets it when a scene event fires.
  uint8_t counterA() const {
    return c->mem_r8(obj + 0x29);
  }
  void setCounterA(uint8_t v) {
    c->mem_w8(obj + 0x29, v);
  }
  // sceneMode (obj+0x2A, u8): per-handler scene-mode tag. beh_typed_table_seed_gate seeds this to 0x22
  //   at init, and its state-1 gate branches on the global scene-phase byte 0x800E7EAA == 0x22. That
  //   coupling names it: "this actor participates when the scene is in the 0x22 phase".
  void setSceneMode(uint8_t v) {
    c->mem_w8(obj + 0x2A, v);
  }
  // triggerSub (obj+5, u8): SCENE-TRIGGER sub-state machine index for the sibling handler
  //   beh_typed_init_scene_trigger (FUN_80073CD8). Dispatched via jumptable at 0x80016BE8 through
  //   values 0..6: 0 = confirm-pending (interactState == 3 gate), 1/5 = pick-scene-id + FUN_8007E110 into
  //   sceneHandle, 2 = pad-edge trigger, 3 = release sceneHandle + go idle, 4 = enter-active, 6 =
  //   FUN_80042728 completion. Separate axis from the oscillator subState (obj+6).
  uint8_t triggerSub() const {
    return c->mem_r8(obj + 0x05);
  }
  void setTriggerSub(uint8_t v) {
    c->mem_w8(obj + 0x05, v);
  }

  // subState (obj+6, u8): SUB-state machine index for FUN_801337E4's 5-way dispatch (jumptable at
  //   0x80109E58). 0 = INIT (clear oscPhase, advance to 1), 1 = MAIN OSCILLATOR TICK (Trig::rcos of
  //   oscPhase + PRNG-jittered accumulate), 2/3/4 = TURN/SCAN sub-states that consult a pilot-actor
  //   region at 0x800E7E80 and set targetDelta. Semantics named from FUN_801337E4 RE (2026-07-03).
  uint8_t subState() const {
    return c->mem_r8(obj + 0x06);
  }
  void setSubState(uint8_t v) {
    c->mem_w8(obj + 0x06, v);
  }
  // interactState (obj+0x2B, u8): MULTI-PURPOSE byte set from OUTSIDE the handlers (writer still
  //   un-RE'd), consumed by whichever handler owns this actor slot:
  //     - FUN_801337E4 case [1]/[4] (background actor tick): when non-zero, dispatch FUN_80077768
  //       (turn-direction lookup) and set targetDelta = ±256, then clear the interaction state.
  //     - beh_typed_init_scene_trigger state 1 case 0: gate on the specific value 3 as a
  //       "confirm-pending" signal, then advance triggerSub and dispatch FUN_80040B48 per-type.
  //     - beh_scene_ui_trigger state 1 case 0 (save signs / scene UI): the SAME value-3 confirm gate,
  //       advancing triggerSub and firing the area-mask trigger. Confirmed 2026-07-21 by tracing the
  //       pad edge (0x2000) through to the flag reaching 3.
  //   State-0 inits and various tails zero-clear it.
  //
  //   *** WHEN YOU MAY CLEAR IT MATTERS. *** The consuming handler READS this flag inside its
  //   sub-state switch; the guest only zeroes it in the TAIL, after that read. Clearing it earlier —
  //   e.g. at the top of state 1, next to the cull check — makes the ==3 test unreachable, the
  //   sub-state never advances, and the actor latches forever. That was the save-sign softlock
  //   (kanban #5, docs/findings/scene.md): one clear, one line too early, no crash, no diverging
  //   byte — just a correct write at the wrong time. Clear it in the render tail, nowhere else.
  // NAMED PROPERLY 2026-07-21 (game/player/interact_scan.cpp, RE of guest FUN_80024794). This byte is
  // the object's INTERACTION STATE, not a generic "sub flag":
  //     kInteractNone (0)      not a candidate
  //     kInteractInRange (1)   this object is offering an interaction — set by the object's OWN
  //                            handler, NOT by one central proximity pass (97 distinct writers of the
  //                            literal 1 across the area overlays; each object type decides for itself)
  //     kInteractActivated (3) player just acted on it — set by the interaction scanner, consumed by
  //                            this object's own handler, cleared in that handler's render tail
  // The scanner also publishes WHICH activation happened at 0x800BF840 (0x84 sign/plaque family,
  // 0x85 otherwise).
  enum : uint8_t { kInteractNone = 0, kInteractInRange = 1, kInteractActivated = 3 };
  uint8_t interactState() const {
    return c->mem_r8(obj + 0x2B);
  }
  void setInteractState(uint8_t v) {
    c->mem_w8(obj + 0x2B, v);
  }
  bool interactActivated() const {
    return interactState() == kInteractActivated;
  }

  // counterB (obj+0x46, u8): retry-delay counter for FUN_801337E4's oscillator-tick gate. In sub-state 1
  //   the tick only fires when counterB == 0; otherwise counterB is decremented and the tick is
  //   skipped this frame. Zero-cleared at init by beh_typed_table_seed_gate.
  uint8_t retryDelay() const {
    return c->mem_r8(obj + 0x46);
  }
  void setRetryDelay(uint8_t v) {
    c->mem_w8(obj + 0x46, v);
  }
  // (old counterB setter kept for beh_typed_table_seed_gate's init reset — same field as retryDelay.)
  void setCounterB(uint8_t v) {
    c->mem_w8(obj + 0x46, v);
  }

  // targetDelta (obj+0x44, i16): signed 16-bit "target-angle delta" set by FUN_801337E4's turn/scan
  //   sub-states — case 1 turn-setup path writes ±256 based on the direction lookup return; case 3 seeds
  //   from a small per-pilot-mode table (32/48/64/128); case 4 wraps it into ±0x1000 and clamps.
  //   zero-cleared by the section-A reset (stateEcho != 0 path) at the top of FUN_801337E4.
  int16_t targetDelta() const {
    return (int16_t)c->mem_r16(obj + 0x44);
  }
  void setTargetDelta(int16_t v) {
    c->mem_w16(obj + 0x44, (uint16_t)v);
  }

  // renderRec (obj+0xC0, u32): guest pointer to the object's RENDER RECORD (allocated by
  //   GraphicsBind::recordAlloc in the parent handler's INIT state). FUN_801337E4 writes
  //   renderRec[+0xC] with cos(oscPhase) >> 5 in the oscillator-tick body — the display attribute
  //   that reads as this actor's per-frame animated parameter. Not yet promoted to a typed struct;
  //   raw pointer for now.
  uint32_t renderRec() const {
    return c->mem_r32(obj + 0xC0);
  }

  // oscPhase (obj+0x42, i16 / u16 both used): SBS TARGET-#4 accumulator — the angle-space fixed-point
  //   phase that feeds the case [1] Trig::rcos oscillator in FUN_801337E4 (accumulates +68 + rng>>8
  //   each active tick; the PRNG at FUN_8009A450 seeds off 0x80105EE8 is the almost-certain
  //   divergence source per docs/findings/sbs.md target-#4 upstream probe). beh_typed_table_seed_gate
  //   zero-clears this at init (state 0).
  int16_t oscPhase() const {
    return (int16_t)c->mem_r16(obj + 0x42);
  }
  uint16_t oscPhase_u() const {
    return c->mem_r16(obj + 0x42);
  }
  void setOscPhase(int16_t v) {
    c->mem_w16(obj + 0x42, (uint16_t)v);
  }
  void setOscPhase(uint16_t v) {
    c->mem_w16(obj + 0x42, v);
  }

  // ── World position (u16 signed as needed; halfword stride 4) ────────────────────────────────────
  // posX / posY / posZ (obj+0x2E/0x32/0x36): the object's world position, read as HALFWORDS at
  //   stride 4 (skipping padding at 0x2C/0x30/0x34). Confirmed from the CULL WRAPPER FUN_8007778C
  //   (subtracts camera scratchpad 0x1F8000D2/D6/DA from these three) and from the scene walker
  //   (game/render/scene_build.cpp reads them as `P[3]` before the model->view compose).
  //
  //   NOTE: FUN_801337E4's section-A reset and per-type seed (osc_base_table) write posY (obj+0x32),
  //   which had earlier been mis-named `oscBase`. Its FUN_801337E4 role is a "background-actor Y
  //   baseline" — the case [2] pilot-consult adjusts it by ±(pilotState-2)*6 and clamps against
  //   TBL_A6F4[type], which is world-Y clamping (per-type Y range).
  int16_t posX() const {
    return (int16_t)c->mem_r16(obj + 0x2E);
  }
  int16_t posY() const {
    return (int16_t)c->mem_r16(obj + 0x32);
  }
  int16_t posZ() const {
    return (int16_t)c->mem_r16(obj + 0x36);
  }
  // ── Sub-pixel position + velocity (RE'd 2026-07-21, docs/findings/object.md) ────────────────
  // Position is 16.16 FIXED POINT. The u32 at 0x2C/0x30/0x34 is the whole value; its HIGH halfword
  // IS posX/posY/posZ (0x2E/0x32/0x36) by little-endian aliasing, and the low halfword is the
  // fraction. That is why motion handlers integrate with a 32-bit read-modify-write at 0x30 and then
  // read position as a 16-bit value at 0x32 — same field, two views.
  //   evidence: release_trigger_motion.cpp integrates  pos32 += vel16 * 0x100  then  vel16 += accel16
  //   on all three axes (0x2C/0x48, 0x30/0x4A/0x50, 0x34/0x4C); beh_variant_actor_sm.cpp writes
  //   0x26DE0000 to +0x2C — integer X in the high half, zero fraction in the low half.
  uint32_t posXFixed() const {
    return c->mem_r32(obj + 0x2C);
  }
  uint32_t posYFixed() const {
    return c->mem_r32(obj + 0x30);
  }
  uint32_t posZFixed() const {
    return c->mem_r32(obj + 0x34);
  }
  void setPosXFixed(uint32_t v) {
    c->mem_w32(obj + 0x2C, v);
  }
  void setPosYFixed(uint32_t v) {
    c->mem_w32(obj + 0x30, v);
  }
  void setPosZFixed(uint32_t v) {
    c->mem_w32(obj + 0x34, v);
  }
  // Per-axis velocity (i16, in 1/256 world units per frame — hence the *0x100 when integrated into
  // the 16.16 position) and the Y acceleration the arc/hover motions add to velY each frame.
  int16_t velX() const {
    return c->mem_r16s(obj + 0x48);
  }
  int16_t velY() const {
    return c->mem_r16s(obj + 0x4A);
  }
  int16_t velZ() const {
    return c->mem_r16s(obj + 0x4C);
  }
  void setVelX(uint16_t v) {
    c->mem_w16(obj + 0x48, v);
  }
  void setVelY(uint16_t v) {
    c->mem_w16(obj + 0x4A, v);
  }
  void setVelZ(uint16_t v) {
    c->mem_w16(obj + 0x4C, v);
  }
  int16_t accelY() const {
    return c->mem_r16s(obj + 0x50);
  }
  void setAccelY(uint16_t v) {
    c->mem_w16(obj + 0x50, v);
  }

  uint16_t posX_u() const {
    return c->mem_r16(obj + 0x2E);
  }
  uint16_t posY_u() const {
    return c->mem_r16(obj + 0x32);
  }
  uint16_t posZ_u() const {
    return c->mem_r16(obj + 0x36);
  }
  void setPosX(uint16_t v) {
    c->mem_w16(obj + 0x2E, v);
  }
  void setPosY(uint16_t v) {
    c->mem_w16(obj + 0x32, v);
  }
  void setPosZ(uint16_t v) {
    c->mem_w16(obj + 0x36, v);
  }
  // ── Bounds-cull check (was FUN_8007778C thin wrapper) ────────────────────────────────────────────
  // FULL NATIVE CHAIN as of this arc: FUN_8007778C's delta-math + flag reset happens here inline,
  // then dispatches the 5-way cull body via eng(c).cull.performBaseCull (game/render/cull.cpp —
  // FUN_8007712C reimplemented byte-exact, was previously the file-scope `cull_native_body`, now the
  // public entry). Result is the visibility flag returned by the cull body in c->r[2] (1 = visible,
  // 0 = culled) — same value the guest guest instruction path would return.
  uint32_t boundsCull() {
    int16_t dx = (int16_t)(posX_u() - c->mem_r16(0x1F8000D2u));
    int16_t dy = (int16_t)(posY_u() - c->mem_r16(0x1F8000D6u));
    int16_t dz = (int16_t)(posZ_u() - c->mem_r16(0x1F8000DAu));
    c->mem_w32(0x1F800080u, 0);
    c->mem_w32(0x1F800084u, 0);
    c->r[4] = obj;
    c->r[5] = (uint32_t)(int32_t)dx;
    c->r[6] = (uint32_t)(int32_t)dy;
    c->r[7] = (uint32_t)(int32_t)dz;
    eng(c).cull.performBaseCull(); // FUN_8007712C body — native (was typed runtime address dispatch)
    return c->r[2];
  }
  // boundsCullYOffset — the Y-offset variant of boundsCull (was FUN_800778E4). Identical to
  // boundsCull but the object's posY is shifted by `yOffset` BEFORE the delta subtract, so this
  // asks "is (posX, posY + yOffset, posZ) visible?". Used by beh_typed_table_seed_gate's spatial
  // trigger check — the parent passes triggerParam (-200) to test whether the ground below the
  // actor is inside the cull volume. Same performBaseCull dispatch, same return semantics.
  uint32_t boundsCullYOffset(int16_t yOffset) {
    int16_t dx = (int16_t)(posX_u() - c->mem_r16(0x1F8000D2u));
    int16_t dy = (int16_t)((uint16_t)(posY_u() + (uint16_t)yOffset) - c->mem_r16(0x1F8000D6u));
    int16_t dz = (int16_t)(posZ_u() - c->mem_r16(0x1F8000DAu));
    c->mem_w32(0x1F800080u, 0);
    c->mem_w32(0x1F800084u, 0);
    c->r[4] = obj;
    c->r[5] = (uint32_t)(int32_t)dx;
    c->r[6] = (uint32_t)(int32_t)dy;
    c->r[7] = (uint32_t)(int32_t)dz;
    eng(c).cull.performBaseCull(); // FUN_8007712C body — native (was typed runtime address dispatch(0x800778E4))
    return c->r[2];
  }

  // ── World transform (EULER angles + a scene-entity handle) ───────────────────────────────────────
  // rotX / rotY / rotZ (obj+0x54/0x56/0x58, u16 signed as needed): the object's Euler rotation used
  //   by the scene walker (game/render/scene_build.cpp reads these as `euler_to_R(ax,ay,az)`).
  //   rotY (obj+0x56) is also passed as SIGNED-halfword arg into FUN_801337E4's subFlag turn-lookup
  //   sub-behavior (state_one_tick's run_turn_setup — the "which direction is the pilot from us"
  //   dispatch). beh_typed_init_scene_trigger's state-0 init zero-clears rotX/rotZ and writes rotY
  //   per-type from a small set of magic angle presets (0x400/0xC00/0x4D0/…).
  int16_t rotX() const {
    return (int16_t)c->mem_r16(obj + 0x54);
  }
  int16_t rotY() const {
    return (int16_t)c->mem_r16(obj + 0x56);
  }
  // Unsigned width-views of the SAME fields (no new semantics). Faithful bodies read some of these
  // unsigned (`mem_r16`) and some signed (`mem_r16s`); the port must keep whichever the guest used,
  // so the lens offers both rather than forcing a cast at the call site and risking a sign change.
  uint16_t rotX_u() const {
    return c->mem_r16(obj + 0x54);
  }
  uint16_t rotY_u() const {
    return c->mem_r16(obj + 0x56);
  }
  uint16_t rotZ_u() const {
    return c->mem_r16(obj + 0x58);
  }
  int16_t rotZ() const {
    return (int16_t)c->mem_r16(obj + 0x58);
  }
  void setRotX(uint16_t v) {
    c->mem_w16(obj + 0x54, v);
  }
  void setRotY(uint16_t v) {
    c->mem_w16(obj + 0x56, v);
  }
  void setRotZ(uint16_t v) {
    c->mem_w16(obj + 0x58, v);
  }

  // sceneHandle (obj+0x14, u32): SCENE ENTITY handle returned by FUN_8007E110 in
  //   beh_typed_init_scene_trigger's triggerSub == 1/5 branch. Read at case 3 as a foreign object
  //   pointer whose obj[+4] state is written to 2 to RELEASE the linked scene entity when this
  //   handler goes idle. Zero-cleared on release and on the state-0 init.
  uint32_t sceneHandle() const {
    return c->mem_r32(obj + 0x14);
  }
  void setSceneHandle(uint32_t v) {
    c->mem_w32(obj + 0x14, v);
  }

  // ── Trigger box / range params ───────────────────────────────────────────────────────────────────
  // triggerParam (obj+0x60, i16): the signed per-object Y-offset passed into Actor::boundsCullYOffset
  //   (was FUN_800778E4). Adds to posY BEFORE the cull delta subtract, so it asks "is (posX,
  //   posY+triggerParam, posZ) visible?". beh_typed_table_seed_gate seeds it to -200 world units —
  //   likely a "ground beneath the actor" probe (test whether the ground the actor stands on would
  //   render this frame).
  int16_t triggerParam() const {
    return (int16_t)c->mem_r16(obj + 0x60);
  }
  uint16_t triggerParam_u() const {
    return c->mem_r16(obj + 0x60);
  }
  void setTriggerParam(int16_t v) {
    c->mem_w16(obj + 0x60, (uint16_t)v);
  }
  // stateEcho (obj+0x62, u16 / i16): mirror of the state byte written by beh_typed_table_seed_gate
  //   state-1 non-triggered gate paths. Its READER is FUN_801337E4's section A — when stateEcho is
  //   NON-ZERO on entry, the sub-state machine resets (clear subState, targetDelta, renderRec+0xC,
  //   and re-seed oscBase from the per-type table). So the semantic is: "parent latched a
  //   not-in-scene tick; sub-behavior on next call must reset".
  int16_t stateEcho() const {
    return (int16_t)c->mem_r16(obj + 0x62);
  }
  uint16_t stateEcho_u() const {
    return c->mem_r16(obj + 0x62);
  }
  void setStateEcho(uint16_t v) {
    c->mem_w16(obj + 0x62, v);
  }

  // ── Bounding box / trigger volume (u16 × 4) ──────────────────────────────────────────────────────
  // Seeded at init by beh_typed_table_seed_gate to (30, 60, 50, 100). Reads as a bounding-box or
  // radius-tuple parameterizing a trigger volume; the exact interpretation (AABB half-extents vs
  // XYZ+radius) will be pinned down when the state-1 sub-behavior that CONSUMES it is RE'd.
  void setBoxX(uint16_t v) {
    c->mem_w16(obj + 0x80, v);
  }
  void setBoxY(uint16_t v) {
    c->mem_w16(obj + 0x82, v);
  }
  void setBoxZ(uint16_t v) {
    c->mem_w16(obj + 0x84, v);
  }
  void setBoxW(uint16_t v) {
    c->mem_w16(obj + 0x86, v);
  }

private:
  Core *c;
  uint32_t obj;
};

// Global SCENE-PHASE gate byte. Read every frame by beh_typed_table_seed_gate (and other cull-record
// handlers) to gate their state-1 behavior. Semantics (advanced by what? represents what phase?) not
// yet RE'd — named for the observation that beh_typed_table_seed_gate has sceneMode=0x22 (obj+0x2A)
// and dispatches on scene_phase()==0x22 in its state-1 body.
inline uint8_t scene_phase(Core *c) {
  return c->mem_r8(0x800E7EAAu);
}

// Per-type OSCILLATION BASE table: 8014A6E4[type] -> u16 seeded into Actor::setOscBase() at init by
// beh_typed_table_seed_gate. Named for its state-machine role, not its content — content is per-type
// tuning data.
inline uint16_t osc_base_table(Core *c, uint8_t type) {
  return c->mem_r16(0x8014A6E4u + (uint32_t)type * 2u);
}
