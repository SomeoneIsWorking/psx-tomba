// repl_commands.h — Tomba! 2's developer commands (ents, census, stage, bgm, musictest, ...).
#pragma once

class Core;

namespace tomba {

// True when `cmd` was one of these; replies go to the log.
bool replCommand(Core &core, const char *line);

} // namespace tomba
