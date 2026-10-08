#include "frame/boot_cards.h"

#include "core.h"
#include "frame/movie_policy.h"
#include "game.h"
#include "gpu_native_internal.h"
#include "gpu_vk.h"
#include "host_input.h"
#include "scea_asset.h"

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
  core.game->presentation.commitUnpresented(&core);
}

void BootCards::stepScea(Core &core) {
  Game &game = *core.game;
  if (sceaImage_.empty()) {
    sceaImage_.resize(static_cast<size_t>(SCEA_DISP_W) * SCEA_DISP_H * 4);
    gpu_scea_decode_rgba(sceaImage_.data());
  }
  // The host turn is ours, so the pad is read here; a held Start skips the card.
  game.pad.pollHostInput();
  const bool skipped = (game.pad.buttons & psx::input::kButtonStart) == 0;
  if (skipped) {
    lucent::info("scea", "skipped (Start) at frame {}", sceaFrame_);
  } else {
    gpu_vk_present_image(
        &core, sceaImage_.data(), SCEA_DISP_W, SCEA_DISP_H, static_cast<float>(sceaFade(sceaFrame_)) / kFadeFull);
    game.framePacer.paceFrame(core);
  }
  if (skipped || ++sceaFrame_ >= kSceaFrames) {
    sceaImage_ = {};
    gpu_clear_display(&core);
    phase_ = Phase::Movie;
  }
}

void BootCards::stepMovie(Core &core) {
  Game &game = *core.game;
  if (!movieOpen_) {
    movieOpen_ = true;
    if (!MoviePolicy::plays(false) || !game.fmv.beginPath(kBootMovie)) {
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
  gpu_clear_display(&core);
  phase_ = Phase::Done;
}

} // namespace tomba
