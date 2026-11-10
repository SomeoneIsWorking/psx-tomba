# 0055 — gameplay state producers still compose the pause screen

Features: F-FPS60.

Status: fixed (2026-11-10).

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

## Fix (2026-11-10)
G:80108B0C runs the page step G:8010810C after the draw. On page 1 it links the pause menu into the same OT: the dim
`FUN_8007E9C8(0x808080, 0, 4)`, then `FUN_801084F8`, which draws the menu through `FUN_8007EAE4` (font rows, cursor
`0x8007E998`, frame sprites; it reads only the cursor `0x800BF808`) before its input handling. While paused the
update block is skipped, so the draw's inputs do not move. `DrawSection` now replays the draw and then the page
step whenever the stage is GAME (`0x801FE00C == 0x8010637C`); every GAME caller of the draw (G:80108B0C,
G:80108BE4 and the submode frames 0x8010737C, 0x80107638, 0x801078CC, 0x801074D0) calls the page step next.
`DrawReplay` no longer forgets on `0x1F800136`. The orchestrators are overrides (`Render::registerOverrides`), so
guest callers reach `Render::frame`/`frameX` too: DEMO's attract frame (0x80106EBC, no page step) was composed
from producers and is now replayed.

Shown (unpaced, player settings): `scratch/pause-replay/f_pause` 27 paused in-betweens replayed, every present
of the menu identical to its neighbours; `io` the pause entry and exit, 224 of 224 in-betweens replayed with the
menu whole; `f_w0_0` village and `f_w1_0` cave halfway; `attract2` the attract demo halfway. The census
`c3` (boot unskipped, title, attract, warp, walk, pause, options, map) has no in-between without a state.

Deleted with it (state renders and their saves; the native overrides stay): RainStreaks, LineStrip,
GradientBackdrop, AreaMap, DustField, BackdropPlane, BackdropSprites, FogModelEmitter, FxSpriteAnchored,
FxSpriteSwarm, QuadRtptSubmit, HudGaugeEmitter, WidescreenMarginQuad, text_label, LitModelEmitter,
SwayModelEmitter, OverlayGt3Gt4, OverlayGroundGt3Gt4, `ObjectDraws` (A08 quarter-turn mesh, A0A/A0F hanging
strips) and `AnchorQuads` whole (its override only keyed and saved). WidescreenMarginQuad, DustField,
FogModelEmitter, BackdropPlane, QuadRtptSubmit and the model emitters keep their overrides: they own the
draw-window culls that extend the picture into the widescreen margins. Kept: `BillboardState` (the map screen
draws `0x8003C8F4` with the field draw off, census `c2`), and the boot, SOP, title and menu producers.
