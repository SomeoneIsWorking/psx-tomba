// game/camera/camera_look_builder.h — class LookAngleBuilder: where the camera decides what it looks at.
//
// The follow pipeline moves the camera; this builds the LOOK POINT and the HEADING that follow
// solves for. It owns the scratchpad look accumulators (S+0x00/S+0x08), the yaw/distance fold that
// turns a look-relative offset into them, the scripted-camera angle builders the snap follow modes
// use, the distance/zoom solver, and the rotation builder the special-camera modes run.
//
// Every owner of the camera shares the same scratchpad and the same camera object, so all three
// derive from CameraState: they are VIEWS over one camera state, not owners of separate state, and
// none of them is ever used as a CameraState in its own right.
#pragma once

#include "camera/camera_state.h"

#include <cstdint>

namespace tomba::camera {

class LookAngleBuilder : private CameraState {
public:
  explicit LookAngleBuilder(const CameraState &shared) : CameraState(shared) {}

  // Fold a look-relative (dx, dz) into the heading and the S+0/S+8 accumulators; settle bit if the
  // distance is under 401. FUN_8006D02C's shared tail.
  void yawDistAccumulate(int32_t dx, int32_t dz);
  // Place a look point at (theta, radius) around the scene centre and fold it in. The shared tail of
  // rotBuild's modes.
  void lookatTail(int32_t theta, int32_t radius);
  // FUN_8006DC38 — direct place of the X/Z look accumulators (scripted mode 2).
  void posBuildA();
  // FUN_8006DAD8 — place, then yaw/distance accumulate (scripted mode 4).
  void posBuildB();
  // FUN_8006DF88 — heading step: a fixed offset when a1 is zero, else an rsin step.
  void headBuildA(uint32_t nonzero);
  // FUN_8006DEF0 — heading step with the +/-10 snap.
  void headBuildB();
  // rotBuild's shared entry into the yaw/distance fold.
  void joinE640(int32_t delta, int32_t radius);
  // rotBuild's two table-driven delta/radius sources.
  int32_t table1Delta();
  void table2(int32_t radius);
  // FUN_8006E464 — the rotation/look-at builder for the special-camera modes.
  void rotBuild();
  // FUN_8006D2AC — the distance/zoom solver and its planar placement.
  void distSolve();

private:
  LookAngleBuilder(const LookAngleBuilder &) = delete;
  LookAngleBuilder &operator=(const LookAngleBuilder &) = delete;
};

} // namespace tomba::camera