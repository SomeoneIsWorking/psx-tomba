// game/camera/camera_guest_math.h — the guest maths vocabulary the camera cluster shares.
//
// The libgte trig/matrix helpers, the shake RNG and the sound-effect queue are LIBRARIES, not PSX
// hardware, so the camera reaches them through the substrate rather than owning them. This header
// names that set once and gives the cluster its one guest-call spelling and its two integer helpers
// that reproduce the guest's exact arithmetic.
#pragma once

#include "core.h"
#include "guest_call.h"

#include <cstdint>

namespace tomba::camera::guestmath {

// Retained libgte entries.
inline constexpr uint32_t kIsqrt = 0x80084080u;         // isqrt(x) -> floor(sqrt); rotBuild's entry
inline constexpr uint32_t kRsinSubstrate = 0x80083E80u; // rsin's substrate body, for the byte-exact arms
inline constexpr uint32_t kLookAtIsqrt = 0x80077FB0u;   // lookAt's matrix helpers use a DIFFERENT isqrt
inline constexpr uint32_t kMrInit = 0x80051794u;        // MR_init(identity)
inline constexpr uint32_t kMulMatrix0 = 0x80084250u;    // MulMatrix0(a0,a1)
inline constexpr uint32_t kCopyMatrix = 0x800847B0u;    // CopyMatrix(a0,a1)
// The shake tail's helpers: a utility RNG and the sound/rumble-effect request queue.
inline constexpr uint32_t kShakeRand = 0x8009A450u;
inline constexpr uint32_t kShakeFx = 0x800521F4u;

// The cluster's one guest-call convention: load a0..a3, dispatch, return v0.
inline int32_t call(Core &c, uint32_t fn, int32_t a0 = 0, int32_t a1 = 0, int32_t a2 = 0, int32_t a3 = 0) {
  c.r[4] = (uint32_t)a0;
  c.r[5] = (uint32_t)a1;
  c.r[6] = (uint32_t)a2;
  c.r[7] = (uint32_t)a3;
  psx::cpu::dispatchGuestToReturn0(c, fn, psx::cpu::ExecutionBudget::currentTurn(c), __func__);
  return (int32_t)c.r[2];
}

// Clamp a sign-extended s16 delta to +/-maxStep (engine FUN_8006CE74).
inline int32_t clampStep(int16_t delta, int16_t maxStep) {
  if (delta >= 0) {
    return delta < maxStep ? delta : maxStep;
  }
  return delta < (int16_t)(-maxStep) ? (int16_t)(-maxStep) : delta;
}

// The 32-bit multiply-low, matching the guest instruction path's truncating arithmetic exactly.
inline int32_t mulLo(int32_t a, int32_t b) {
  return (int32_t)((uint32_t)a * (uint32_t)b);
}

} // namespace tomba::camera::guestmath