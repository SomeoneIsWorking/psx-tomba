// game/scene/str_movie.h — class StrMovie, the retail STR movie sub-sequence (DEMO 0x80107054..0x80107280,
// A0L 0x80112660) owned natively: the host player shows the movie, then the retail player's teardown runs.
#pragma once

class Core;

namespace tomba {

class StrMovie {
public:
  // Steps `path` one frame per turn; true once it has ended (or never played) and the teardown ran. The next
  // call starts the movie over.
  bool step(Core &core, const char *path);

private:
  // The retail player's last state: SPU and libgs reset, CD cleanup, skip and playing flags cleared.
  static void teardown(Core &core);

  bool playing_ = false;
};

} // namespace tomba
