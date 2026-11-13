#include "scene/str_movie.h"

#include "core.h"
#include "entry/session.h"
#include "game.h"
#include "guest_call.h"

namespace tomba {

bool StrMovie::step(Core &core, const char *path) {
  Game &game = *core.game;
  if (!playing_) {
    // An unresolved movie ends at once, as the retail player's would.
    playing_ = sessionOf(&core).playsMovies(game) && game.fmv.beginPath(path);
  }
  if (playing_ && !game.fmv.finished()) {
    game.fmv.step(); // presents its own frames and skips on a held Start
  }
  if (playing_ && !game.fmv.finished()) {
    return false;
  }
  playing_ = false;
  teardown(core);
  return true;
}

// The CdControlB(CdlPause) busy-loop is a no-op here: no async CD read runs in this model.
void StrMovie::teardown(Core &core) {
  core.r[4] = 0;
  psx::cpu::callGuestNow(core, __func__, 0x8001CF00u); // SPU CD mix off
  core.r[4] = 0;
  psx::cpu::callGuestNow(core, __func__, 0x8009C820u); // libgs reset
  core.r[4] = 0;
  psx::cpu::callGuestNow(core, __func__, 0x8009C8BCu); // libgs reset
  psx::cpu::callGuestNow(core, __func__, 0x8008CD40u); // CD event cleanup
  psx::cpu::callGuestNow(core, __func__, 0x8008CCE0u); // alloc/dispatch cleanup
  core.mem_w8(0x1F80019Cu, 0);
  core.mem_w8(0x1F80019Du, 0);
}

} // namespace tomba
