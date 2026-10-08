// game/math/gte_transform3.h — the GTE 3-vertex rotate-and-pack utility (guest FUN_80084250).
//
// Sits immediately after the owned Math cluster in gte_math.{h,cpp} (matMul 0x80084110,
// applyMatlv 0x80084220, applyMatrixLV 0x80084470) and shares its shape: load a rotation matrix
// into the GTE control registers, run RTPS per vertex, pack the results back.
#pragma once
struct Core;
class Game;

class GteTransform3 {
public:
  // FUN_80084250(matInOut = a0, src = a1) -> a0 in v0.
  //
  // a0 is BOTH input and output: five words holding the CR-packed rotation matrix on entry, and the
  // packed per-vertex IR results on exit, overwritten in place. a1 is a 20-byte 3-vertex SoA source.
  //
  // The guest PIPELINES the GTE: each vertex's IR1/IR2/IR3 are read back BEFORE the NEXT vertex's
  // operands are written, never after its own RTPS. That ordering is load-bearing (it is what makes
  // the reads observe the right vertex) and is transcribed as-is rather than tidied.
  static void rotate3AndPackIr(Core *c);

  static void registerOverrides(Game *game);
};
