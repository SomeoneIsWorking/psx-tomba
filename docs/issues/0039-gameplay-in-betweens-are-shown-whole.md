# 0039 — gameplay in-betweens are shown whole

Features: F-FPS60.

Status: open.

## Observed
Real-speed walk from boot, 8x on a 3840x2160 sink, fps60 (`scratch/judder-check/walk-b3`, load 1.6 to 1.0):
`warp 1` moving and turning, 252 and 254 presents, 0 composed in-betweens; `warp 8` 3 and 30 composed of 126
and 127. The simulated 60.00 Hz display shows no repeats or drops, so every in-between repeats its real
present: gameplay moves at 30 Hz. The same counts come from a HEAD build without this pass's changes.

## Cause
The composer keeps a record whole when any primitive in it is unkeyed (psxport `frame_composer.cpp:index`).
`PSXPORT_PRODUCERS=1`, stepped `warp 1` turning: 22150 unkeyed primitives, 20839 of them under
`game/render/perobj_billboard.cpp:Render::perObjRenderDispatch`. Its effect leaves (`effect_mod.cpp`:
`effectColorAdd`, `effectClutSwap`, `effectSemiOn`/`Off`, `effectFlatTint`) rewrite the command word of every
packet `cmdListDispatch` just keyed, outside any emission scope, which unbinds the key; and a state render that
re-runs the emitters would not apply the effect either.

The rest of the queue (unkeyed primitives in the same runs), all writers with no producer:
`warp 1`: 0x800286CC 701 (from 0x8003C048), 0x80122974 252, 0x8013E9D8 180 (rope anchor), 0x80027A4C 90 (from
0x80027E5C), 0x80026864 82 (`LetterboxBars::tick`); `warp 8`: 0x80117628 8720, 0x80117E84 2128, 0x8002DBE4 612,
0x80027A4C 460.

## Fix (not done)
The effect is part of the object: `perObjRenderDispatch` has to apply it inside the node's scope and save what
it applied (the leaf and the node's tint, CLUT or blend), and the state render has to apply the same leaf over the
packets its emitters re-run. Then each remaining writer above needs a producer with a state render.

## Fixed on the way (2026-10-22)
The new-game intro showed 94 consecutive in-betweens whole: SOP `FUN_8010BF54` draws list 0x8010CC08 twice a
frame through `PackedMesh::write`, and both calls took the object `(0x80027768, list)`, so the state was
ambiguous. An unscoped `PackedMesh::write` is now its call site, n-th in the logic frame (`producers.md`). The
intro composes every in-between but the title's static backdrop (SOP 0x80106690's two sprites, unkeyed and still).
