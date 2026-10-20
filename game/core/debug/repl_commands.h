// repl_commands.h — Tomba! 2's developer commands (ents, node, geomblk, stage, bgm, musictest, ...).
#pragma once

#include "command_args.h"

class Core;

namespace tomba {

// True when the command word was one of these; replies go to the log.
bool replCommand(Core &core, const psx::debug::CommandArgs &args);

} // namespace tomba
