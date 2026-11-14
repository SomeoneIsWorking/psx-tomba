# 0071 — Camera shake in-betweens blend two jittered views

Features: F-FPS60.

Status: closed (2026-11-14). Owner: `game/core/frame/draw_motion.cpp` (the `view` track) with `game/camera/cutscene_camera.cpp`.

## Reproduction
`warp 0/0`, stand still; the idle sequence's stomp (entry 9 of `0x800A4610`) shakes the camera for 12 ticks.
Unpaced, `ires=0`, `fps60=1`, `PSXPORT_PRESENT_SINK=3840x2160` (`scratch/s006p/plans/idle_seq.py`, `TRIG=9:72:shake:15`).

## Observed vs expected
The tree at the top left moves 0.9, -8.7, -16.8, -12.5, -7.4, 0.9, 9.3, 1.1, -7.0 sink px over consecutive presents;
every in-between sits within 0.8 px of its neighbours' midpoint (`scratch/s006p/shake_judge.txt`,
`shake_sheet.png`). The shake is random per tick, so the halfway picture shows a jitter neither tick drew. Expected:
the in-between draws the unshaken camera blended, plus the newer tick's shake.

## Cause
`CutsceneCamera::shakeTail` (FUN_8006C988) writes the eye height after this tick's `lookAt`, and the next tick's
`mainFollow` skips `heading` while the shake state is set, so its `lookAt` builds the view from the jittered eye
(tomba2-decomp `docs/notes/tomba-draw-transform.md`, Idle sequence). The `view` track blends the drawn views, which
carry the jitter; no unshaken view exists in guest memory to blend instead.

## Fix
`ShakenView` (`draw_motion`), a motion source: `CutsceneCamera::shakeTail` notes the eye it wrote and its offset from
the anchor `cam+0x86/88/8A`, and `CutsceneCamera::lookAt` notes the eye and target words it built the view from,
the view words, and the offset when the eye is still the one the tail wrote. At the draw it keeps the tick when the
drawn rotation is that look-at's, with what moved the translation after it (`mainFollow`'s dolly). An interval where
either tick carries an offset is rebuilt: the eye at the blended unshaken eyes plus the later tick's offset, the
target blended, then the guest look-at `FUN_8006D02C` in the replay, the dolly blended, and the eye and target words
put back as drawn (tomba2-decomp `docs/notes/tomba-draw-transform.md`).

Shown on the reproduction (`scratch/t2shake/a2`, judge `scratch/t2shake/shake_judge_after.txt`, crops
`scratch/t2shake/shake_sheet_after.png`): over the stomp's ticks every in-between sits within 0.8 sink px of the
later present (the unshaken camera stands still, so it is that tick's shake), against 5.1 to 8.4 px off it before
(`scratch/s006p/shake_sheet.png`); the first tick after the shake blends from the anchor toward the eye `heading`
eases back (20.9 between 16.4 and 23.6), and the ease after it blends at its midpoints.

Main RAM and scratchpad are identical with fps60 off and on at 870 pad frames of an idle boot over the turn and the
stomps (`scratch/t2shake/ram_identity.txt`).
