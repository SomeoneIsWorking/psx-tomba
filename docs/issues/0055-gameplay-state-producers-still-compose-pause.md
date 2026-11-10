# 0055 — gameplay state producers still compose the pause screen

Features: F-FPS60.

Status: open (2026-11-10).

## Reproduction
`warp 0/0`, hold right 120 frames, release, tap start, wait 90 frames, then debug `presentseq,compose` and dump 8
presents; unpaced, `ires=0`, `fps60=1`, `PSXPORT_PRESENT_SINK=3840x2160` (`scratch/replay/pause.py`).

## Observed
The replay covers every gameplay in-between (plan `replay-interpolation.md`), but `DrawReplay::beforeDraw` does not
replay a paused draw (`0x1F800136 != 0`), so the pause screen composes its in-betweens from the state producers.
With the gameplay-only producers removed (RainStreaks, LineStrip, GradientBackdrop, AreaMap, DustField,
BackdropPlane, BackdropSprites, FogModelEmitter, FxSpriteAnchored, FxSpriteSwarm, QuadRtptSubmit;
`scratch/replay/producer_deletion.diff`) every paused in-between is shown whole:
`object 0x80027cb4/0x000edc08 has no render or state` (`scratch/replay/z0/run.log`, 24 lines, 0 composed). On HEAD
the same run composes all of them (`scratch/replay/z1/run.log`). The pause census (`scratch/replay/census`) renders
0x80027CB4, 0x800281EC and the overlay object draws while paused.

## Cause
`game/core/frame/draw_replay.cpp:DrawReplay::beforeDraw` treats a paused frame as not replayable, which keeps every
producer that can draw under the pause menu live.

## Proper fix
Replay the paused draw: the field draw runs while paused with a frozen state, so the section replays with every
track held, and the pause menu's own writers (Font, UiSprite, Panel, ScreenFade) are captured or tracked. Then
delete the gameplay producers in one change and remove their rows from `docs/producers.md`.
