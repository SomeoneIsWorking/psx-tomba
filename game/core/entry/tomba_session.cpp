// tomba_session.cpp — wires each owner's back-pointer to the session's Core.
#include "entry/tomba_session.h"

namespace tomba {

TombaSession::TombaSession(Core &core)
    : render(std::make_unique<Render>()), cd(core), scheduler(*core.game, cd), audio(native_music, music_list),
      devWarp(core) {
  Core *c = &core;
  verification.bind(core);
  screenFade.core = c;
  engine.core = c;
  engine.cadence().core = c;
  engine.sceneTransition.core = c;
  engine.transitionState3.core = c;
  engine.objectList.core = c;
  engine.array8Dispatch.core = c;
  engine.objectTable.core = c;
  engine.demo.core = c;
  engine.sop.core = c;
  engine.fieldTransition.core = c;
  engine.bgSceneTransitionSm.core = c;
  engine.areaFadeSequencer.core = c;
  engine.parallaxBg.core = c;
  engine.pool.core = c;
  engine.placement.core = c;
  engine.graphicsBind.core = c;
  engine.font.core = c;
  engine.animation.core = c;
  engine.asset.core = c;
  engine.musicCoord.core = c;
  engine.collision.core = c;
  engine.bit.core = c;
  engine.spawn.core = c;
  engine.behaviors.core = c;
  engine.cull.core = c;
  engine.sceneEvents.core = c;
  engine.sfx.core = c;
  engine.audioDispatch.core = c;
  engine.sequencer.core = c;
  engine.areaSlots.core = c;
  engine.modeStateArm.core = c;
  engine.script.core = c;
  engine.actorTomba.core = c;
  engine.attackOrbit.core = c;
  engine.releaseTriggerMotion.core = c;
  engine.actorMeleeEngage.core = c;
  engine.meleeProximity.core = c;
  rng.core = c;
  trig.core = c;
  math.core = c;
  mtx.core = c;
  inventory.core = c;
  saveMenu.core = c;
  music_list.core = c;
  render->mCore = c;
  render->mNodeXform.core = c;
}

} // namespace tomba
