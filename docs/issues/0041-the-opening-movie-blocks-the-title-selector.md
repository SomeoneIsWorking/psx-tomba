# 0041 — the opening movie blocks the title selector

Features: S026.

Status: fixed (2026-10-23).

## Observed
The in-window title selector showed Tomba! 1's panel playing its publisher logo and then standing still, and the
Tomba! 2 panel black, for about a minute (`scratch/interp-regression/picker2`): the selector's own presents and its
control channel both stopped. The run log (`~/.local/state/psxport/tomba2/last-run.log`, 2026-10-23 12:51) ends
during `MOVIE/OP.STR`.

## Cause
DEMO s1's opening-movie sub-sequence called `Fmv::play("MOVIE/OP.STR")` (`game/scene/demo.cpp`,
`demo_menu_machine`), a blocking player: it decoded all 2,026 frames in one turn, 67.6 s, so the picker's loop —
which steps the panels — never returned and no panel composited. The boot LOGO is stepped (`BootCards`), which is
why the logo animated and the opening did not. `TitleSession` is documented as steppable precisely so the selector
can run several titles; a blocking call defeats it.

## Fix
`Demo::stepOpeningMovie` opens OP.STR once (`Fmv::beginPath`) and steps it one frame per turn (the boot LOGO's
pattern), returning true when it ends so the state-7 teardown runs on that turn. The guest's whole sub-sequence
stays owned; only the block is removed. Skip behavior is unchanged: `Fmv::step` still ends on a held Start, and
`PSXPORT_NO_FMV` still skips the movie.

## Evidence
`scratch/interp-regression/picker3` (2 panels, real speed, 3840x2160): no control-channel timeout during the
opening; both panels hold their own pictures through it, and the Tomba! 2 panel shows OP.STR frames while the
Tomba! 1 panel runs its own boot.
