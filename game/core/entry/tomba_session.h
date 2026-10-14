// tomba_session.h — Tomba! 2's per-Core state and its accessors (eng, fade, rngOf, ...).
#pragma once

#include "audio/music_list.h"
#include "audio/native_music.h"
#include "audio/tomba_audio.h"
#include "cd/tomba_cd.h"
#include "cd/tomba_cd_state.h"
#include "core.h"
#include "core/debug/dev_areas.h"
#include "core/debug/verification_counters.h"
#include "core/engine/engine.h"
#include "core/entry/tomba_settings.h"
#include "core/overrides/native_override_catalog.h"
#include "core/sched/tomba_scheduler.h"
#include "game.h"
#include "game_runtime.h"
#include "items/inventory.h"
#include "math/gte_math.h"
#include "math/mtx.h"
#include "math/rng.h"
#include "math/trig.h"
#include "render.h"
#include "render/call_site_objects.h"
#include "render/screen_fade.h"
#include "state_producer.h"
#include "ui/save_menu.h"

#include <memory>

namespace tomba {

// Created with each Game (TombaRuntime::createSession) and destroyed with it.
class TombaSession final : public psx::game::TitleSessionState {
public:
  explicit TombaSession(Core &core);

  const TombaSettings settings = TombaSettings::fromCVars();
  Engine engine;
  VerificationCounters verification;
  ScreenFade screenFade;
  Rng rng;
  Trig trig;
  Math math;
  Mtx mtx;
  Inventory inventory;
  SaveMenu saveMenu;
  NativeMusic native_music; // mixed over the SPU by `audio`
  MusicList music_list;     // the Sound Test catalogue and the area BGM driver
  std::unique_ptr<Render> render;
  render::CallSiteObjects callSites; // the objects of the 2D writers, counted per logic frame
  TombaCd cd;
  TombaCdState cdState{cd};
  sched::TombaScheduler scheduler; // after `cd`, which it holds slot 2 for
  TombaAudio audio;
  TombaDevWarp devWarp;
  psx::present::StateProducerTable stateProducers;
  native::NativeOverrideCatalog nativeOverrides;
};

} // namespace tomba

namespace tomba {

inline tomba::TombaSession &tombaSession(Core *c) {
  return static_cast<tomba::TombaSession &>(*c->game->sessionState);
}

inline Engine &eng(Core *c) {
  return tombaSession(c).engine;
}
inline ScreenFade &fade(Core *c) {
  return tombaSession(c).screenFade;
}
inline Rng &rngOf(Core *c) {
  return tombaSession(c).rng;
}
inline Trig &trigOf(Core *c) {
  return tombaSession(c).trig;
}
inline Math &mathOf(Core *c) {
  return tombaSession(c).math;
}
inline Mtx &mtxOf(Core *c) {
  return tombaSession(c).mtx;
}
inline Inventory &inv(Core *c) {
  return tombaSession(c).inventory;
}
inline SaveMenu &saveMenuOf(Core *c) {
  return tombaSession(c).saveMenu;
}
inline Render *rend(Core *c) {
  return tombaSession(c).render.get();
}

} // namespace tomba
