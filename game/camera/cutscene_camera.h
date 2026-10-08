// game/camera/cutscene_camera.h — class CutsceneCamera: the FIELD / FOLLOW camera of Tomba! 2,
// PC-native, expressed as PC-game STRUCTURE (named state, methods) rather than a register-convention
// transcription.
//
// This is the free-roam field camera as well as the SOP/cutscene one (docs/findings/camera.md): the
// resident chain 0x8006EC44 → snapFollow (0x8006E3B0) → lookAt (0x8006D02C) is the free-roam chain.
//
// The per-frame driver, its jump-table modes and the init/mode selector are `update()` and `init()`.
// Where the camera LOOKS is `LookAngleBuilder` (game/camera/camera_look_builder.h); this class owns
// the driver, the follow pipeline, the mode orchestrators, the view-matrix builder and the shake tail.
//
// STATE MODEL. Two blocks, both reached guest-direct behind named accessors:
//   * the per-instance CAMERA OBJECT (cam_; SOP uses 0x800E8008, which the resident driver hardcodes)
//     — position, distance, velocities, mode flags, settled bits.
//   * the GLOBAL camera/scene state block kCamGlobal — the master position (player and camera-center
//     share it in this 2.5D game), scene heading, mode selectors — and the SCRATCHPAD block
//     kCamScratch holding the follow accumulators and the composed view matrix the projection reads.
// Method params replace the guest register convention; the arithmetic is the reverse-engineered
// engine behaviour, verified per-call against the guest reference. See docs/engine_re.md.
#pragma once

#include "camera/camera_look_builder.h"
#include "camera/camera_state.h"

#include "core.h"
#include "guest_call.h"

#include <cstdint>

class Game;

namespace tomba::camera {

class CutsceneCamera : private CameraState {
public:
  // Installs the camera's RE-ahead leaves (resetFollowAccum/pushMode/restoreMode/
  // snapToMasterOffsetY200/orbitTick) in the shared override registry. See cutscene_camera.cpp.
  static void registerOverrides(Game *game);

  CutsceneCamera(Core *core, uint32_t cameraObject) : CameraState(*core, cameraObject), look_(*this) {}

  // ── Per-frame DRIVER (0x8006EC44) + init/mode selector (0x8006EA7C) ──────────────────────────────
  // update(): the resident camera driver. It reads the outer state from cam[0] (0=first-frame init,
  //   1=run, else idle), runs the sub-state machine, dispatches on the MODE byte cam[0x64]&0x3F to
  //   one of the follow orchestrators / a still-substrate leaf / a field-overlay handler, and runs
  //   the post-mode shake tail.
  void update();
  // The guest-stack-faithful mirror of 0x8006EC44: it reproduces the gen's own frame (r29-=24,
  // r16@sp+16 / r31@sp+20) and arms r31 with the exact jal-site constant before every callee, so
  // downstream still-substrate leaves see the same guest stack. See cutscene_camera.cpp.
  void updateFaithful();
  void init();                    // 0x8006EA7C — first-frame field reset and render-mode-keyed mode select.
  void initPlace();               // FUN_8006E918 — place camera X/Z base (scratch+0x02/+0x0a) from heading.
  void initSeedGrp(uint32_t src); // FUN_8006CBA8 — seed cam[0x3a/0x3e/0x42] from a source group.

  // Reverse-engineered leaves with no observed reachable call site: they read and write the same
  // cam_/scratch/global state the owned orchestrators do, so they belong on this class. Not gated by
  // a differential run yet.
  void resetFollowAccum();       // FUN_8006E8F8 — zero cam[0x24]/cam[0x28], seed scratch+0x1E, reset cam[0x56].
  void pushMode(uint8_t mode);   // FUN_8006E1C0 — save cam[0x64] to cam[0x67], set new mode, zero cam[4..6].
  void restoreMode();            // FUN_8006E1E4 — on global+2==1: mode=0 and camY=master Y; else restore cam[0x67].
  void snapToMasterOffsetY200(); // FUN_8006EA00 — hard-reset the accumulators to master X/Y-200/Z.
  void orbitTick();              // FUN_8006EF38 — during the render-timing window {3,4}, step the orbit
                                 // angle cam[0x70] and orbit the look point around cam[0x3a]/cam[0x42]
                                 // at radius 500, snapping the position to it.

