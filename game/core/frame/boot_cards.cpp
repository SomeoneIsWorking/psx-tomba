#include "frame/boot_cards.h"

#include "core.h"
#include "display_presenter.h"
#include "entry/session.h"
#include "frame/frame_cadence.h"
#include "game.h"
#include "scea_splash.h"

#include <lucent/log.h>

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
    sceaImage_ = psx::platform::decodeSceaSplash();
  }
  // The host turn is ours, so the pad is read here; a held Start skips the card.
  game.pad.pollHostInput();
  const bool skipped = (game.pad.buttons & psx::input::kButtonStart) == 0;
  if (skipped) {
    lucent::info("scea", "skipped (Start) at frame {}", sceaFrame_);
  } else {
    core.game->display.presentHostPicture(sceaImage_.data(),
                                          {psx::platform::kSceaSplashWidth, psx::platform::kSceaSplashHeight},
                                          static_cast<float>(sceaFade(sceaFrame_)) / kFadeFull);
    // The card frames were measured at the stub's 30 fps rate.
    game.framePacer.paceSubframeFields(core, {FrameCadence::kRetailVblanksPerLogicFrame, 1});
  }
  if (skipped || ++sceaFrame_ >= kSceaFrames) {
    sceaImage_ = {};
    game.display.present(psx::present::PresentCompletion::Transition);
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
  core.game->display.present(psx::present::PresentCompletion::Transition);
  phase_ = Phase::Done;
}

} // namespace tomba
