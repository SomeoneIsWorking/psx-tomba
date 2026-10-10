# 0034 — the debug warp launches the transition the community debug menu uses

Status: closed. FREE POSITION, SYSTEM TOOLS and WORLD INFO of the menu are not decoded (not asked for in the fix).

## Result

The routines, state words and tables are in `docs/re-frontier.md` ("Debug menu decode"). The community code list is
evidence only; none of it is in the repo.

- `warp <area> [entry]` (`game/core/debug/dev_warp.cpp`) raises the game's own pending-transition request
  (`0x800BF839`) with the destination halfword (`0x800BF83A`) at the frame boundary. The game's field machine runs
  the transition. The synchronous cold path is deleted: nothing it reached is out of reach of the transition. From the
  scripted opening the warp first leaves it for ordinary play (issue 0029).
- `items all`, `items <id> [amount]`, `flag get <i>` and `flag set <i> <v>` are armed on the control channel and
  written at the frame boundary. The Debug tab has an Entry row, a Grant All Items button and Flag, Value, Read and
  Write rows that send the same lines.
- The transition needed two engine fixes: `Engine::frame` now owns sub-mode 5, and `submode1Case0Native` loads the
  area in `0x800BF870`.

## Open

- Areas 4 and 5 abort after the warp (issue 0035).
- The Debug tab rows load without errors (28 rows, 4 of 4 readouts); a click through them was not driven, because
  the menu keys are not on the control channel.
