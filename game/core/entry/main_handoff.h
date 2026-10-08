#pragma once

class Game;

namespace tomba {

// The disc boot stub's hand-off: LoadExec of cdrom:\MAIN.EXE;1. The bytes come from the run's own disc,
// are authenticated against the manifest, and replace the stub the host loaded.
void loadMainExecutable(Game &game);

} // namespace tomba
