# 0071 — Camera shake in-betweens blend two jittered views

Features: F-FPS60.

Status: open. Owner: `game/core/frame/draw_motion.cpp` (the `view` track) with `game/camera/cutscene_camera.cpp`.

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

## Fix (not built)
The camera notes, per tick, the eye offset its view was built with (the jittered height minus the shake anchor
`cam+0x88`) and the anchor; a view rule blends the views rebuilt at the anchor and adds the newer tick's offset
through the same `lookAt`.
