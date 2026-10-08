// class Mtx — PC-native libgte matrix helpers (tiny leaves the game calls all over).
//
// PROPER OOP: instance subsystem, one per Core (embedded as `Core::mtx`). Back-pointer wired in
// Core::Core(). Callers reach the methods as `mtxOf(c).identity(addr)` — no Core* on the surface;
// the class reads/writes guest memory via `this->core`. Was previously `Mtx::identity(Core*, addr)`
// (static-with-Core); promoted to match the standard subsystem shape (same as Rng / Trig / Math).
//
// Each method here corresponds ONE-TO-ONE to a resident libgte leaf that the game (and the
// substrate) still calls. Owning them native folds the substrate hops those calls used to be.
// Layout is the standard PSX libgte MATRIX: `short m[3][3]` (9 int16 = 18 bytes) + 2 bytes pad
// + `long t[3]` (3 int32 = 12 bytes) = 32 bytes total.
#pragma once
#include <cstdint>
class Core;

class Mtx {
public:
  Core *core = nullptr;

  // MR_init (guest FUN_80051794): write identity (diag 0x1000 in 4.12 fixed, all else zero) at
  //   `addr`. 8 word stores, faithful to the guest.
  void identity(uint32_t addr);

  // Wire 0x80051794 into the ONE override registry, WITH the main-module setter — this leaf has ~22
  // direct `guest 0x80051794(c)` call sites spread across every shard as well as typed runtime address dispatch ones,
  // and only the setter covers the direct half. Until 2026-07-29 the native above existed but was never installed, so
  // it served native callers only while the substrate kept running the guest body 27,594 times per 6000 replay frames —
  // the #3 entry on the recdep histogram, and pure duplication rather than missing work.
  //
  // Safe to wire where Trig::registerOverrides deliberately is NOT: abi_extract reports frame_size 0
  // for this address, so there is no guest stack frame for the native to fail to mirror, which is
  // exactly the hazard that keeps rsin/rcos unregistered (see game/math/trig.cpp's banner).
  static void registerOverrides(class Game *game);

  // NOTE: the diagonal-scale matrix write (guest FUN_800517BC) is owned by
  // `NodeXform::seedBlock` (game/render/node_xform.h) — that is the wired dispatch target for
  // this address. A duplicate `Mtx::diagonal` (dead code, zero callers) lived here until
  // 2026-07-08's dual-ownership dedup; do not re-add it — call `rend(c)->mNodeXform.seedBlock(...)`
  // instead.
};
