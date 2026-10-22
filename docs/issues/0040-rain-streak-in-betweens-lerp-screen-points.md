# 0040 — rain streak in-betweens lerp screen points

Features: F-FPS60.

Status: open.

## Observed
Code review, 2026-10-22: `rain_streaks.cpp:StreakStateProducer::render` draws an in-between streak from its two
saved screen points lerped by t (`ProjectedVertex::nearest`). The 60 fps rule is to lerp transforms and project
again; a lerped screen point leaves the projected path while the camera turns. A08 is not on the boot walk, so
no run has shown it on screen.

## Cause
`RainStreaks::draw` saves only the streak's two screen points (`StreakState`), not the drop's lattice vector or
the camera transform it projected through, so the render has nothing to project.

## Fix (not done)
Save the drop's `kDropVector` SVECTOR, its trail point and the GTE control after `SetTransMatrix`. The render
blends the control, projects the head again with RTPS, and keeps the guest's tail rule
(`head + 2 * (trail - head)`) with the trail lerped, since the guest itself keeps the trail in screen space.
