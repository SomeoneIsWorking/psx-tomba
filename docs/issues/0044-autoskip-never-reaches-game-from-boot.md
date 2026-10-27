# 0044 — PSXPORT_AUTO_SKIP never reaches GAME from boot

Status: open (2026-10-28). Owner: `game/core/debug/auto_drive.cpp` (`AutoDrive::beforeFrame`).

## Observed
`PSXPORT_AUTO_SKIP=1`, unpaced hidden run, 150 s: the log says `armed: drive into GAME free-roam` and never
`reached GAME`; at frame 52,357 the picture is still the opening cutscene (`scratch/player-bugs-3/autoskip-review3/`).
The same build driven by hand (`tap start` at 30 s, then `tap x` twice) leaves the cutscene and reaches the first
field (`scratch/player-bugs-3/tap-review/strip.png`), so the virtual gamepad reaches the guest.

## What is known
In `ReachGame` the driver taps Cross every 12 frames until the stage is `kStageGameEntry`; the opening cutscene is left
by Start, which the driver only presses in its later `IntroCutscene` phase. The taps now count pad frames, as the old
`Pad::driveTap` drive did, so the button choice, not the clock, is the first suspect. Not traced further in this pass.
