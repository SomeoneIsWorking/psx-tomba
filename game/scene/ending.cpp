#include "scene/ending.h"

#include "core.h"
#include "core/engine/engine.h"
#include "core/engine/task_sm.h"
#include "entry/session.h"
#include "guest_call.h"

#include <cstdint>

namespace tomba {
namespace {

// A0L 0x8011A6AC: {path, staging, frames} per movie the player takes by index.
constexpr const char *kMovies[] = {"MOVIE/END.STR", "MOVIE/LOGO.STR"};

// The tail every set-up state shares: clear the movie timer and flags, then advance.
void armMovie(Core &core, TaskSm &sm) {
  core.mem_w32(0x1F800250u, 0);
  core.mem_w8(0x1F80019Du, 0);
  core.mem_w8(0x1F80019Cu, 0);
  sm.setS4e(0);
  sm.setStage4c(static_cast<std::uint16_t>(sm.stage4c() + 1));
}

void resetForMovie(Core &core) {
  eng(&core).pool.reset75240();
  core.r[4] = 1;
  psx::cpu::callGuestNow(core, __func__, 0x8001CF00u); // SPU CD mix on
}

} // namespace

// The retail skip (any button after 0x466 frames, through 0x1F80019D) is the native player's Start skip.
void Ending::step(Core &core) {
  TaskSm sm(&core);
  switch (sm.stage4c()) {
  case 0:
    core.r[4] = 0;
    psx::cpu::callGuestNow(core, __func__, 0x80050894u);
    psx::cpu::callGuestNow(core, __func__, 0x8001CF2Cu); // audio settle
    resetForMovie(core);
    core.mem_w16(0x800E7E68u, 0); // pad edges
    armMovie(core, sm);
    return;
  case 1:
  case 3: {
    const int movie = sm.stage4c() == 1 ? 0 : 1;
    sm.setF69(static_cast<std::uint8_t>(movie));
    if (movie_.step(core, kMovies[movie])) {
      sm.setStage4c(static_cast<std::uint16_t>(sm.stage4c() + 1));
    }
    return;
  }
  case 2:
    resetForMovie(core);
    armMovie(core, sm);
    return;
  case 4:
    psx::cpu::callGuestNow(core, __func__, 0x8001CF2Cu); // audio settle
    sm.setSubMode(4);
    sm.setStage4c(0);
    sm.setS4e(0);
    return;
  default:
    return;
  }
}

} // namespace tomba
