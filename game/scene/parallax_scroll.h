// The parallax backdrop's scroll reduction (guest over-then-rollback loop). Pure arithmetic; ParallaxBg
// owns where the moduli come from.
#pragma once
#include <cstdint>

namespace tomba::parallax {

// The guest's own reduction, transcribed from the instruction path's over-then-rollback loops.
// REQUIRES mod > 0. Returns `v` unchanged whenever 0 <= v < mod.
//
// This is NOT a wrap into [0, mod) and does not agree with `v % mod`: for v = -5, mod = 256 it
// returns -5, and for v = 600 it returns 344. That is deliberate — it is what the title computes,
// and ParallaxBg::step stores the result as a signed 16-bit field that later readers sign-extend.
inline int32_t guestReduce(int32_t v, int32_t mod) {
  if (v < 0) {
    v += mod;
    while (v < 0) {
      v += mod;
    }
    v -= mod;
  }
  if (mod <= v) {
    v -= mod;
    while (mod <= v) {
      v -= mod;
    }
    v += mod;
  }
  return v;
}

} // namespace tomba::parallax
