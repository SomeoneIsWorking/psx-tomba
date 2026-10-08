// game/ai/actor_zoned_attacker_zone_classify.cpp — ActorZonedAttacker::zoneClassify, the
// zoned attacker's {0,1,2} zone-band classifier (guest 0x80145C78).
#include "actor_zoned_attacker.h"

#include "core.h"

#include <cstdint>

// FUN_80145C78 — classifies (u8 at record+0x2A, s16 at record+0x36) into a {0,1,2} zone band.
// 7,504 substrate dispatches per 6000 replay frames. Its only caller in the whole image is
// FUN_8014047C, which actor_zoned_attacker.cpp owns as ActorZonedAttacker::gateCheck.
//
// NAMED FOR THE MECHANISM ONLY. The RE proposed "phaseZone"; the adversarial verify pass rejected
// that, because the body proves the classification but proves nothing about the byte being a story
// phase — that reading comes from a comment elsewhere, not from here. zoneClassify says what is
// demonstrable and stops.
// ORACLE: overlay guest 0x80145C78
namespace tomba::ai {

void ActorZonedAttacker::zoneClassify(Core *c) {
  c->r[2] = (uint32_t)((int32_t)c->r[4] < 4);
  {
    int _t = (c->r[2] == c->r[0]);
    if (_t) {
      goto L_80145C98;
    }
  }
  c->r[2] = (uint32_t)(int16_t)c->mem_r16((c->r[5] + (uint32_t)10));
  c->r[2] = (uint32_t)((int32_t)c->r[2] < 4700);
  c->r[2] = c->r[2] ^ 1u;
  return;
L_80145C98:;
  c->r[2] = c->r[0] + (uint32_t)7;
  {
    int _t = (c->r[4] == c->r[2]);
    c->r[3] = c->r[4] + (uint32_t)-4;
    if (_t) {
      goto L_80145CB8;
    }
  }
  c->r[3] = (uint32_t)(c->r[3] < (uint32_t)8);
  {
    int _t = (c->r[3] != c->r[0]);
    c->r[2] = c->r[0] + (uint32_t)1;
    if (_t) {
      goto L_80145CC8;
    }
  }
  c->r[2] = c->r[0] + (uint32_t)2;
  return;
L_80145CB8:;
  c->r[2] = (uint32_t)(int16_t)c->mem_r16((c->r[5] + (uint32_t)10));
  c->r[2] = (uint32_t)((int32_t)c->r[2] < 4700);
  c->r[2] = c->r[2] ^ 1u;
L_80145CC8:;
  return;
}

} // namespace tomba::ai
