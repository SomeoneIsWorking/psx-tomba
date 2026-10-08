// game/camera/cutscene_camera.cpp — the field/follow camera's per-frame driver, mode dispatch,
// follow pipeline, view-matrix builder and shake tail. See cutscene_camera.h for the state model and
// game/camera/camera_look_builder.h for where the camera decides what it looks at. The arithmetic is
// the reverse-engineered engine behaviour (docs/engine_re.md, docs/findings/camera.md).
#include "camera/cutscene_camera.h"

#include "camera/camera_guest_math.h"
#include "camera/camera_look_builder.h"
#include "camera/camera_mode.h"
#include "cfg.h"
#include "core/entry/game_ctx.h"
#include "core/overrides/native_override_catalog.h" // tomba::native::declareOverride — the one native-override registry
#include "game.h"                                   // c->game->verify — the shared A/B verify scaffold (camverify)
#include "guest_abi.h"                              // GuestFrame — the guest stack frame contract, spelled once
#include "guest_call.h"
#include "mtx.h"
#include "scene/script_globals.h" // the status byte the shore floor branches on
#include "trig.h"

#include <cstdint>
#include <stdio.h>
#include <string.h>

namespace tomba::camera {

bool CutsceneCamera::followAxis(
    uint32_t accAddr, uint32_t tgt32Addr, uint16_t tgtInt, uint16_t curInt, int16_t maxStep) {
  int16_t delta = (int16_t)(tgtInt - curInt); // low-16 sign-extended
  if ((uint16_t)(delta + 10) < 21) {          // |delta| <= 10 -> snap to the target
    w32(accAddr, r32(tgt32Addr));
    return true;
  }
  int32_t step = guestmath::clampStep(delta, maxStep) << 13;
  w32(accAddr, r32(accAddr) + (uint32_t)step);
  return false;
}

// ── follow accumulators ──────────────────────────────────────────────────────────────────────────
bool CutsceneCamera::trackXZ(uint32_t target) { // FUN_8006D960
  // (The dev teleport used to be consumed here. It is Engine::devTeleportApply's now — this method
  // only runs in the follow-camera mode, so consuming it here made `tp` a no-op in every area whose
  // camera is in another mode. See engine.h's mCamTpPending banner for the measurement.)
  bool snapX = followAxis(kCamScratch + 0x0C, target + 0, r16(target + 2), r16(kCamScratch + 0x0E), 6144);
  bool snapZ = followAxis(kCamScratch + 0x14, target + 8, r16(target + 10), r16(kCamScratch + 0x16), 6144);
  return snapX && snapZ;
}

bool CutsceneCamera::trackY(uint32_t target) { // FUN_8006DA54
  return followAxis(kCamScratch + 0x10, target + 4, r16(target + 6), r16(kCamScratch + 0x12), 5632);
}

// ── angleStep ────────────────────────────────────────────────────────────────────────────────────
void CutsceneCamera::angleStep() { // FUN_8006E010
  int32_t cur = (int32_t)camR32(0x34);
  if (r8(kCamGlobal + 0x164) != 3) { // not mode 3: drive cam[+0x34] toward 0 by ±8
    if (cur <= 0) {
      int32_t r = cur + 8;
      camW32(0x34, (uint32_t)r);
      if (r >= 0) {
        camW32(0x34, 0);
      }
    } else {
      int32_t r = cur - 8;
      camW32(0x34, (uint32_t)r);
      if (r <= 0) {
        camW32(0x34, 0);
      }
    }
    return;
  }
  uint8_t idx = r8(kCamGlobal + 0x168);
  int32_t target;
  switch (idx >= 5 ? 4 : idx) {
  case 0:
  case 1:
    target = 0;
    break;
  case 2:
    target = -128;
    break;
  case 3:
    target = -256;
    break;
  default:
    target = -384;
    break;
  }
  if (target < cur) {
    int32_t r = cur - 8;
    camW32(0x34, (uint32_t)r);
    if (r < target) {
      camW32(0x34, (uint32_t)target);
    }
  } else {
    int32_t r = cur + 8;
    camW32(0x34, (uint32_t)r);
    if (target < r) {
      camW32(0x34, (uint32_t)target);
    }
  }
}

void CutsceneCamera::yFloor() { // FUN_8006C80C
  // The render mode selects a FLOOR the camera height is not allowed below. Five of the thirteen
  // render modes have one and eight have none; the thresholds are per mode, not global, and two of
  // them are conditional on a second byte. They are spelled as named constants here because the
  // numbers are the whole content of this function and a reader could not otherwise tell a floor from
  // a comparison bound: note that several of them DIFFER by one from the value they are compared
  // against, and the difference is the guest's own (a `>=` against a `>` bound), not a typo.
  const uint8_t renderMode = r8(tomba::camera::kRenderModeByte);
  const uint32_t index = static_cast<uint32_t>(static_cast<uint8_t>(renderMode - 1u));
  if (index > tomba::camera::kHighestFloorRenderModeIndex) {
    return; // render modes 1 and 14 and above have no floor
  }
  const uint32_t kCameraHeight = 0x1F8000E2u;
  const int32_t height = static_cast<int16_t>(r16(kCameraHeight));

  // Clamp the camera height to `floor`, but only when it is already strictly below it. The `!(y <
  // bound)` shape of the guest's own tests is preserved: it is a "clamp unless already at or above
  // the bound" test, which is NOT the same as "clamp when below" for a height exactly on the bound.
  auto clampTo = [&](int32_t floorValue) {
    if (height < floorValue) {
      w16(kCameraHeight, static_cast<uint16_t>(static_cast<int16_t>(floorValue)));
    }
  };
  // The same clamp, written the guest's other way round: raise to `floor` unless already strictly
  // below the lower bound. Used where the floor is only reachable from a distance.
  auto raiseUnlessBelow = [&](int32_t lowerBound, int32_t floorValue) {
    if (!(height < lowerBound)) {
      w16(kCameraHeight, static_cast<uint16_t>(static_cast<int16_t>(floorValue)));
    }
  };

  switch (index) {
  case tomba::camera::kFloorRenderModeIndex: // render mode 1
    clampTo(tomba::camera::kFloorRenderMode1);
    break;
  case tomba::camera::kFloorSeaRenderModeIndex: // render mode 4: two floors, chosen by a second byte
    if (r8(0x800BF871u) == tomba::camera::kSeaSubAreaSeven) {
      if (static_cast<int16_t>(r16(0x800E7EB6u)) < tomba::camera::kSeaDistanceBound) {
        if (height < tomba::camera::kSeaFloorLow) {
          raiseUnlessBelow(tomba::camera::kSeaFloorLowPlusOne, tomba::camera::kSeaFloorNear);
        } else {
          w16(kCameraHeight, static_cast<uint16_t>(static_cast<int16_t>(tomba::camera::kSeaFloorLow)));
        }
      }
    } else {
      raiseUnlessBelow(tomba::camera::kSeaFloorHighBound, tomba::camera::kSeaFloorHigh);
    }
    break;
  case tomba::camera::kFloorShoreRenderModeIndex: // render mode 6: two floors, chosen by the status byte
    if (r8(tomba::scene::script_globals::kStatusByteMirror) == tomba::camera::kShoreSubAreaFourteen) {
      clampTo(tomba::camera::kShoreFloorNear);
    } else {
      clampTo(tomba::camera::kShoreFloorFar);
    }
    break;
  case tomba::camera::kFloorNightRenderModeIndex: // render mode 10
    clampTo(tomba::camera::kFloorRenderMode10);
    break;
  case tomba::camera::kFloorFinalRenderModeIndex: // render mode 13
    raiseUnlessBelow(tomba::camera::kFloorRenderMode13Bound, tomba::camera::kFloorRenderMode13);
    break;
  default:
    break; // the other eight render modes impose no floor
  }
}

// ── pitch (vertical-look height smoother) ────────────────────────────────────────────────────────
void CutsceneCamera::pitch() { // FUN_8006D654
  int32_t g30 = (int32_t)r32(kCamGlobal + 0x30);
  int16_t g17e = (int16_t)r16(kCamGlobal + 0x17e);
  int sign = (g17e & 0x8000) != 0;

  int32_t r5 = g30;
  int viaC8 = 0;
  uint8_t idx = r8(kCamGlobal + 0x164);
  if (idx >= 13) {
    idx = 7;
  }
  switch (idx) {
  case 2: {
    uint32_t p = r32(kCamGlobal + 0x10);
    r5 = (int32_t)(r32(p + 0x30) << 16);
    viaC8 = 1;
    break;
  }
  case 3: {
    uint32_t p = r32(kCamGlobal + 0x10);
    r5 = (int32_t)(r32(p + 0x30) << 16);
    break;
  }
  case 12: {
    int32_t a = (int16_t)r16(0x1F800202u);
    int32_t sum = (a << 16) + g30;
    r5 = (int32_t)((sum + (int32_t)((uint32_t)sum >> 31)) >> 1);
    viaC8 = 1;
    break;
  }
  case 7:
  case 8:
    r5 = g30;
    viaC8 = 1;
    break;
  default: {
    uint8_t m = r8(0x800BF821u);
    uint8_t gm = r8(kCamGlobal + 0x145);
    if (m == 1) {
      r5 = g30 - (200 << 16);
    } else if (m != 0) {
      if (gm == 2) {
        r5 = g30 + (sign ? -(240 << 16) : (40 << 16));
      } else {
        r5 = g30 + (sign ? -(310 << 16) : -(240 << 16));
      }
    } else {
      if (gm == 2) {
        r5 = sign ? g30 : (g30 + (200 << 16));
      } else {
        r5 = sign ? (g30 - (70 << 16)) : g30;
      }
    }
    break;
  }
  }
  if (viaC8) {
    r5 += (200 << 16);
  }
  int32_t target = r5 - (600 << 16);

  int32_t delta = target - (int32_t)camR32(0x0c);
  int32_t r7 = (g17e < 0) ? (20 << 16) : (140 << 16);
  int32_t r4 = (g17e < 0) ? (580 << 16) : (460 << 16);

  int dir;
  int32_t r3;
  int32_t t = delta + r4;
  if (t >= 0) {
    dir = 0;
    r3 = t;
  } else {
    int32_t t2 = t + r7;
    if (t2 < 0) {
      dir = 1;
      r3 = t2;
    } else {
      camW32(0x18, 0);
      return;
    }
  }

  if (dir == 0) {
    if (!(0x80000 < r3)) {
      camW32(0x0c, (uint32_t)(r5 + r4 - (600 << 16)));
      return;
    }
    int32_t cv = (int32_t)camR32(0x18);
    if (!(r3 < cv)) {
      if (cv < 0) {
        camW32(0x18, 0);
      }
      camW32(0x18, (uint32_t)((int32_t)camR32(0x18) + (16 << 16)));
    } else if (0x4FFFFF < r3) {
      camW32(0x18, (uint32_t)(80 << 16));
    } else {
      camW32(0x18, (uint32_t)r3);
    }
  } else {
    uint8_t g145 = r8(kCamGlobal + 0x145);
    if (g145 != 0) {
      if (!(r3 < -(256 << 16))) {
        return;
      }
      r3 += (256 << 16);
      if (-(96 << 16) < r3) {
        camW32(0x18, (uint32_t)r3);
      } else {
        camW32(0x18, (uint32_t)-(96 << 16));
      }
    } else {
      if (!(r3 < -(22 << 16))) {
        camW32(0x18, (uint32_t)r3);
      } else {
        int32_t cv = (int32_t)camR32(0x18);
        if (!(cv < r3)) {
          if (cv > 0) {
            camW32(0x18, 0);
          }
          camW32(0x18, (uint32_t)((int32_t)camR32(0x18) + (int32_t)0xFFFEA000u));
        } else {
          camW32(0x18, (uint32_t)-(22 << 16));
        }
      }
    }
  }
  camW32(0x0c, (uint32_t)((int32_t)camR32(0x0c) + (int32_t)camR32(0x18)));
}

// ── heading (heading tracker) ────────────────────────────────────────────────────────────────────
void CutsceneCamera::heading() { // FUN_8006DCF4
  if (camR8(0x74) & 4) {
    camW8(0x66, camR8(0x66) | 2);
    return;
  }

  int32_t off = 0;
  uint8_t g61 = r8(kCamGlobal + 0x61);
  int useTable;
  if (g61 == 0) {
    useTable = 1;
  } else if ((g61 & 1) == 0) {
    off = 768;
    useTable = 0;
  } else {
    useTable = 1;
  }

  if (useTable) {
    uint8_t idx = r8(kCamGlobal + 0x164);
    if (idx >= 13) {
      return;
    }
    switch (idx) {
    case 0:
    case 4: {
      if (r8(kCamGlobal + 0x145) == 0) {
        off = 320;
        break;
      }
      int32_t g4a_s = (int16_t)r16(kCamGlobal + 0x4a);
      uint32_t r3v = (uint16_t)r16(kCamGlobal + 0x4a);
      if (g4a_s < 0) {
        r3v = (uint32_t)(0 - r3v);
      }
      r3v += (r8(kCamGlobal + 0x165) == 0) ? (uint32_t)-14976 : (uint32_t)-16384;
      int32_t s = (int32_t)((uint32_t)r3v << 16) >> 21;
      off = 320 - s;
      break;
    }
    case 1:
    case 11: {
      uint32_t sub = r32(kCamGlobal + 0x158);
      off = (r8(sub + 0xc) == 4 && r8(sub + 0x2) != 0) ? 1100 : 700;
      break;
    }
    case 9:
      off = -320;
      break;
    case 3:
      off = 700;
      break;
    case 12: {
      uint32_t diff = (uint32_t)(uint16_t)r16(kCamGlobal + 0x32) - (uint32_t)(uint16_t)r16(0x1F800202u);
      off = ((int16_t)diff < 501) ? (int32_t)diff : 500;
      break;
    }
    default:
      return;
    }
  }

  int32_t s06 = (uint16_t)r16(kCamScratch + 6);
  int32_t c26 = (uint16_t)camR16(0x26);
  int32_t s12 = (uint16_t)r16(kCamScratch + 0x12);
  int32_t r5 = s06 + off - c26 - s12;
  if ((uint16_t)(r5 + 10) < 21) {
    w16(kCamScratch + 6, (uint16_t)(c26 + (s12 - off)));
    camW8(0x66, camR8(0x66) | 2);
  } else {
    int32_t step = (int32_t)((uint32_t)r5 << 16) >> 3;
    w32(kCamScratch + 4, (uint32_t)((int32_t)r32(kCamScratch + 4) - step));
  }

  uint8_t f = camR8(0x74);
  int cond = 0, active = 1;
  if (f & 2) {
    cond = ((int16_t)r16(kCamScratch + 6) < (int16_t)camR16(0x4a));
  } else if (f & 8) {
    cond = ((int16_t)camR16(0x4a) < (int16_t)r16(kCamScratch + 6));
  } else {
    active = 0;
  }
  if (active && !cond) {
    w16(kCamScratch + 6, (uint16_t)camR16(0x4a));
    camW8(0x66, camR8(0x66) | 2);
  }
}

// ── lookAt (camera basis / view-matrix builder) ──────────────────────────────────────────────────
static inline int32_t cam_idiv(Core *c, int32_t num, int32_t den) {
  cpu_div(c, (uint32_t)num, (uint32_t)den);
  return (int32_t)c->lo;
}

void CutsceneCamera::lookAt() { // FUN_8006D02C
  // guest 0x8006D02C pushes a 56-byte guest frame (r29-=56, spills s0..s7/fp/ra at +16..+52, restored
  // symmetrically at the single exit — abi_extract 0x8006D02C). Mirrored relative to the CALLER's live
  // c->r[29] with the LIVE register values, so the guest-stack bytes byte-match the substrate wherever
  // this native runs on an SBS-compared leg (the snapToMasterOffsetY200/orbitTick override trees, whose
  // callers preload r16/r17/r31 with the gen's s0/s1/jal-site values before calling in). Every callee of
  // this body (isqrt 0x80077FB0, MR_init 0x80051794, ratan2 0x80085690, MulMatrix0 0x80084250,
  // applyMatrixLV 0x80084470, CopyMatrix 0x800847B0) is frameless per abi_extract, so the own frame is
  // the whole stack footprint.
  static constexpr GuestFrameSpill kSpills[] = {
      {16, 16}, {17, 20}, {18, 24}, {19, 28}, {20, 32}, {21, 36}, {22, 40}, {23, 44}, {30, 48}, {31, 52}};
  GuestFrame<56, 10> frame(c, kSpills);
  int32_t dX = (int16_t)r16(kCamScratch + 14) - (int16_t)r16(kCamScratch + 2);
  int32_t dZ = (int16_t)r16(kCamScratch + 22) - (int16_t)r16(kCamScratch + 10);
  int32_t dY = (int16_t)r16(kCamScratch + 18) - (int16_t)r16(kCamScratch + 6);
  int32_t xz = dX * dX + dZ * dZ;
  int32_t s18 = guestmath::call(*c, guestmath::kLookAtIsqrt, xz + dY * dY) & 0xffff;
  int32_t s19 = guestmath::call(*c, guestmath::kLookAtIsqrt, xz) & 0xffff;
  camW32(0x5c, (uint32_t)s18);
  camW32(0x60, (uint32_t)s19);

  if (s18 != 0) {
    mtxOf(c).identity(kCamScratch + 40);
    int32_t sinp = cam_idiv(c, dY << 12, s18);
    int32_t cosp = cam_idiv(c, s19 << 12, s18);
    int32_t pitch = (int16_t)trigOf(c).ratan2(sinp, cosp);
    w16(kCamScratch + 32, (uint16_t)pitch);
    w16(kCamScratch + 48, (uint16_t)cosp);
    w16(kCamScratch + 56, (uint16_t)cosp);
    w16(kCamScratch + 50, (uint16_t)(-sinp));
    w16(kCamScratch + 54, (uint16_t)sinp);

    if (s19 != 0) {
      int32_t a = cam_idiv(c, (-dX) << 12, s19);
      int32_t b = cam_idiv(c, dZ << 12, s19);
      int32_t yaw = (int16_t)trigOf(c).ratan2(a, b);
      w16(kCamScratch + 34, (uint16_t)yaw);
      mtxOf(c).identity(0x1F800000u);
      w16(0x1F800000u, (uint16_t)b);
      w16(0x1F800004u, (uint16_t)a);
      w16(0x1F800010u, (uint16_t)b);
      w16(0x1F80000Cu, (uint16_t)(-a));
      guestmath::call(*c, guestmath::kMulMatrix0, (int32_t)(kCamScratch + 40), 0x1F800000);
    }
  }

  uint32_t M = kCamScratch + 40;
  uint16_t r104 = r16(kCamScratch + 52);
  uint16_t r106 = r16(kCamScratch + 54);
  uint16_t r108 = r16(kCamScratch + 56);
  w16(kCamScratch + 24, r104);
  w16(kCamScratch + 26, r106);
  w16(kCamScratch + 28, r108);
  w16(0x1F8000C0u, (uint16_t)(0u - (uint32_t)r16(kCamScratch + 2)));
  w16(0x1F8000C2u, (uint16_t)(0u - (uint32_t)r16(kCamScratch + 6)));
  w16(0x1F8000C4u, (uint16_t)(0u - (uint32_t)r16(kCamScratch + 10)));
  mathOf(c).applyMatrixLV((uint32_t)M, 0x1F8000C0u, 0x1F80010Cu); // FUN_80084470 (native)
  guestmath::call(*c, guestmath::kCopyMatrix, (int32_t)M, (int32_t)(kCamScratch + 72));
}

// ── orchestrators (per-frame camera modes) ───────────────────────────────────────────────────────
void CutsceneCamera::snapAccXZ(uint32_t target) { // FUN_8006D934
  w32(kCamScratch + 0x0C, r32(target + 0));       // snap X accumulator
  w32(kCamScratch + 0x14, r32(target + 8));       // snap Z accumulator
}

void CutsceneCamera::snapAccY(uint32_t target) { // FUN_8006D950
  w32(kCamScratch + 0x10, r32(target + 4));      // snap Y accumulator
}

void CutsceneCamera::snapFollow(uint32_t target) { // FUN_8006E3B0
  // guest 0x8006E3B0 pushes a 32-byte guest frame (r29-=32, spills s0@+16/s1@+20/ra@+24), then sets
  // s0=a0(cam) / s1=a1(target) for the body — abi_extract 0x8006E3B0. Mirrored with live values so the
  // stack bytes byte-match on the SBS-compared leg (reached via the orbitTick override, which preloads
  // r16/r17/r31 with the gen's values at its 0x8006EFE0 jal site). snapAccXZ/snapAccY (0x8006D934/
  // 0x8006D950) are frameless; lookAt mirrors its own 56-byte frame and spills the r16/r17/r31 set here.
  static constexpr GuestFrameSpill kSpills[] = {{16, 16}, {17, 20}, {31, 24}};
  GuestFrame<32, 3> frame(c, kSpills);
  c->r[16] = cam_;
  c->r[17] = target;

  snapAccXZ(target); // snap the follow accumulators to target (no smoothing)
  snapAccY(target);
  c->r[31] = 0x8006E3E0u; // gen jal-site for lookAt (spilled by lookAt's frame)
  lookAt();
}

void CutsceneCamera::snapFollowA(uint32_t target) { // FUN_8006E294 (driver mode 2 + init post-check)
  snapAccXZ(target);
  snapAccY(target);
  if (camR8(0x76) == 0) {
    look_.posBuildA();
    look_.headBuildA(1);
  } // scripted look-build A
  lookAt();
}

void CutsceneCamera::pitchFollow(uint32_t target) { // FUN_8006E360 (driver mode 3)
  pitch();
  snapAccXZ(target);
  snapAccY(target);
  lookAt();
}

void CutsceneCamera::snapFollowB(uint32_t target) { // FUN_8006E2FC (driver mode 4)
  snapAccXZ(target);
  snapAccY(target);
  if (camR8(0x76) == 0) {
    look_.posBuildB();
    look_.headBuildB();
  } // scripted look-build B
  lookAt();
}

void CutsceneCamera::mainFollow() { // FUN_8006E0F0
  look_.distSolve();
  trackXZ(cam_ + 8);
  pitch();
  trackY(cam_ + 8);
  yFloor();
  if (camR8(0x76) == 0 && r8(kCamGlobal + 0x17a) == 0) {
    heading();
  }
  lookAt();
  if (camR8(0x77) == 0) {
    angleStep();
  }
  int32_t acc = (int32_t)camR32(0x28) + (int32_t)camR32(0x34);
  w32(kCamScratch + 0x44, r32(kCamScratch + 0x44) - (uint32_t)acc);
}

void CutsceneCamera::simpleFollow(uint32_t target) { // FUN_8006E3F4
  if (trackXZ(target)) {
    camW8(0x66, camR8(0x66) | 1);
  }
  if (trackY(target)) {
    camW8(0x66, camR8(0x66) | 2);
  }
  lookAt();
}

void CutsceneCamera::trackFollow(uint32_t target) { // FUN_8006E228
  trackXZ(target);
  trackY(target);
  camW16(0x0e, r16(0x1F8000E2u));
  if (camR8(0x76) == 0) {
    look_.posBuildB();
    look_.headBuildB();
  } // scripted look-build B (see snapFollowB — same pattern)
  lookAt();
}

// ── post-mode TAIL — the camera SHAKE state machine ────────────────────────────────────────────────
// Runs every frame after every mode. The state byte is cam[0x76], written by EXTERNAL code and read
// here; the states themselves are named in camera_mode.h, and the two families are:
//
//   three-axis free-running — capture the look position once, then jitter X/Y/Z around it on every
//     frame until external code asks to stop, at which point the anchor is restored EXACTLY and the
//     camera goes idle.
//   height-only — the same shape, three variants: one free-running, and two ONE-SHOT pulses that fire
//     in the frame they are set and always end at idle. The pulse's begin states fall straight into
//     their jitter arm in the same frame; that is the guest's own control flow, not a bug, which is
//     why the two are separate states rather than one state with a flag.
//
// The three jitter arms are NOT the same arm with different parameters, and are deliberately not
// written as one: the free-running arm moves all THREE axes, drawing a 5-bit jitter for X and Z and a
// 4-bit one for Y (Y jitters half as far), while the height arms move Y ONLY. Unifying them would
// make the height shakes move the camera sideways, which is a different camera.
//
// A one-shot pulse ABORTS without jittering when the busy byte is set, rather than queueing itself
// for later. That is a drop, not a deferral, and it is the guest's behaviour.
void CutsceneCamera::shakeTail() { // FUN_8006C988
  using State = tomba::camera::CameraShakeState;
  using namespace tomba::camera;
  const uint8_t state = camR8(0x76);
  if (state > kHighestShakeState) {
    return; // not a shake state: external code wrote something this tail does not interpret
  }
  // One draw from the shared RNG, masked to `bits` and taken as the jitter offset.
  const auto drawJitter = [&](uint32_t bits) {
    return static_cast<int32_t>(rngOf(c).next() & bits);
  };
  // The two height-only jitters, which differ only in amplitude, mask and effect id. These two ARE
  // the same arm, so they are one arm — unlike the axis arm above, which moves three coordinates.
  const auto jitterHeightOnly = [&](int32_t amplitude, uint32_t mask, uint32_t effectId) {
    w16(kCamScratch + 0x06,
        static_cast<uint16_t>(static_cast<int32_t>(camR16(kShakeAnchorY)) - amplitude + drawJitter(mask)));
    guestmath::call(
        *c, guestmath::kShakeFx, 0, 0, static_cast<int32_t>(effectId), static_cast<int32_t>(kShakeEffectPriority));
  };
  switch (static_cast<State>(state)) {
  case State::kCaptureAxes:
    camW16(kShakeAnchorX, r16(kCamScratch + 0x02));
    camW16(kShakeAnchorY, r16(kCamScratch + 0x06));
    camW8(0x76, static_cast<uint8_t>(State::kJitterAxes));
    camW16(kShakeAnchorZ, r16(kCamScratch + 0x0a));
    break;
  case State::kJitterAxes: {
    // Three draws, in the guest's order: X, then Z, then Y. Y takes half the amplitude and half the
    // mask of the other two, which is why it is not simply a third call to the same expression.
    const int32_t jitterX = drawJitter(kShakeMaskNarrow);
    w16(kCamScratch + 0x02,
        static_cast<uint16_t>(static_cast<int32_t>(camR16(kShakeAnchorX)) - kShakeXAmplitude + jitterX));
    const int32_t jitterZ = drawJitter(kShakeMaskNarrow);
    w16(kCamScratch + 0x0a,
        static_cast<uint16_t>(static_cast<int32_t>(camR16(kShakeAnchorZ)) - kShakeXAmplitude + jitterZ));
    const int32_t jitterY = drawJitter(kShakeMaskNarrow >> 1);
    w16(kCamScratch + 0x06,
        static_cast<uint16_t>(static_cast<int32_t>(camR16(kShakeAnchorY)) - kShakeYAmplitude + jitterY));
    guestmath::call(*c,
                    guestmath::kShakeFx,
                    0,
                    0,
                    static_cast<int32_t>(kShakeEffectAxes),
                    static_cast<int32_t>(kShakeEffectPriority));
    break;
  }
  case State::kRestoreAxes:
    w16(kCamScratch + 0x02, camR16(kShakeAnchorX));
    w16(kCamScratch + 0x06, camR16(kShakeAnchorY));
    w16(kCamScratch + 0x0a, camR16(kShakeAnchorZ));
    camW8(0x76, static_cast<uint8_t>(State::kIdle));
    break;
  case State::kCaptureHeight:
    camW16(kShakeAnchorY, r16(kCamScratch + 0x06));
    camW8(0x76, static_cast<uint8_t>(State::kJitterHeight));
    break;
  case State::kJitterHeight:
    jitterHeightOnly(kShakeHeightAmplitude, kShakeMaskWide, kShakeEffectHeight);
    break;
  case State::kPulseHeightBegin:
    camW16(kShakeAnchorY, r16(kCamScratch + 0x06));
    camW8(0x76, static_cast<uint8_t>(State::kPulseHeight));
    [[fallthrough]];
  case State::kPulseHeight:
    if (camR8(kShakeBusyByte) != 0) {
      camW8(0x76, static_cast<uint8_t>(State::kIdle));
      break;
    }
    jitterHeightOnly(kShakeHeightAmplitude, kShakeMaskWide, kShakeEffectAxes);
    camW8(0x76, static_cast<uint8_t>(State::kIdle));
    break;
  case State::kPulseHeightSmallBegin:
    camW16(kShakeAnchorY, r16(kCamScratch + 0x06));
    camW8(0x76, static_cast<uint8_t>(State::kPulseHeightSmall));
    [[fallthrough]];
  case State::kPulseHeightSmall:
    if (camR8(kShakeBusyByte) != 0) {
      camW8(0x76, static_cast<uint8_t>(State::kIdle));
      break;
    }
    jitterHeightOnly(kShakeHeightSmallAmplitude, kShakeMaskNarrow, kShakeEffectAxes);
    camW8(0x76, static_cast<uint8_t>(State::kIdle));
    break;
  case State::kIdle:
    break;
  }
}

// ── the MODE dispatch ────────────────────────────────────────────────────────────────────────────
// Eighteen modes, one policy: `tomba/camera/camera_mode.h` names them all and says what each one
// follows, and the table below is reached through it rather than through a second 18-arm switch.
// The three render modes with a dedicated overlay prologue are looked up rather than tested, and the
// two dispatchers below (native and guest-faithful) are the only two places the table is read.
namespace {
using tomba::camera::CameraMode;
using tomba::camera::FollowTarget;
using tomba::camera::RenderModePrologue;

// The render-mode-keyed prologue for one of the two modes that have one, or nullptr when this render
// mode has no dedicated overlay and the mode's own follow runs instead.
const RenderModePrologue *prologueFor(uint8_t renderMode) {
  for (const RenderModePrologue &row : tomba::camera::kRenderModePrologues) {
    if (row.renderMode == renderMode) {
      return &row;
    }
  }
  return nullptr;
}
} // namespace

void CutsceneCamera::dispatchMode(uint8_t mode) {
  switch (static_cast<CameraMode>(mode)) {
  case CameraMode::kMainFollow: {
    const uint8_t rm = r8(tomba::camera::kRenderModeByte);
    if (const RenderModePrologue *prologue = prologueFor(rm)) {
      sub(prologue->forMainFollow);
      break;
    }
    if (!(camR8(0x64) & 0x80)) {
      mainFollow();
      look_.rotBuild();
    }
    // Then whichever handler the resident render-mode table holds for this render mode.
    sub(r32(tomba::camera::kRenderModeFunctionTable + static_cast<uint32_t>(rm) * 4u));
    break;
  }
  case CameraMode::kTrackFollow: {
    const uint8_t rm = r8(tomba::camera::kRenderModeByte);
    if (const RenderModePrologue *prologue = prologueFor(rm)) {
      sub(prologue->forTrackFollow);
      break;
    }
    trackFollow(cam_ + 0x38);
    break;
  }
  case CameraMode::kSnapFollowScriptedA:
    snapFollowA(cam_ + 0x38);
    break;
  case CameraMode::kPitchFollow:
    pitchFollow(cam_ + 0x38);
    break;
  case CameraMode::kSnapFollowScriptedB:
    snapFollowB(cam_ + 0x38);
    break;
  case CameraMode::kSnapFollowMaster:
    snapFollow(kCamGlobal + 0x2c);
    break; // snap to the master position
  case CameraMode::kFreezeAtMasterHeight:
    camW8(0x64, 0);
    camW32(0x0c, r32(kCamGlobal + 0x30));
    break; // cam[+0x0c] = the master's Y
  case CameraMode::kSnapFollowSelf:
  case CameraMode::kSnapFollowSelfAlias:
    snapFollow(cam_ + 0x38);
    break;
  case CameraMode::kSimpleFollowSelf:
    simpleFollow(cam_ + 0x38);
    break;
  case CameraMode::kFieldOverlay9:
    sub(tomba::camera::guestEntryFor(CameraMode::kFieldOverlay9));
    break;
  case CameraMode::kAreaOverlayScripted:
    sub(tomba::camera::guestEntryFor(CameraMode::kAreaOverlayScripted));
    break;
  case CameraMode::kReset:
  case CameraMode::kForceModeByte:
    camW8(0x64, 0);
    camW8(2, 0);
    camW8(3, 0);
    break; // reset the mode byte and the two sub-state bytes
  case CameraMode::kForceModeSix:
    camW8(0x64, tomba::camera::kForceModeSixValue);
    break;
  case CameraMode::kSimpleFollowMaster:
    simpleFollow(kCamGlobal + 0x2c);
    break;
  case CameraMode::kTailOnly:
    break; // no body; the driver still runs its post-mode tail
  case CameraMode::kFieldOverlay17:
    sub(tomba::camera::guestEntryFor(CameraMode::kFieldOverlay17));
    break;
  }
}

void CutsceneCamera::initPlace() { // FUN_8006E918 (init: place the camera X/Z base from the heading)
  // guest 0x8006E918 pushes a 40-byte guest frame (r29-=40, spills s0@+16/s1@+20/s2@+24/s3@+28/ra@+32,
  // restored at the single exit — abi_extract 0x8006E918). Mirrored with live values so the stack bytes
  // byte-match on the SBS-compared leg (reached via the snapToMasterOffsetY200 override, which preloads
  // r16/r17/r31 with the gen's values at its 0x8006EA60 jal site). rcos (0x80083F50) is frameless; rsin
  // is NOT (24-byte frame, ra@+16 — the reason Trig left it unregistered, trig.cpp), so the gen's rsin
  // call is reproduced through the substrate with its jal-site ra (0x8006E9C0) preloaded.
  static constexpr GuestFrameSpill kSpills[] = {{16, 16}, {17, 20}, {18, 24}, {19, 28}, {31, 32}};
  GuestFrame<40, 5> frame(c, kSpills);
  int32_t g140 = (int16_t)r16(kCamGlobal + 0x140);
  uint16_t cam56 = camR16(0x56);
  // s0: cam[0x56], negated unless the scene heading kCamGlobal+0x140 already equals the target heading kCamGlobal+0x56.
  int32_t s0 = (g140 == (int16_t)r16(kCamGlobal + 0x56)) ? (int16_t)cam56 : (int16_t)(uint16_t)(0u - (uint32_t)cam56);
  int32_t s1 = (int16_t)(uint16_t)(0u - (uint32_t)r16(0x1F8000EEu)); // -(radius) as int16
  int32_t angle = g140 + (int16_t)r16(cam_ + 0x52) + s0;
  int32_t cx = (int32_t)(uint16_t)r16(kCamGlobal + 0x2e) + (guestmath::mulLo(trigOf(c).rcos(angle), s1) >> 12);
  w16(kCamScratch + 0x02, (uint16_t)cx);
  c->r[31] = 0x8006E9C0u; // gen jal-site for rsin (spilled by rsin's own 24-byte frame)
  int32_t cz = (int32_t)(uint16_t)r16(kCamGlobal + 0x36) -
               (guestmath::mulLo(guestmath::call(*c, guestmath::kRsinSubstrate, angle), s1) >> 12);
  w16(kCamScratch + 0x0a, (uint16_t)cz);
}

void CutsceneCamera::initSeedGrp(uint32_t src) { // FUN_8006CBA8 (writes the FIXED driver cam @0x800E8008)
  w16(kCameraObject + 0x3a, r16(src + 2));
  w16(kCameraObject + 0x3e, r16(src + 6));
  w16(kCameraObject + 0x42, r16(src + 10));
}

// ── Wiring pass (2026-07-08 frontier follow-up) ─────────────────────────────────────────────────
// Ghidra decomp: scratch/decomp_local/region_8006.c (import "main_ram", va 0x80060000-0x80070000) +
// guest 0x8006E8F8 (authenticated executable/overlay evidence, Ghidra missed this one — read the guest body directly,
// which is instruction-exact ground truth per CLAUDE.md).
//
// REAL BUG found wiring this one (was drafted assuming camera-only semantics): the guest-visible behavior reads
// its object base from a0 (c->r[4]), NOT a hardcoded kCameraObject constant like pushMode/restoreMode/
// snapToMasterOffsetY200/orbitTick below. Confirmed by its cross-module callers (authenticated executable/overlay
// evidence ov_a00_shard_0.c..ov_a0k_shard_0.c etc.) — every one of them calls FUN_8006E8F8 with a0 = that overlay's OWN
// actor-object pointer (e.g. `c->r[16]`, an A00-area actor base), not the camera object at 0x800E8008. So this leaf is
// a generic "reset follow accumulator" applied to whatever object shares this field shape (0x24/0x28/0x56) — the camera
// is just ONE caller (of many). The method itself is unaffected (it already takes its base from the instance's `cam_`,
// which reads as "cam" only by naming convention); the fix lives entirely in the override-registry wiring below, which
// constructs the instance from the LIVE a0 rather than hardcoding kCameraObject.
void CutsceneCamera::resetFollowAccum() { // FUN_8006E8F8
  camW32(0x24, 0);
  camW32(0x28, 0);
  w16(kCamScratch + 0x1e, (uint16_t)(int16_t)-1750);
  camW16(0x56, 256);
}

void CutsceneCamera::pushMode(uint8_t mode) { // FUN_8006E1C0
  camW8(0x67, camR8(0x64));                   // stash the current mode
  camW8(0x64, mode);
  camW8(4, 0);
  camW8(5, 0);
  camW8(6, 0);
}

void CutsceneCamera::restoreMode() { // FUN_8006E1E4
  if (r8(kCamGlobal + 2) == 1) {
    camW8(0x64, 0);
    camW32(0x0c, r32(kMasterY));
    return;
  }
  camW32(0x0c, r32(kMasterY));
  camW8(0x64, camR8(0x67)); // restore the mode pushMode() stashed
}

// FUN_8006EA00 pushes a real 32-byte guest frame (r29-=32, s0/s1/ra spilled at +16/+20/+24,
// restored symmetrically before every return) — confirmed against authenticated executable/overlay evidence
// guest 0x8006EA00. Since this leaf is wired GLOBALLY (any typed runtime address dispatch(c, 0x8006EA00u) caller,
// substrate context included, per the "MIRROR THE GUEST STACK" directive), the frame is mirrored
// relative to the CALLER's live c->r[29] (not a fixed offset): spill the live s0/s1/ra so their
// bytes land in guest RAM exactly where the substrate would leave them. The gen then loads
// s0=kCameraObject / s1=kCameraObject+8 for the body — those register values MUST be reproduced here, because
// the frame-pushing callees (initPlace frame 40, lookAt frame 56 — mirrored in their own bodies)
// spill s0/s1 into THEIR frames; likewise each frame-pushing callee gets r31 preloaded with the
// gen's exact jal-site constant so its ra spill slot byte-matches. Restore before returning (a
// nested call, e.g. lookAt's guestmath::kLookAtIsqrt typed runtime address dispatch, can clobber the shared Core::r[]
// register file, so the restore is a real requirement, not a formality).
void CutsceneCamera::snapToMasterOffsetY200() { // FUN_8006EA00
  static constexpr GuestFrameSpill kSpills[] = {{16, 16}, {17, 20}, {31, 24}};
  GuestFrame<32, 3> frame(c, kSpills);
  c->r[16] = kCameraObject;     // gen: s0 = 0x800E8008 (spilled by initPlace's/lookAt's frames)
  c->r[17] = kCameraObject + 8; // gen: s1 = 0x800E8010 (ditto)
  eng(c).frameCut().noteCameraPlaced();
  // cam[8]/[0xc]/[0x10] are a 32-bit (X,Y,Z) staging triple (same shape trackXZ/trackY/snapAccXZ/snapAccY
  // read as `target`); only the HIGH (integer) half is written here — the low half keeps whatever was
  // already there, exactly like the guest (never "cleaned up").
  w16(kCameraObject + 0x0e, (uint16_t)((int16_t)r16(kMasterY + 2) - 200)); // cam[0xc].hi = kMasterY.hi - 200
  w16(kCameraObject + 0x0a, r16(kMasterX + 2));                            // cam[8].hi   = kMasterX.hi
  w16(kCameraObject + 0x12, r16(kMasterZ + 2));                            // cam[0x10].hi= kMasterZ.hi
  snapAccXZ(kCameraObject + 8);                                            // 0x8006D934 — frameless, no ra needed
  snapAccY(kCameraObject + 8);                                             // 0x8006D950 — frameless
  c->r[31] = 0x8006EA60u; // gen jal-site for initPlace (spilled by initPlace's frame) (spilled by initPlace's frame)
  initPlace();
  c->r[31] = 0x8006EA68u; // gen jal-site for lookAt (spilled by lookAt's frame)
  lookAt();
}

// FUN_8006EF38 pushes the same shape of 32-byte frame (r29-=32, s0/s1/ra spilled at +16/+20/+24)
// UNCONDITIONALLY — even on the early-return path (the gen's branch-delay-slot spill runs before
// the {3,4}-window check, and the early-return target is the same restore tail as the normal
// exit) — confirmed against authenticated executable/overlay evidence guest 0x8006EF38. Mirrored the same way as
// snapToMasterOffsetY200 above (own frame + gen s0/s1 register values + per-jal-site ra constants
// for the frame-pushing callees rsin/snapFollow/lookAt); see that method's comment for the rationale.
void CutsceneCamera::orbitTick() { // FUN_8006EF38
  static constexpr GuestFrameSpill kSpills[] = {{16, 16}, {17, 20}, {31, 24}};
  GuestFrame<32, 3> frame(c, kSpills);
  if ((uint8_t)(r8(0x1F800236u) - 3) < 2) { // only during render-timing window {3,4}
    int32_t angle = (int16_t)camR16(0x70);
    int32_t rc = trigOf(c).rcos(angle); // 0x80083F50 — frameless, native ok
    c->r[31] = 0x8006EF90u;             // gen jal-site for rsin (spilled by rsin's 24-byte frame)
    int32_t rs =
        guestmath::call(*c, guestmath::kRsinSubstrate, angle); // substrate rsin — its gen frame must land on the stack
    w16(kCamScratch + 0x02, (uint16_t)((int16_t)camR16(0x3a) + (int16_t)(guestmath::mulLo(rc, 500) >> 12)));
    w16(kCamScratch + 0x0a, (uint16_t)((int16_t)camR16(0x42) + (int16_t)(guestmath::mulLo(rs, 500) >> 12)));
    camW16(0x70, (uint16_t)(angle + 8));
    // gen: s0 = rcos*500>>12 (kept live across the body), s1 = kCameraObject — spilled by snapFollow's frame.
    c->r[16] = (uint32_t)(guestmath::mulLo(rc, 500) >> 12);
    c->r[17] = kCameraObject;
    c->r[31] = 0x8006EFE0u;           // gen jal-site for snapFollow (spilled by its frame)
    snapFollow(kCameraObject + 0x38); // snap the camera's own position accumulators to the fixed orbit center
  }
}

void CutsceneCamera::update() { // FUN_8006EC44 (resident per-frame camera driver; cam obj @0x800E8008)
  uint8_t outer = camR8(0);     // cam[0] = outer state
  if (outer == 0) {
    camW8(0, 1);
    init();
    return;
  } // first frame: init (no post-mode tail)
  if (outer != 1) {
    return; // idle
  }
  uint8_t ss = camR8(1); // cam[1] = sub-state
  if (ss == 0) {
    camW8(1, 1);
    camW8(2, 0);
    camW8(3, 0);
  } // fall through into the run state
  else if (ss != 1) {
    return;
  }
  uint8_t mode = (uint8_t)(camR8(0x64) & 0x3f);
  camW8(0x66, 0);
  if (mode < 18) {
    dispatchMode(mode);
  }
  shakeTail(); // post-mode tail (always, after any mode)
}

// pc_faithful mirror of guest 0x8006EC44 (authenticated executable/overlay evidence). Guest frame: r29-=24,
// s0(r16)@sp+16 spilled with the CALLER's live value (Engine::fieldFrameFaithful leaves r16=0x1F800000
// there before calling), ra(r31)@sp+20 spilled with the caller's jal-site (0x80108B90u), s0 reassigned to
// kCameraObject (hardcoded in the gen, independent of any cam_ this instance was constructed with) for the body,
// both restored and the frame deallocated on every exit path (including the two early-return/idle paths,
// which skip shakeTail exactly like the gen's direct goto to the restore tail). Every callee gets r31 set
// to the exact gen jal-site constant first, matching the reference-mirror style (Engine::fieldFrameFaithful
// / Sop::fieldModeFaithful). init/mainFollow/rotBuild/trackFollow/snapFollow*/pitchFollow/simpleFollow/
// shakeTail are dispatched via typed runtime address dispatch(c, addr) to their guest address — since no
// override-registry entry exists for any of them, this falls straight through to the substrate gen_func body (same code
// the oracle runs), NOT a call to the native sibling *methods* on this class (those exist for the native_sync path
// only). Calling convention for the two-arg follow leaves (trackFollow/snapFollowA/pitchFollow/
// snapFollowB/snapFollow/simpleFollow) is a0(r4)=cam, a1(r5)=cam+56 (or kCamGlobal+0x2C for the "snap-to-master"
// variants of snapFollow/simpleFollow) — verified against the gen bodies AND the real mode-dispatch jump
// table at 0x80016A44 (18 uint32 entries, read from scratch/bin/tomba2/MAIN.EXE @ file offset 0x7244).
void CutsceneCamera::updateFaithful() { // FUN_8006EC44
  uint8_t outer = camR8(0);
  static constexpr GuestFrameSpill kSpills[] = {{16, 16}, {31, 20}};
  GuestFrame<24, 2> frame(c, kSpills);
  c->r[16] = kCameraObject; // s0 = kCameraObject (0x800E8008, hardcoded in the gen)

  if (outer == 0) {
    c->mem_w8(c->r[16] + 0, 1);
    c->r[31] = 0x8006EC84u;
    c->r[4] = c->r[16];
    psx::cpu::dispatchGuestToReturn0(*c,
                                     0x8006EA7Cu,
                                     psx::cpu::ExecutionBudget::currentTurn(*c),
                                     __func__); // init FUN_8006EA7C(cam) — substrate until its faithful conversion
    goto epilogue;
  }
  if (outer != 1) {
    goto epilogue;
  }
  {
    uint8_t ss = c->mem_r8(c->r[16] + 1);
    if (ss == 0) {
      c->mem_w8(c->r[16] + 1, 1);
      c->mem_w8(c->r[16] + 2, 0);
      c->mem_w8(c->r[16] + 3, 0);
    } else if (ss != 1) {
      goto epilogue;
    }
    uint8_t mode = (uint8_t)(c->mem_r8(c->r[16] + 100) & 63u);
    c->mem_w8(c->r[16] + 102, 0);
    if (mode < 18) {
      dispatchModeFaithful(mode);
    }
    c->r[31] = 0x8006EF28u;
    c->r[4] = c->r[16];
    psx::cpu::dispatchGuestToReturn0(
        *c, 0x8006C988u, psx::cpu::ExecutionBudget::currentTurn(*c), __func__); // shakeTail FUN_8006C988(cam)
  }
epilogue:; // GuestFrame's destructor restores r16/ra and ascends sp on every path
}

// The guest-faithful mirror of the mode dispatch: the SAME eighteen modes and the SAME policy, read
// from the same table, reached through guest addresses instead of native calls. What differs from
// `dispatchMode` above is only that each callee is dispatched with the guest's own return constant
// armed in `ra` first, so the frame bytes a downstream still-guest leaf spills are the ones the
// reference wrote. That per-arm constant is the ONLY thing this function owns; the policy — which
// mode does what, and which render modes get the overlay prologue — is `camera_mode.h`'s.
void CutsceneCamera::dispatchModeFaithful(uint8_t mode) {
  // The guest's own call shape for this table: a0 = the camera object, and for the follow modes
  // a1 = the follow target. `kFollowsCameraSelf` and `kFollowsMaster` differ only in which address
  // goes in a1, so the descriptor decides that rather than each arm repeating it. The camera object
  // is read from c->r[16] — the frame `updateFaithful` set — and NOT from this instance's `cam_`,
  // because the guest's mode dispatch passes the object the driver was handed and the two are not
  // necessarily the same base.
  const tomba::camera::ModeDescriptor *const descriptor = tomba::camera::descriptorFor(static_cast<CameraMode>(mode));
  const uint32_t followTarget = (descriptor != nullptr && descriptor->follows == FollowTarget::kFollowsMaster)
                                    ? kCamGlobal + 0x2cu
                                    : c->r[16] + 56u;

  switch (static_cast<CameraMode>(mode)) {
  case CameraMode::kMainFollow: {
    const uint8_t rm = c->mem_r8(tomba::camera::kRenderModeByte);
    if (const RenderModePrologue *prologue = prologueFor(rm)) {
      subFaithfulAtReturn(prologue->forMainFollow, prologue->mainFollowReturn);
      return;
    }
    if (!(c->mem_r8(c->r[16] + 100) & 0x80)) {
      subFaithfulAtReturn(0x8006E0F0u, 0x8006ED7Cu); // mainFollow(cam)
      subFaithfulAtReturn(0x8006E464u, 0x8006ED84u); // rotBuild(cam)
    }
    // The render mode is re-read here, as the guest does: the follow above may have changed it.
    const uint8_t rmAfter = c->mem_r8(tomba::camera::kRenderModeByte);
    subFaithfulAtReturn(c->mem_r32(tomba::camera::kRenderModeFunctionTable + static_cast<uint32_t>(rmAfter) * 4u),
                        0x8006EDACu);
    return;
  }
  case CameraMode::kTrackFollow: {
    const uint8_t rm = c->mem_r8(tomba::camera::kRenderModeByte);
    if (const RenderModePrologue *prologue = prologueFor(rm)) {
      subFaithfulAtReturn(prologue->forTrackFollow, prologue->trackFollowReturn);
      return;
    }
    subFaithfulAtReturn(0x8006E228u, 0x8006EE2Cu, followTarget); // trackFollow
    return;
  }
  case CameraMode::kSnapFollowScriptedA:
    subFaithfulAtReturn(0x8006E294u, 0x8006EE40u, followTarget); // snapFollowA
    return;
  case CameraMode::kPitchFollow:
    subFaithfulAtReturn(0x8006E360u, 0x8006EE54u, followTarget); // pitchFollow
    return;
  case CameraMode::kSnapFollowScriptedB:
    subFaithfulAtReturn(0x8006E2FCu, 0x8006EE68u, followTarget); // snapFollowB
    return;
  case CameraMode::kSnapFollowMaster:
    subFaithfulAtReturn(0x8006E3B0u, 0x8006EE80u, followTarget); // snapFollow
    return;
  case CameraMode::kFreezeAtMasterHeight:
    c->mem_w8(c->r[16] + 100, 0);
    c->mem_w32(c->r[16] + 12, c->mem_r32(kCamGlobal + 0x30));
    return;
  case CameraMode::kSnapFollowSelf:
  case CameraMode::kSnapFollowSelfAlias:
    subFaithfulAtReturn(0x8006E3B0u, 0x8006EEF8u, followTarget); // snapFollow
    return;
  case CameraMode::kSimpleFollowSelf:
    subFaithfulAtReturn(0x8006E3F4u, 0x8006EEA8u, followTarget); // simpleFollow
    return;
  case CameraMode::kFieldOverlay9:
    subFaithfulAtReturn(tomba::camera::guestEntryFor(CameraMode::kFieldOverlay9), 0x8006EEB8u);
    return;
  case CameraMode::kAreaOverlayScripted:
    subFaithfulAtReturn(tomba::camera::guestEntryFor(CameraMode::kAreaOverlayScripted), 0x8006EEC8u);
    return;
  case CameraMode::kReset:
  case CameraMode::kForceModeByte:
    c->mem_w8(c->r[16] + 100, 0);
    c->mem_w8(c->r[16] + 2, 0);
    c->mem_w8(c->r[16] + 3, 0);
    return;
  case CameraMode::kForceModeSix:
    c->mem_w8(c->r[16] + 100, tomba::camera::kForceModeSixValue);
    return;
  case CameraMode::kSimpleFollowMaster:
    subFaithfulAtReturn(0x8006E3F4u, 0x8006EF10u, followTarget); // simpleFollow
    return;
  case CameraMode::kTailOnly:
    // No body. The driver's post-mode tail still runs, exactly as the guest's fallthrough does.
    return;
  case CameraMode::kFieldOverlay17:
    subFaithfulAtReturn(tomba::camera::guestEntryFor(CameraMode::kFieldOverlay17), 0x8006EF20u);
    return;
  }
}

void CutsceneCamera::init() { // FUN_8006EA7C (first-frame field reset + render-mode-keyed mode selector)
  camW16(0x56, 256);
  camW8(0x72, 0);
  camW8(0x76, 0);
  camW8(0x77, 0);
  camW8(0x74, 0);
  camW16(6, 0);
  camW32(0x0c, r32(0x1F8000E0u));
  camW16(0x52, 1024);
  camW16(0x22, 240);
  camW16(0x8c, 0);

  uint8_t rm = r8(0x800BF870u);
  bool mainPath = false; // whether we take the mainFollow(+E918) branch after selecting the mode
  // Render-mode jump table @0x800169EC: rm 0 -> special; {2,7,20} -> mode 0-cleared; 3 -> mode 14;
  // {16..19} -> mode 128; everything else (incl. rm>=21) -> default (mode 0-cleared + mainFollow path).
  if (rm == 0) {
    if (r8(0x800BF89Cu) == 2) { // pre-scripted camera fixup
      camW8(0x64, 7);
      camW16(0x3a, 2746);
      camW16(0x3e, (uint16_t)(int16_t)-800);
      camW16(0x42, 3808);
      w16(0x1F8000D2u, 3387);
      w16(0x1F8000D6u, (uint16_t)(int16_t)-2691);
      w16(0x1F8000DAu, 3506);
    } else if (r8(0x800BF816u) != 0) {
      /* skip mode-clear + mainFollow, go straight to the post-check */
    } else {
      camW8(0x64, 0);
      mainPath = true;
    }
  } else if (rm == 2 || rm == 7 || rm == 20) {
    camW8(0x64, 0);
  } else if (rm == 3) {
    camW8(0x64, 14);
  } else if (rm >= 16 && rm <= 19) {
    camW8(0x64, 128);
  } else {
    camW8(0x64, 0);
    mainPath = true; // default label (incl. rm>=21)
  }

  if (mainPath) {
    mainFollow();
    initPlace();
  }

  // post-check (0x8006EBA8): when render-timing byte 0x1F800236 is 5 or 6, seed a scripted follow.
  if ((uint8_t)(r8(0x1F800236u) - 5) < 2) {
    initSeedGrp(kCamGlobal + 0x2c);
    uint16_t d3e = camR16(0x3e);
    camW16(0x3e, (uint16_t)(d3e + 1000));
    if (r8(kCamGlobal + 2) != 0) {
      return;
    }
    camW16(0x3e, (uint16_t)(d3e + 860));
    camW16(0x0e, (uint16_t)(d3e + 860));
    camW8(0x64, 15);
    camW16(0x6c, 1400);
    camW16(0x6e, 64);
    camW16(0x70, (uint16_t)(r16(kCamGlobal + 0x140) + 1024));
    snapFollowA(cam_ + 0x38);
    snapFollow(kCamGlobal + 0x2c);
  }
}

// ── Override-registry wiring (2026-07-08 frontier follow-up) ────────────────────────────────────
// resetFollowAccum/pushMode/restoreMode/snapToMasterOffsetY200/orbitTick reach the substrate
// EXCLUSIVELY via typed runtime address dispatch(c, addr) from cross-module (overlay) call sites — confirmed by
// grepping every authenticated executable/overlay evidence reference to these 5 addresses: none appear as a direct
// same-module `a direct guest-address call` call EXCEPT pushMode (0x8006E1C0), which authenticated executable/overlay
// evidence, shard_4.c and shard_6.c also call directly (MAIN calling its own resident code — the recorded guest call
// graph contains that as a plain C call, bypassing typed runtime address dispatch entirely and consulting the recorded
// binary evidence's OWN image-qualified runtime dispatcher table instead). So pushMode needs the dual-wire
// (tomba::native::declareOverride setter passed to install(), same pattern as PcScheduler's primitives in
// game/core/pc_scheduler.cpp); the other 4 only need the typed runtime address dispatch-only registration (setter
// omitted).

static void eov_resetFollowAccum(Core *c) {
  // a0 (c->r[4]) IS the target object base here — NOT hardcoded kCameraObject (see the RE note above
  // resetFollowAccum's definition). Construct the instance from the live a0.
  CutsceneCamera(c, c->r[4]).resetFollowAccum();
}
static void eov_pushMode(Core *c) {
  CutsceneCamera(c, kCameraObject).pushMode((uint8_t)c->r[4]);
}
static void eov_restoreMode(Core *c) {
  CutsceneCamera(c, kCameraObject).restoreMode();
}
static void eov_snapToMasterOffsetY200(Core *c) {
  CutsceneCamera(c, kCameraObject).snapToMasterOffsetY200();
}
static void eov_orbitTick(Core *c) {
  CutsceneCamera(c, kCameraObject).orbitTick();
}

void CutsceneCamera::registerOverrides(Game * /*game*/) {
  // pushMode (0x8006E1C0) has direct same-module callers -> tomba::native::declareOverride installs the thunk;
  // the other four are typed runtime address dispatch-only (setter omitted).
  tomba::native::declareOverride(0x8006E1C0u, "CutsceneCamera::pushMode", eov_pushMode);
  tomba::native::declareOverride(0x8006E8F8u, "CutsceneCamera::resetFollowAccum", eov_resetFollowAccum);
  tomba::native::declareOverride(0x8006E1E4u, "CutsceneCamera::restoreMode", eov_restoreMode);
  tomba::native::declareOverride(0x8006EA00u, "CutsceneCamera::snapToMasterOffsetY200", eov_snapToMasterOffsetY200);
  tomba::native::declareOverride(0x8006EF38u, "CutsceneCamera::orbitTick", eov_orbitTick);
}

} // namespace tomba::camera
