// class BehaviorDispatch — the engine's per-object BEHAVIOR-HANDLER dispatcher.
//
// PROPER OOP: one instance per Core, embedded as `Core::engine::behaviors`. Back-pointer wired
// once by Core's constructor (same pattern as Collision / Bit / Spawn). Callers reach it as
// `eng(c).behaviors.method(args)`.
//
// SCOPE: the single registry that maps a per-object HANDLER address to its native implementation
// (each `beh_*.cpp` behavior file exposes a `beh_*(Core*)` entry that's registered in the
// table). Owns the dispatcher entry point every field walk / transition machine uses to hand off
// one object's per-frame tick — the walker sets the current-object bookkeeping and picks native-
// vs-substrate.
//
// The 50-entry behavior table is a file-local `static const` array in behavior_dispatch.cpp — read
// by every method here. Class doesn't need per-instance storage for it (same registry for every
// Core), but the class itself is per-instance so its methods can reach guest RAM via `this->core`.
#pragma once

#include <cstdint>
#include <lucent/log.h>
struct Core;

namespace tomba {

class BehaviorDispatch {
public:
  Core *core = nullptr;

  // One object's per-frame tick and the guest handler that runs it.
  struct ObjectTick {
    uint32_t object;
    uint32_t handler;
  };
  // Runs the native behavior registered for the handler, else the guest handler through Lightrec.
  void dispatchObj(const ObjectTick &tick);

  // dispatchNative(handler): table lookup + call. Returns true if the handler was owned natively
  //   (and ran), false if there's no native entry (caller must fall through to typed runtime address dispatch). The
  //   behaviors read the object from c->r[4] which caller set — this class doesn't marshal it.
  bool dispatchNative(uint32_t handler);

  // nativeName(handler): the short slug for the native behavior at `handler`, or nullptr if the
  //   object's logic still runs as the guest instruction path substrate. Used by the `ents` REPL diagnostic to
  //   flag which objects are owned.
  const char *nativeName(uint32_t handler) const;

  // `behcov` channel: logs each native handler's first hit and every decade after it.
  void noteNativeHit(uint32_t handler, const char *name);

private:
  static constexpr int kMaxCov = 128;
  struct Cov {
    uint32_t addr;
    uint64_t hits;
  };
  Cov mCov[kMaxCov] = {};
  int mNCov = 0;
  lucent::Channel coverageChannel_{"behcov"};
};

} // namespace tomba
