# 0044 — PSXPORT_AUTO_SKIP never reaches GAME from boot

Status: closed (2026-10-28). Owner: `game/core/debug/auto_drive.cpp` (`AutoDrive::beforeFrame`).

## Observed
`PSXPORT_AUTO_SKIP=1`, unpaced hidden run, 150 s: the log says `armed: drive into GAME free-roam` and never
`reached GAME`; at frame 52,357 the picture is still the opening cutscene (`scratch/player-bugs-3/autoskip-review3/`).
The same build driven by hand (`tap start` at 30 s, then `tap x` twice) leaves the cutscene and reaches the first
field (`scratch/player-bugs-3/tap-review/strip.png`), so the virtual gamepad reaches the guest.

## What is known
In `ReachGame` the driver taps Cross every 12 frames until the stage is `kStageGameEntry`; the opening cutscene is left
by Start, which the driver only presses in its later `IntroCutscene` phase. The taps now count pad frames, as the old
`Pad::driveTap` drive did, so the button choice, not the clock, is the first suspect. Not traced further in this pass.

## Cause
Two defects. `AutoDrive` ran in every session, including the picker's Tomba 2 preview panel, and its taps go
through the virtual gamepad into the host queue the picker reads: the first tap confirmed panel 0 (Tomba! 1), so
the product the run watched was never driven. And the driver pressed Cross on every pre-GAME screen, while the
opening movie (DEMO s1, `MOVIE/OP.STR`) and the scripted opening (`DevWarp::kLoadMode` 2) are left only by Start.

## Fix
The driver presses through psxport's `Pad::injectButtons`, which refuses a press from a Game that does not hold
the player's input (psxport issue 0182 owns the entry), so a preview panel's taps never reach the picker; and
`AutoDrive::screenOf` names the screen from guest state (task 0 stage, DEMO state word, load mode) so each screen
gets its own skip: Cross on boot and the front end, Start on the opening movie and the scripted opening.

## Shown
`PSXPORT_AUTO_SKIP=1`, unpaced hidden run, `pick tomba2` over the control channel: `front end at frame 563`,
`opening movie at frame 564`, `fmv skipped by Start`, `scripted opening at frame 682`, `free roam at frame 794`;
the shot at 90 s is Tomba in the first field (`scratch/player-bugs-4/autoskip2/end_small.png`). Repeated with the
press refusal in `Pad::injectButtons` instead of a driver guard: the same screens at the same frames (563, 564, 682,
794) and Tomba in the first field (`scratch/player-bugs-4/autoskip3/run.log`, `end_small.png`).

