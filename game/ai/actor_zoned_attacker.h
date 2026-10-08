#pragma once

struct Core;
class Game;

namespace tomba::ai {

class ActorZonedAttacker {
public:
  // FUN_8014047C — the tick/despawn gate predicate; v0 is the result.
  static void gateCheck(Core *c);
  // FUN_80140544 — per-type initialization.
  static void typeInit(Core *c);
  // FUN_801409C0 — pick the attack for the current range; v0 is the chosen attack.
  static void pickAttackByRange(Core *c);
  // FUN_80143A00 — the default (non-motion) sub-state machine.
  static void defaultSubStateMachine(Core *c);
  // FUN_80144928 — approach the target and face it; v0 signals arrival.
  static void approachAndFace(Core *c);
  // FUN_80144B50 — the idle sub-state tick.
  static void idleTick(Core *c);
  // FUN_80145C78 — classify (record+0x2A, record+0x36) into a {0,1,2} zone band.
  static void zoneClassify(Core *c);

  // Installs all seven guest addresses in the shared override registry.
  static void registerOverrides(Game *game);
};

} // namespace tomba::ai