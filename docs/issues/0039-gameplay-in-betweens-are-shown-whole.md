# 0039 — gameplay in-betweens are shown whole

Features: F-FPS60.

Status: partial (2026-10-23) — gameplay, the intro, the fisherman cutscene and the title compose every
in-between; the attract demo composes 1919 of 2556, the boot cards and two writers below still whole.

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

## The title and the attract demo (2026-10-23)
`scratch/interp-regression/judge6` measured boot 466 whole, title 107 whole, attract mostly whole
(`attract1`: 3177 whole of 3471), while intro, field, `warp 1` and `warp 8` composed every in-between.
`PSXPORT_PRODUCERS=1` in the attract (`attract2`) named the writers: `0x8013CDD4` (A00) 10073,
`0x80129114` (A01) 6129, `0x80132358` (A01 scenery walker) 2879, `0x8013E08C` (A00) 1064,
`0x8002847C` (`PackedMesh::emitScreen`) 900, `0x8012D9E8` (A01) 234, span-miss 1800.

Each got a producer whose state is the packets it links (`quad_state.h`, the `AnchorQuads` pattern).
`0x8013CDD4` (`WidescreenMarginQuad::emit`, already ported) and `0x8002847C` wrap their native bodies;
`0x80129114`, `0x8012D9E8` (A01 leaves) and `0x8013E08C` (A00 line leaf) are unported guest leaves keyed to
the node in a0; `0x80106690` (DEMO's two edge sprites) and `0x801311FC` (A01's per-frame effect) are guest
bodies that take no object and are keyed to one screen (`GuestKeyedEmitters`). The span-miss 1800 was
`0x80106690` itself. The DEMO overlay's `load_base` was missing from `tools/decomp/manifest.json`, which is
why that address read as unreferenced; it and the other non-MODE overlays now carry the slot they load at.

Result: `attract12` composes 1919 of 2556 in-between records (was 625 of 2556), and `judge7` (real speed,
8x, 3840x2160) shows title 108 of 108 composed (was 1 of 108), boot 162 of 467 (was 0 of 467), and intro,
field, `warp 1`, `warp 8` unchanged and fully composed. The census is down to `unkeyed attributed 0`.

Still whole in the attract: 116 records `object 0x8003C8F4/0x000EDD18 has no render or state` (a
`billboardEmit` object the state does not pair with) and about 200 records whose packet sits at the packet
page boundary (0x800BFFFC) and is written by `UnlitModelEmitter`'s bodies (0x8007FDB0) with no scope open.

Wrapping a *walker* in one scope does not work and was reverted: the A01 scenery walker `0x80132358` in a
single scope made 453 records whole for `0x8012F8D8` (the sway cue pair), because `EmitterObject` gives a
nested emitter call the *enclosing* scope's object, so several sway calls under one scope name one object
with several serials — ambiguous, hence no state. A body that calls produced emitters belongs to
`ObjectDrawScope` (one object, its emitter calls as numbered parts), never a bare scope.

Two earlier broad keyings were tried and REVERTED for the same reason: wrapping `Render::frame`/`frameX` in
one scope regressed `warp 1`/`warp 8` from fully composed to whole, and keying `HudGaugeEmitter::emitFrame`
to the gauge table left 5044 of 5512 keyed primitives with no state. The owner must be the emitter, and its
state must be the packets it links, not a whole-frame catch-all.

## The village and cutscenes (2026-10-23)
Free roam in the starting village (`scratch/interp-regression/field2`, `PSXPORT_PRODUCERS=1`) first composed 69 of
2298 in-betweens; `PSXPORT_DEBUG=compose` reported 1352 records shown whole for an unkeyed packet, and the census
blamed `0x8013E9D8 no producer unkeyed 5622`, `0x80122974 258`, `0x8003F174 159`, `0x80026864 84`
(`LetterboxBars::tick`), `span-miss 1839`.

`0x8013E9D8` (A00, the rope/anchor render fn) and `0x80122974` (A00, a quad/segment chain) both draw through the
guest `FUN_8013DD34` — a GT4-style textured-quad emitter from two 3-s16 node points — with no native body to
re-run, and `0x80039F4C` (`Render::textLabelEmit`) draws its per-character glyph quads the same way. These are
guest render fns, not worth porting, so they are keyed to their node and their state is the packets they linked
(`quad_state.*`): the render moves each quad's screen vertices by t. Fixed alongside: `LetterboxBars` saves its two
bars' height and re-emits them, keyed by its slot.

Result (`scratch/interp-regression/judge4`, real speed, 8x, 3840x2160; and `field6`/`cut1`): the village composes
every in-between except the boot cards, the intro 238 of 238, and `warp 1`/`warp 8` all of them; `warp 8` turning
moves ~8 px per present with each in-between halfway between its neighbours. The census is down to
`unkeyed attributed 0`, `span-miss 124`. The one remaining blocker is a textured sprite per frame (tpage 768,256,
CLUT 0x7FE8) whose writer the pool-address tools cannot pin down (the packet pool is reused every frame, so a
sealed record's `sourceAddress` names a different packet by the time it is queried).

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
static backdrop (DEMO 0x80106690's two sprites, unkeyed and still; fixed above).
