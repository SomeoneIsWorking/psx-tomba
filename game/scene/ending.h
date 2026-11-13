// game/scene/ending.h — class Ending, GAME sub-mode 3 (FUN_80106A24): the field enters it when 0x800BF839 is 8;
// it plays END.STR then LOGO.STR, each through A0L's player 0x80112660, then takes the Quit exit (sub-mode 4).
#pragma once
#include "scene/str_movie.h"

class Core;

namespace tomba {

class Ending {
public:
  // One step of the sub-mode, on sm[0x4c].
  void step(Core &core);

private:
  StrMovie movie_;
};

} // namespace tomba
