// session.h — Tomba! 2's per-Core state and its accessors (eng, fade, rngOf, ...).
#pragma once

#include "audio/audio.h"
#include "audio/music_list.h"
#include "audio/native_music.h"
#include "cd/engine_cd.h"
#include "cd/engine_cd_state.h"
#include "core.h"
#include "core/debug/dev_areas.h"
#include "core/debug/verification_counters.h"
#include "core/engine/engine.h"
#include "core/entry/settings.h"
#include "core/overrides/native_override_catalog.h"
#include "core/sched/scheduler.h"
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

// Created with each Game (Runtime::createSession) and destroyed with it.
class Session final : public psx::game::TitleSessionState {
public:
  explicit Session(Core &core);

  const Settings settings = Settings::fromCVars();
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
  EngineCd cd;
  EngineCdState cdState{cd};
  sched::Scheduler scheduler; // after `cd`, which it holds slot 2 for
  Audio audio;
  DevAreas devWarp;
  psx::present::StateProducerTable stateProducers;
  native::NativeOverrideCatalog nativeOverrides;
};

} // namespace tomba

namespace tomba {

inline tomba::Session &sessionOf(Core *c) {
  return static_cast<tomba::Session &>(*c->game->sessionState);
}

inline Engine &eng(Core *c) {
  return sessionOf(c).engine;
}
inline ScreenFade &fade(Core *c) {
  return sessionOf(c).screenFade;
}
inline Rng &rngOf(Core *c) {
  return sessionOf(c).rng;
}
inline Trig &trigOf(Core *c) {
  return sessionOf(c).trig;
}
inline Math &mathOf(Core *c) {
  return sessionOf(c).math;
}
inline Mtx &mtxOf(Core *c) {
  return sessionOf(c).mtx;
}
inline Inventory &inv(Core *c) {
  return sessionOf(c).inventory;
}
inline SaveMenu &saveMenuOf(Core *c) {
  return sessionOf(c).saveMenu;
}
inline Render *rend(Core *c) {
  return sessionOf(c).render.get();
}

} // namespace tomba
