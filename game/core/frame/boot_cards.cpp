#include "frame/boot_cards.h"

#include "core.h"
#include "display_presenter.h"
#include "entry/session.h"
#include "frame/frame_cadence.h"
#include "frame/scea_splash.h"
#include "game.h"

#include <lucent/log.h>

#include <algorithm>
#include <cmath>

namespace tomba {
namespace {

// Measured from the stub against the oracle: a linear fade-in, a full-brightness hold, a short fade-out.
constexpr int kSceaFadeIn = 57;
constexpr int kSceaHold = 245;
constexpr int kSceaFadeOut = 3;
constexpr int kSceaFrames = kSceaFadeIn + kSceaHold + kSceaFadeOut;
constexpr int kFadeFull = 128;
constexpr const char *kBootMovie = "MOVIE/LOGO.STR";

int sceaFade(int frame) {
  if (frame < kSceaFadeIn) {
    return frame * kFadeFull / kSceaFadeIn;
  }
  if (frame < kSceaFadeIn + kSceaHold) {
    return kFadeFull;
  }
  return (kSceaFrames - 1 - frame) * kFadeFull / kSceaFadeOut;
}

} // namespace

void BootCards::step(Core &core) {
  switch (phase_) {
  case Phase::Scea:
    stepScea(core);
    break;
  case Phase::Movie:
    stepMovie(core);
    break;
  case Phase::Done:
    return;
  }
  core.game->presentation.commitUnpresented();
}

void BootCards::stepScea(Core &core) {
  Game &game = *core.game;
  if (sceaImage_.empty()) {
    sceaImage_ = decodeSceaSplash();
  }
  // The host turn is ours, so the pad is read here; a held Start skips the card.
  game.pad.pollHostInput();
  const bool skipped = (game.pad.buttons & psx::input::kButtonStart) == 0;
  if (skipped) {
    lucent::info("scea", "skipped (Start) at frame {}", sceaFrame_);
  } else {
    // The card frames were measured at the stub's 30 fps rate; at 60 fps the second field shows the fade halfway
    // to the next card frame's.
    const int parts = game.mods.fps60 != 0 ? 2 : 1;
    for (int part = 0; part < parts; ++part) {
      const float fade = std::lerp(static_cast<float>(sceaFade(sceaFrame_)),
                                   static_cast<float>(sceaFade(std::min(sceaFrame_ + 1, kSceaFrames - 1))),
                                   static_cast<float>(part) / static_cast<float>(parts));
      game.display.buildHostPicture(sceaImage_.data(), {kSceaSplashWidth, kSceaSplashHeight}, fade / kFadeFull);
      const psx::frame::FieldCadence cadence{FrameCadence::kRetailVblanksPerLogicFrame, parts};
      game.display.showAt(game.framePacer.subframeDeadline(core, cadence),
                          {.fence = game.presentation.fence(),
                           .t = static_cast<float>(part + 1) / static_cast<float>(parts),
                           .fields = cadence.fields,
                           .parts = cadence.parts});
    }
  }
  if (skipped || ++sceaFrame_ >= kSceaFrames) {
    sceaImage_ = {};
    game.display.build(psx::present::PresentCompletion::Transition);
    game.display.show();
    phase_ = Phase::Movie;
  }
}

void BootCards::stepMovie(Core &core) {
  Game &game = *core.game;
  if (!movieOpen_) {
    movieOpen_ = true;
    if (sessionOf(&core).settings.skipMovies || !game.fmv.beginPath(kBootMovie)) {
      finish(core);
      return;
    }
  }
  game.pad.pollHostInput();
  game.fmv.step();
  if (game.fmv.finished()) {
    finish(core);
  }
}

void BootCards::finish(Core &core) {
  core.game->display.build(psx::present::PresentCompletion::Transition);
  core.game->display.show();
  phase_ = Phase::Done;
}

} // namespace tomba
