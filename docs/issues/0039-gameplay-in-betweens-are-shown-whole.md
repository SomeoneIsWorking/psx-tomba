# 0039 — gameplay in-betweens are shown whole

Features: F-FPS60.

Status: partial (2026-10-23) — areas 1 and 8 compose every in-between; the village/cutscene areas still show
records whole, because the writers below have no producer.

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

## Open — the village and cutscenes (2026-10-23)
Free roam in the starting village (`scratch/interp-regression/field2`, `PSXPORT_PRODUCERS=1`) composes 69 of 2298
in-betweens; `PSXPORT_DEBUG=compose` reports 1352 records shown whole for an unkeyed packet, and the census
attributes them:
`0x8013E9D8 no producer unkeyed 5622`, `0x80122974 no producer unkeyed 258`, `0x8003F174 producer unkeyed 159`
(a producer whose own body still writes unkeyed packets), `0x80026864 no producer unkeyed 84`
(`LetterboxBars::tick`), `span-miss 1839`.

`0x8013E9D8` (A00, the rope/anchor render fn) calls `FUN_8013DD34` — a GT4-style textured-quad emitter from two
3-s16 node points (`node+0x14` record and `node+0x4e/50/52`) with a 16-entry colour ramp — and then
`Render::perObjRenderDispatch`. `FUN_8013DD34`'s packets are written outside any emission scope, so the whole
record is shown whole. Porting `FUN_8013DD34` as an emitter and keying `0x8013E9D8` (with a state render moving
the two points by t) is the remaining step, then `0x80122974`, `LetterboxBars` and `subPartWalk`'s unkeyed span.

## Fix (2026-10-23)
The effect is part of the object. `perObjRenderDispatch` now makes the node's effect an `ObjectEffect`
(`object_effect.h`) and `ObjectEffectScope` makes it the active one for the emission it wraps; each emitter call
saves it with its `ListJob` (`list_job.h`), and `ListStateProducer::render` re-applies it over the packets it
emits. `ObjectEffect::apply` rewrites each packet under its own key (`PacketAmendScope`, psxport
`emission_scope.h`), so the rewrite re-binds the colour word it shares the command with instead of unbinding it.

The remaining writers each got a producer and a state render:
`ObjectDraw`/`ObjectDrawRender` (A08 0x80117628's whole mesh-as-four-turns draw, one scope with its emitter
calls as numbered parts), `FxSpriteAnchored` (0x80027CB4 and the three siblings) and `FxSpriteSwarm`
(0x800281EC) over `ScaledSprites` (0x80027A4C), `LineStrip` (A08 0x80117E84), `AreaMap` (A02 0x80113628),
`DialogGlyphs` (0x8007CC00) and `Panel` (0x8005019C build + 0x8004FFB4 fill); the `ListJob` inputs also gained
`Lanes16` and `Camera` blends so a moved list or the scene camera moves by t.

Judged 2026-10-23 (`scratch/interp-regression/judge3`, real speed, 8x, 3840x2160, moving and turning in areas 1
and 8): area 1 and area 8 gameplay compose every in-between (`whole-in-between=0`); `warp 8` turning, the only
screen with clear camera motion, moves every present a sub-pixel step of ~8 px with each in-between landing
halfway between its neighbours; the new-game intro still composes its in-betweens.

## Fixed on the way (2026-10-22)
The new-game intro showed 94 consecutive in-betweens whole: SOP `FUN_8010BF54` draws list 0x8010CC08 twice a
frame through `PackedMesh::write`, and both calls took the object `(0x80027768, list)`, so the state was
ambiguous. An unscoped `PackedMesh::write` is now (call site, list, n-th call of that pair in the logic frame)
(`CallSiteObjects::object(returnAddress, list)`, `producers.md`); keying by call order alone would pair different
meshes when a site's set of lists changed between frames. The intro composes every in-between but the title's
static backdrop (SOP 0x80106690's two sprites, unkeyed and still).
