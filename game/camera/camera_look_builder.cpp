// game/camera/camera_look_builder.cpp — the camera's look-point and heading construction.
// The arithmetic is the reverse-engineered engine behaviour; see docs/engine_re.md's camera section
// and game/camera/camera_mode.h for the mode table these builders serve.
#include "camera/camera_look_builder.h"

#include "camera/camera_guest_math.h"
#include "camera/cutscene_camera.h"
#include "core/entry/game_ctx.h"
#include "guest_call.h"
#include "trig.h"

#include <cstdint>

namespace tomba::camera {

// guest 0x8006E570: the signed look delta the table-1 branches of table1Delta share.
namespace {
int32_t table1Entry(Core &core, uint32_t globalBlock, uint32_t cam) {
  int32_t g140 = core.mem_r16s(globalBlock + 0x140), g56 = core.mem_r16s(globalBlock + 0x56);
  int32_t cam56 = (int32_t)core.mem_r16(cam + 0x56);
  return (g140 == g56) ? cam56 : -cam56;
}
} // namespace

// ── rotBuild (special-camera rotation / look-at builder) ─────────────────────────────────────────
void LookAngleBuilder::yawDistAccumulate(int32_t dx, int32_t dz) {
  int32_t yaw = (int16_t)trigOf(c).ratan2(-dz, dx);
  int32_t dist = (int16_t)guestmath::call(*c, guestmath::kIsqrt, dx * dx + dz * dz);
  int32_t rc2 = trigOf(c).rcos(yaw);
  w32(kCamScratch + 0x00, r32(kCamScratch + 0x00) + ((rc2 * dist) >> 1));
  int32_t rs2 = trigOf(c).rsin(yaw);
  w32(kCamScratch + 0x08, r32(kCamScratch + 0x08) - ((rs2 * dist) >> 1));
  if (dist < 401) {
    camW8(0x66, camR8(0x66) | 1);
  }
}

void LookAngleBuilder::lookatTail(int32_t theta, int32_t radius) {
  int32_t rc = trigOf(c).rcos(theta);
  int32_t lookX = (int32_t)r16(0x1F800160u) + ((rc * radius) >> 12);
  int32_t rs = trigOf(c).rsin(theta);
  int32_t lookZ = (int32_t)r16(0x1F800164u) - ((rs * radius) >> 12);
  int32_t camZ = (int32_t)r16(kCamScratch + 0x0A);
  int32_t camX = (int32_t)r16(kCamScratch + 0x02);
  int32_t dz = (int16_t)(lookZ - camZ);
  int32_t dx = (int16_t)(lookX - camX);
  yawDistAccumulate(dx, dz);
}

// ── scripted-camera look-angle builders (0x8006DC38/DAD8/DF88/DEF0 — used by snapFollowA/B) ─────────
void LookAngleBuilder::posBuildA() { // FUN_8006DC38 — overwrite the X/Z look accumulators
  int32_t P = guestmath::mulLo(trigOf(c).rcos((int16_t)camR16(0x6e)), (int16_t)camR16(0x6c)) >> 12;
  int32_t x =
      (int32_t)(int16_t)r16(kCamScratch + 0x0e) + (guestmath::mulLo(trigOf(c).rcos((int16_t)camR16(0x70)), P) >> 12);
  int32_t cz = guestmath::mulLo(trigOf(c).rsin((int16_t)camR16(0x70)), P) >> 12;
  w32(kCamScratch + 0x00, (uint32_t)(x << 16));
  int32_t z = (int32_t)(int16_t)r16(kCamScratch + 0x16) - cz;
  w32(kCamScratch + 0x08, (uint32_t)(z << 16));
  camW8(0x66, camR8(0x66) | 1);
}

void LookAngleBuilder::posBuildB() { // FUN_8006DAD8 — place a look point then yaw/dist accumulate
  int32_t P = (int16_t)(guestmath::mulLo(trigOf(c).rcos((int16_t)camR16(0x6e)), (int16_t)camR16(0x6c)) >> 12);
  int32_t x =
      (int32_t)(uint16_t)r16(kCamScratch + 0x0e) + (guestmath::mulLo(trigOf(c).rcos((int16_t)camR16(0x70)), P) >> 12);
  int32_t cz = guestmath::mulLo(trigOf(c).rsin((int16_t)camR16(0x70)), P) >> 12;
  int32_t dz = (int16_t)((int32_t)(uint16_t)r16(kCamScratch + 0x16) - cz - (int32_t)(uint16_t)r16(kCamScratch + 0x0a));
  int32_t dx = (int16_t)(x - (int32_t)(uint16_t)r16(kCamScratch + 0x02));
  yawDistAccumulate(dx, dz);
}

void LookAngleBuilder::headBuildA(uint32_t nonzero) { // FUN_8006DF88
  if (nonzero == 0) {
    int32_t off = (int32_t)(int16_t)camR16(0x26) - 320;
    w16(kCamScratch + 6, (uint16_t)((int32_t)r16(kCamScratch + 0x12) + off));
  } else {
    int32_t step = guestmath::mulLo(trigOf(c).rsin((int16_t)camR16(0x6e)), (int16_t)camR16(0x6c)) >> 12;
    w16(kCamScratch + 6, (uint16_t)((int32_t)r16(kCamScratch + 0x12) - step));
  }
  camW8(0x66, camR8(0x66) | 2);
}

void LookAngleBuilder::headBuildB() { // FUN_8006DEF0
  int32_t step = guestmath::mulLo(trigOf(c).rsin((int16_t)camR16(0x6e)), (int16_t)camR16(0x6c)) >> 12;
  int32_t s6 = (int32_t)r16(kCamScratch + 6);
  int32_t s12 = (int32_t)r16(kCamScratch + 0x12);
  int32_t d = (s6 + step) - s12;
  if ((uint16_t)(d + 10) < 21) { // |d| <= 10 -> snap
    w16(kCamScratch + 6, (uint16_t)(s12 - step));
    camW8(0x66, camR8(0x66) | 2);
  } else {
    int32_t s = (int32_t)((uint32_t)d << 16) >> 3;
    w32(kCamScratch + 4, (uint32_t)((int32_t)r32(kCamScratch + 4) - s));
  }
}

void LookAngleBuilder::joinE640(int32_t delta, int32_t radius) {
  int32_t sum = (int32_t)r16(kCamGlobal + 0x140) + (int32_t)camR16(0x52) + delta;
  uint16_t theta16 = (uint16_t)sum;
  camW16(0x8C, theta16);
  lookatTail((int16_t)theta16, radius);
}

int32_t LookAngleBuilder::table1Delta() {
  uint8_t idx = r8(kCamGlobal + 0x164);
  switch (idx) {
  case 1:
  case 9:
  case 11: {
    if (r32(kCamGlobal + 0x158) == 0) {
      return table1Entry(*c, kCamGlobal, cam_);
    }
    int32_t g140 = (int16_t)r16(kCamGlobal + 0x140), g56 = (int16_t)r16(kCamGlobal + 0x56);
    int32_t cam56 = (int32_t)camR16(0x56);
    return (g140 == g56) ? -cam56 : cam56; // inverted sense
  }
  case 3: {
    if (camR8(0x77) != 0) {
      return table1Entry(*c, kCamGlobal, cam_);
    }
    if (r8(0x800BF870u) == 6) {
      return table1Entry(*c, kCamGlobal, cam_);
    }
    int32_t g140 = (int16_t)r16(kCamGlobal + 0x140), g56 = (int16_t)r16(kCamGlobal + 0x56);
    int32_t cam56 = (int32_t)camR16(0x56);
    int32_t g168 = (int32_t)r8(kCamGlobal + 0x168) << 6;
    return (g140 != g56) ? (g168 - cam56) : (cam56 - g168);
  }
  default:
    return table1Entry(*c, kCamGlobal, cam_);
  }
}

void LookAngleBuilder::table2(int32_t radius) {
  uint32_t a1 = r8(kCamGlobal + 0x61);
  uint32_t idx2 = ((a1 & 0xff) >> 4) - 1; // unsigned; >=8 -> default
  int32_t theta;
  if (idx2 == 0) {
    if (a1 & 1) {
      theta = (int32_t)r16(0x1F800196u) + 512;
    } else {
      int32_t s196 = (int16_t)r16(0x1F800196u);
      int32_t g140 = (int16_t)r16(kCamGlobal + 0x140);
      int32_t d = ((s196 - g140) & 0xfff) >> 1;
      theta = (int32_t)r16(0x1F800196u) + d;
    }
  } else if (idx2 == 1) {
    if (a1 & 1) {
      int32_t r = Trig::angleCmp(
          (int16_t)r16(kCamGlobal + 0x56), (int16_t)r16(kCamGlobal + 0x140), 0); // FUN_80077768(a,b,mode=0) -> native
      theta = (int32_t)r16(kCamGlobal + 0x140) + (r == 0 ? 512 : 1536);
    } else {
      int32_t a = (int16_t)r16(0x1F800194u);
      int32_t b = (int16_t)r16(0x1F800196u);
      theta = (int32_t)r16(kCamGlobal + 0x140) + (a != b ? 512 : 1536);
    }
  } else if (idx2 == 2) {
    int32_t vu = (int32_t)r16(0x1F800196u);
    theta = vu + ((int16_t)vu / 2);
  } else if (idx2 == 3) {
    theta = (int32_t)r16(0x1F800196u) + 512;
  } else if (idx2 == 7) {
    theta = (int32_t)r16(kCamGlobal + 0x140) + 1024;
  } else {
    theta = (int32_t)r16(kCamGlobal + 0x140) + 512;
  }
  lookatTail(theta & 0xfff, radius);
}

void LookAngleBuilder::rotBuild() { // FUN_8006E464
  if (camR8(0x76)) {
    return; // disabled this frame
  }
  uint32_t a1 = r8(kCamGlobal + 0x61);
  uint32_t a0 = a1 & 0xff;
  if (a0 != 0 && (camR8(0x72) & 0x80)) {
    return;
  }
  int32_t negEE = -(int32_t)r16(0x1F8000EEu);
  int32_t radius = (int16_t)negEE;
  uint8_t c114 = camR8(0x72);
  if (c114 & 0x40) {
    lookatTail((int16_t)camR16(0x8C), radius);
    return;
  } // SHAPE A
  if (a0 != 0 && (a1 & 1) == 0) {
    table2(radius);
    return;
  }
  if (r8(kCamGlobal + 0x17a)) {
    joinE640(0, (int16_t)(negEE - 600));
    return;
  } // radius override
  if (c114 & 2) {
    int32_t delta = (c114 & 1) ? -(int32_t)camR16(0x56) : (int32_t)camR16(0x56);
    joinE640(delta, radius);
    return;
  }
  joinE640(table1Delta(), radius); // TABLE 1
}

// ── distSolve (distance/zoom solver) ─────────────────────────────────────────────────────────────
void LookAngleBuilder::distSolve() { // FUN_8006D2AC
  // 1. settle timer cam[+0x22]
  uint8_t g61 = r8(kCamGlobal + 0x61);
  int16_t timer;
  if (g61 & 0x80) {
    timer = 0;
  } else if (r8(kCamGlobal + 0x17a)) {
    timer = 0;
  } else if ((g61 & 0xff) == 0) {
    timer = 240;
  } else if (g61 & 1) {
    timer = 240;
  } else if (r8(0x800BF816u)) {
    timer = 0;
  } else {
    timer = 240;
  }
  camW16(0x22, (uint16_t)timer);
  uint8_t flags = camR8(0x72);
  if (flags & 4) {
    camW16(0x22, 0);
  }
  flags = camR8(0x72);

  // 2. target planar point (tX,tZ in 16.16) + heading "mode" byte
  int32_t tX, tZ, mode;
  if (flags & 2) {
    tX = (int32_t)r32(kCamGlobal + 0x2c);
    tZ = (int32_t)r32(kCamGlobal + 0x34);
    mode = flags & 1;
  } else {
    uint8_t idx = r8(kCamGlobal + 0x164);
    uint8_t g147 = r8(kCamGlobal + 0x147);
    if (idx >= 13) {
      idx = 8;
    }
    switch (idx) {
    case 2:
    case 3: {
      uint32_t p = r32(kCamGlobal + 0x10);
      tX = (int32_t)(r32(p + 0x2c) << 16);
      tZ = (int32_t)(r32(p + 0x34) << 16);
      mode = g147;
      break;
    }
    case 12: {
      tX = (int32_t)(r16(0x1F800200u) << 16);
      tZ = (int32_t)(r16(0x1F800204u) << 16);
      mode = g147;
      break;
    }
    case 7: {
      tX = (int32_t)(r16(kCamGlobal + 0x14c) << 16);
      tZ = (int32_t)(r16(kCamGlobal + 0x150) << 16);
      mode = g147;
      break;
    }
    case 8: {
      tX = (int32_t)r32(kCamGlobal + 0x2c);
      tZ = (int32_t)r32(kCamGlobal + 0x34);
      mode = g147;
      break;
    }
    default: {
      tX = (int32_t)r32(kCamGlobal + 0x2c);
      tZ = (int32_t)r32(kCamGlobal + 0x34);
      mode = (r32(kCamGlobal + 0x158) == 0) ? (int32_t)g147 : (1 - (int32_t)g147);
      break;
    }
    }
  }

  // 3. base heading + the look-point offset using the settle timer
  int32_t baseAng = (int32_t)r16(kCamGlobal + 0x140);
  if (mode & 0xff) {
    baseAng += 2048;
  }
  int32_t s0a = (int16_t)(uint16_t)baseAng;
  int32_t cam22 = (int16_t)camR16(0x22);
  int32_t s2 = tX + (guestmath::mulLo(trigOf(c).rcos(s0a), cam22) << 4);
  int32_t s1 = tZ - (guestmath::mulLo(trigOf(c).rsin(s0a), cam22) << 4);

  int32_t g140s = (int16_t)r16(kCamGlobal + 0x140);
  int32_t cam58 = (int32_t)camR32(0x58);
  int32_t coordX = tX + (guestmath::mulLo(trigOf(c).rcos(g140s), cam58) >> 4);
  int32_t coordZ = tZ - (guestmath::mulLo(trigOf(c).rsin(g140s), cam58) >> 4);
  int32_t s2q = (s2 - coordX) >> 8;
  int32_t s1q = (s1 - coordZ) >> 8;
  int32_t s0d = guestmath::call(*c, guestmath::kIsqrt, guestmath::mulLo(s2q, s2q) + guestmath::mulLo(s1q, s1q)) << 8;
  int32_t ang = trigOf(c).ratan2(-s1q, s2q);
  int32_t angd = (ang - (int32_t)r16(kCamGlobal + 0x140) - 1024) & 0xfff;

  // 4. smooth cam[+0x14] toward the distance, accumulate into cam[+0x58], place camera X/Z
  int32_t cam14;
  int32_t cur = (int32_t)camR32(0x14);
  if (0x140000 < s0d) {
    if (angd < 2048) {
      int32_t neg = -s0d;
      if (cur < neg) {
        cam14 = ((int32_t)0xffd80000 < neg) ? neg : (int32_t)0xffd80000;
      } else {
        cam14 = (cur > 0 ? 0 : cur) - 65536;
      }
    } else {
      if (s0d < cur) {
        cam14 = (0x0027ffff < s0d) ? 0x00280000 : s0d;
      } else {
        cam14 = (cur < 0 ? 0 : cur) + 65536;
      }
    }
  } else {
    cam14 = (angd < 2048) ? -s0d : s0d;
  }
  camW32(0x14, (uint32_t)cam14);
  int32_t cam58n = cam58 + (cam14 >> 8);
  camW32(0x58, (uint32_t)cam58n);
  camW32(0x08, (uint32_t)(tX + (guestmath::mulLo(trigOf(c).rcos(g140s), cam58n) >> 4)));
  camW32(0x10, (uint32_t)(tZ - (guestmath::mulLo(trigOf(c).rsin(g140s), cam58n) >> 4)));
}

} // namespace tomba::camera
