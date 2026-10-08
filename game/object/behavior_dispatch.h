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
// vs-substrate. Was the free functions `dispatch_obj_method` / `dispatch_native_behavior` /
// `behavior_native_name` in engine_tomba2.cpp.
//
// The 50-entry behavior table is a file-local `static const` array in behavior_dispatch.cpp — read
// by every method here. Class doesn't need per-instance storage for it (same registry for every
// Core), but the class itself is per-instance so its methods can reach guest RAM via `this->core`.
#pragma once
#include <cstdint>
struct Core;

class BehaviorDispatch {
public:
  Core *core = nullptr;

  // dispatchObj(obj, handler): route ONE object's per-frame tick — run either the native behavior (if registered in the
  // table) or the guest instruction path
  //   substrate leaf via typed runtime address dispatch. Used by the field entity-list walkers (ObjectList / Array8
  //   Dispatch / TransitionState3) and any other per-object dispatcher.
  //   The test-only verification substrate leg skips the native table and crosses the PSXPort guest
  //   boundary. Gameplay has no selectable substrate/interpreter mode.
  void dispatchObj(uint32_t obj, uint32_t handler);

  // dispatchNative(handler): table lookup + call. Returns true if the handler was owned natively
  //   (and ran), false if there's no native entry (caller must fall through to typed runtime address dispatch). The
  //   behaviors read the object from c->r[4] which caller set — this class doesn't marshal it.
  bool dispatchNative(uint32_t handler);

  // nativeName(handler): the short slug for the native behavior at `handler`, or nullptr if the
  //   object's logic still runs as the guest instruction path substrate. Used by the `ents` REPL diagnostic to
  //   flag which objects are owned.
  const char *nativeName(uint32_t handler) const;

  // ---- COVERAGE (`PSXPORT_DEBUG=behcov`) --------------------------------------------------------
  // Which native behaviour handlers a given scenario/replay actually REACHES. A rebuilt beh_* body
  // can only diverge from the guest body it replaces on a path that EXECUTES, so an equivalence A/B
  // (PSXPORT_MIRROR_VERIFY=<addr>) over a handler this scenario never dispatches proves NOTHING.
  // Coverage therefore has to be MEASURED before an A/B campaign, per replay — and it cannot be read
  // out of the mirror-verify log, whose "OK (pass #N)" line is throttled 1-in-64 against a SINGLE
  // counter shared by every armed address (verify_harness.cpp), so a rarely-reached handler can be
  // checked dozens of times and never print.
  // Emits one line the FIRST time each handler runs natively, then at log-scale milestones
  // (10/100/1000/…), so the log doubles as an order-of-magnitude hit count without spamming a
  // handler that fires for every object every frame.
  void noteNativeHit(uint32_t handler, const char *name);

  // ---- PER-INVOCATION NODE DELTA (`PSXPORT_BEH_TRACE=<hex addr>`) -------------------------------
  // The A/B tool that ATTRIBUTES a divergence once tools/beh_ab.sh has found one. Wraps the dispatch
  // of one handler and logs, per invocation, exactly which bytes of the object's node block the
  // handler CHANGED (frame, object, offset, old->new). Because it sits at the DISPATCH SITE, the log
  // has the same shape whether the native body or the substrate guest-visible behavior ran — so `diff` between the
  // native run and the PSXPORT_BEH_SUBSTRATE=<addr> run points at the first invocation, object and
  // node field where the rebuild stops matching the guest, which is where the instruction-level slip
  // lives. (Tracing inside the native body cannot do this: forcing the handler to the substrate
  // means the native body, and its trace, never execute.)
  //   Returns true when it ran the dispatch itself (handler armed); false = caller dispatches.
  bool traceDispatch(uint32_t obj, uint32_t handler);

private:
  static constexpr uint32_t kTraceNodeBytes = 0x80; // node block width the delta log covers
  uint32_t mTraceAddr = 0;                          // 0 = unparsed, 0xFFFFFFFF = off

  static constexpr int kMaxCov = 128;
  struct Cov {
    uint32_t addr;
    uint64_t hits;
  };
  Cov mCov[kMaxCov] = {};
  int mNCov = 0;
  int mCovOn = -1; // latched cfg_dbg("behcov") (-1 = not latched yet)
};
