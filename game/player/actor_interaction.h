// game/player/actor_interaction.h — class ActorInteraction: what Tomba touching something MEANS.
//
// ActorTomba's two per-frame walks (interactWalk, postInteractWalk) walk the field's item lists and
// dispatch each entry here. One method per guest item kind; each is a faithful transcription of the
// authenticated executable/overlay code (ground truth, matching Ghidra 1:1). The per-address RE is in
// docs/engine_re.md; game/player/actor_tomba.h carries the walk-side writeup.
#pragma once

#include <cstdint>

namespace tomba::player {

class ActorTomba;

class ActorInteraction {
public:
  explicit ActorInteraction(ActorTomba &tomba) : tomba_(tomba) {}

  // FUN_80022060 — the proximity test the type 0/1/2/3/7 items share.
  void proximityCheck(uint32_t item);
  // FUN_80114E74 — type 4 subtype 2's guarded check.
  void type4GuardedCheck(uint32_t item);
  // FUN_80022190 — type 6's sub-hitbox check.
  void subHitboxCheck(uint32_t item);
  // FUN_80020364 — the mode-driven step for items 0xF/0x14/0x56 (mode 0) and 0x2F (mode 2). The
  // result is the walk-state the caller stamps onto the item.
  std::uint8_t stepModeInteract(uint32_t item, uint32_t mode);
  // FUN_800205CC — the item-8 interaction, which pushes or rotates Tomba against the item.
  void type8Interact(uint32_t item);
  // FUN_800235A0 — the item-7 interaction. Returns 1 whenever the proximity/step leaf reports a hit.
  std::uint8_t type7Interact(uint32_t item);

private:
  ActorInteraction(const ActorInteraction &) = delete;
  ActorInteraction &operator=(const ActorInteraction &) = delete;

  // The actor whose G block the checks read. Borrowed: it outlives this member, and the CPU comes
  // from the actor's own back-pointer rather than a second one here.
  ActorTomba &tomba_;
};

} // namespace tomba::player