  // ── Orchestrators (per-frame camera MODES). One is picked per frame by the mode selector. ────────
  void snapFollow(uint32_t target);   // FUN_8006E3B0 — SOP/cutscene: SNAP the follow accumulators to the
                                      // target with no smoothing, then build the view.
  void mainFollow();                  // FUN_8006E0F0 — the smoothing MAIN follow (dist → track → pitch → …).
  void simpleFollow(uint32_t target); // FUN_8006E3F4 — track XZ then Y (settled bits), then build the view.
  void trackFollow(uint32_t target);  // FUN_8006E228 — track plus two substrate sub-functions, then build the view.
  // The scripted-camera SNAP variants (driver modes 2/3/4): snap the accumulators to the target, add
  // one more step, then build the view.
  void snapFollowA(uint32_t target); // FUN_8006E294 (mode 2 + init post-check): snap + look-build A.
  void pitchFollow(uint32_t target); // FUN_8006E360 (mode 3): pitch, then snap, then build the view.
  void snapFollowB(uint32_t target); // FUN_8006E2FC (mode 4): snap + look-build B.

  // ── The follow pipeline's steps ────────────────────────────────────────────────────────────────
  bool trackXZ(uint32_t target);   // FUN_8006D960 — smooth camera X/Z toward the target; returns settled.
  bool trackY(uint32_t target);    // FUN_8006DA54 — smooth camera Y toward the target; returns settled.
  void snapAccXZ(uint32_t target); // FUN_8006D934 — SNAP the X/Z follow accumulators to the target.
  void snapAccY(uint32_t target);  // FUN_8006D950 — SNAP the Y follow accumulator to the target.
  void pitch();                    // FUN_8006D654 — the vertical-look height smoother.
  void yFloor();                   // FUN_8006C80C — the per-render-mode camera-Y floor clamp.
  void heading();                  // FUN_8006DCF4 — the heading tracker.
  void angleStep();                // FUN_8006E010 — the angle accumulator step.
  void lookAt();                   // FUN_8006D02C — build the camera basis/view matrix into the scratchpad.
  void shakeTail();                // FUN_8006C988 — the camera SHAKE state machine; runs after every mode.

private:
  // One follow-accumulator axis, shared by trackXZ/trackY: snap when within +/-10 of the target's
  // integer, else take a rate-limited step. Returns true iff it SNAPPED (settled) this frame.
  bool followAxis(uint32_t accAddr, uint32_t tgt32Addr, uint16_t tgtInt, uint16_t curInt, int16_t maxStep);
  // Dispatch a still-unowned resident camera leaf (or a field-overlay handler) via the substrate,
  // mirroring the driver's `jal fn`: set only a0 (= cam_), and a1 when given.
  void sub(uint32_t fn) {
    c->r[4] = cam_;
    psx::cpu::dispatchGuestToReturn0(*c, fn, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
  }
  void sub(uint32_t fn, uint32_t a1) {
    c->r[4] = cam_;
    c->r[5] = a1;
    psx::cpu::dispatchGuestToReturn0(*c, fn, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
  }
  // The same call with the guest's own return constant armed in `ra` first, which the guest-faithful
  // dispatch needs: a body that pushes a frame writes that address into guest stack bytes, so it has
  // to be the reference's value and not this core's stale return. `sub` above is this with the
  // constant left alone; both are the one guest-call convention, not two.
  void subAtReturn(uint32_t fn, uint32_t returnAddress) {
    c->r[31] = returnAddress;
    sub(fn);
  }
  void subAtReturn(uint32_t fn, uint32_t returnAddress, uint32_t a1) {
    c->r[31] = returnAddress;
    sub(fn, a1);
  }
  // The faithful arm that passes the CAMERA OBJECT the faithful driver hardcoded into r[16], not this
  // instance's `cam_`: updateFaithful's dispatch runs inside that frame, so a0 is that value.
  void subFaithfulAtReturn(uint32_t fn, uint32_t returnAddress) {
    c->r[31] = returnAddress;
    c->r[4] = c->r[16];
    psx::cpu::dispatchGuestToReturn0(*c, fn, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
  }
  void subFaithfulAtReturn(uint32_t fn, uint32_t returnAddress, uint32_t a1) {
    c->r[31] = returnAddress;
    c->r[4] = c->r[16];
    c->r[5] = a1;
    psx::cpu::dispatchGuestToReturn0(*c, fn, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
  }
  // update()'s MODE-byte (cam[0x64]&0x3F) dispatch.
  void dispatchMode(uint8_t mode);
  // The guest-stack-faithful mirror of dispatchMode, driven by updateFaithful(): the same 18-entry
  // mode table (cases 7/14 and 11/12 share a body, matching the gen's shared jump-table targets),
  // with r[31] armed before every callee. Runs only under updateFaithful's already-descended frame.
  void dispatchModeFaithful(uint8_t mode);

  LookAngleBuilder look_;

  CutsceneCamera(const CutsceneCamera &) = delete;
  CutsceneCamera &operator=(const CutsceneCamera &) = delete;
};

} // namespace tomba::camera