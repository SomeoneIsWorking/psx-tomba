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

## Follow-up (2026-11-11)

**Page step, every caller by decompile.** A `jal` scan of every overlay and MAIN.EXE finds the draws' only
direct callers in GAME and DEMO (none in MAIN, SOP, OPN, START or A00-A0L, and no data word holds either
address); Ghidra C of each enclosing function (`external/psxport/tools/decomp_pipeline.py --image-name tomba2_game`
/ `tomba2_demo`, bodies present, into `scratch/review-findings/decomp/{game,demo}/c/`):

| caller (call site) | after `if (0x1F800136 < 2) draw();` |
|---|---|
| G:80107230 cases 1 and 5 (0x8010737C, 0x80107638), field draw | `FUN_8010810C(); 0x80077D8C(); 0x80075A80();` |
| G:80107230 case 3 (0x801074D0), room draw | the same |
| G:80107790 (0x801078CC), field draw | the same, then its submode exits |
| G:80108B0C (0x80108BB4), field draw | the same |
| G:80108BE4 (0x80108C7C), room draw | the same |
| D:80106E28 (0x80106EBC), field draw | `0x80077D8C(); 0x80075A80();`, no page step |

`0x80077D8C` (`Engine::postRenderTick`, the sfx/vibration queue) and `0x80075A80` (`AreaSlots::updateTail`) link
nothing, so the page step is the last OT writer after the draw. The native mirrors `Engine::fieldFrame` and
`fieldFrameX` follow `Render::frame`/`frameX` with `submitPage810c` the same way. G:80108CAC also runs the page step,
after `0x8010B328` rather than either draw, so it never reaches `DrawReplay`. GAME and DEMO share the stage slot
0x80106228 and DEMO ends at 0x80107723, so `DrawSection::pageStepResident` asks the image catalog whether GAME holds
G:8010810C instead of reading the stage pointer: the address is dispatched only while its own image is loaded.

**Census, corrected.** The `c3` census cited above never left the title: its `warp 0/0` was refused ("the warp
places are not loaded yet"), so it showed nothing about pause, options or the map. Rerun on a build with the
UnlitModelEmitter, TileGridLayer, PackedMesh and SubdividedGround state renders unregistered
(`scratch/review-findings`, unpaced, player settings, `debug compose,presentseq`):

- `cf`, boot unskipped to frame 3000 (boot cards, START, SOP intro, DEMO title): the SOP intro presents once per
  frame (`fields=2/1`, no in-between); the title's 403 in-betweens compose with no refusal.
- `cg2`, `cm`, `warp 0/0`, walk, pause, options, back, select: 2405 and 1633 in-betweens replayed, 0 refused.
- `co`, boot unskipped, start a new game (the scripted opening, SOP in GAME, then OPN): 79 opening in-betweens
  refused, `0x80027768` (`PackedMesh::write`, SOP 0x8010BF54) and `0x801099B4` (the SOP unlit GT3).

So the scripted opening is what still composes from these producers. Kept: the resident (0x8007FDB0/0x8008007C,
reached from SOP's `0x8003C048` walk) and SOP unlit copies, SOP's grid 0x8010C26C, `PackedMesh::write` and
`emitScreen` (MAIN). Removed, state render and per-call save: every area-overlay unlit copy (A01-A0J), the area
grids (A00, A06, A07, A0A, A0B, A0D, A0F) and A08's `PackedMesh::writeVOffset`. `co2`, the same opening run with
only the kept renders: 83 opening in-betweens composed, 0 refused. The resident emitters still save on every
gameplay call although only SOP frames compose them.

**SubdividedGround (A03 0x80110290) still to do, by its owner.** It is drawn only by the A03 drawer 0x801109D4 in
the replayed field draw, so it needs no state render. `game/render/subdivided_ground.cpp` carries another session's
uncommitted rewrite, so this change was left out of it. Its owner deletes `SubdividedGround::registerStateRenders`
(`subdivided_ground.h`, `subdivided_ground.cpp`) and its call in `tomba::Runtime::registerOverrides`
(`game/core/entry/runtime.cpp`), `SubdividedGroundRender`, and the `ListJobWriter` the native builds and saves per
call (its inputs: the records, `kSplitCount`, `kLeafCount`), keeping the `EmitterObject` scope that keys the
packets; then removes the `SubdividedGround` row from `docs/producers.md`'s state producers.

Shown on the committed tree (HEAD built from `git archive` without the uncommitted `subdivided_ground.cpp`):
village `warp 0/0` running right, each in-between halfway between its neighbours (`h_village/halfway.png`);
paused village, 8 presents identical, the menu whole, its in-betweens replayed (`h_pause`).

**SOP and title replayed.** `DrawSection` gained `Sop` and `Title` variants (plan, Boot and title), so the opening and
title in-betweens replay instead of composing. The SOP grid's state render and per-call save are deleted
(`TileGridLayer`); boot unskipped through title, attract, menu and new-game SOP (`scratch/replay-gaps/op7`): 1922
intervals replayed, 0 refused, 0 held. The resident and SOP `UnlitModelEmitter` renders and per-call saves, `PackedMesh::write`/`emitScreen`'s
and the DEMO edge sprites' wrapper are deleted with them. Census on that build (`scratch/replay-gaps/k1`, boot
unskipped, title, new game, SOP, OPN, into A00): 3301 intervals replayed, 0 refused, 0 dropped producers; the only
held in-betweens are the boot cards (logic 0) and four scene changes. `k2` (`warp 0/0`, walk, map, pause, options
page): 1883 replayed, 0 refused; 12 held on the pause options page, which is not a field draw.

**Menu pages replayed (2026-11-11).** A page `0x1F800136 >= 2` holds the field draw off; G:8010810C draws it alone
(case 3 options `0x8007B45C`, 4/8 card `0x8007BF20`, 5 quit `0x8007EF60`, 6 items `FUN_80108624` → `0x800346BC`,
10 `0x80121040`). `Render::pageStep` overrides it and selects `DrawSection::Variant::Page` with the page byte, so
items, options and the quit prompt replay instead of composing from Font `0x80078CA8` and Panel `0x8005019C`
(`scratch/pages`, unpaced, player settings): with every state render unregistered, `c2` composes nothing over items,
pause and the load screen, where `c3` (no Page variant) showed 1996 in-betweens whole with no keyed object. Idle
item menu, cursor step and load screen: every present identical to its neighbours (`fx1/{items,items2,load}`).

The card page leaked: its replay ran `TestEvent` (B0:0x0B) through the HLE, which cleared the card event the real
frame waited for, so the load screen's card machine advanced differently with in-betweens on (RAM at frame 1900 differs
in 32 bytes, "Checking MEMORY CARD" against "Select slot"). Fixed in psxport: a discarded run refuses at any host
service with state outside guest memory (`SideEffectJournal::admitHostService`, `Hle::biosCallKeepsHostState`).
The same plan with in-betweens on and off now ends in identical RAM, the same BIOS call counts (64 `TestEvent`, 64
`_card_info`), and 130 card-page in-betweens refused (`fx0`, `fx1`).

Still composed, so no producer is deleted by this: A03's cart frames (G:80108CAC runs A03's `0x8010B328` and the page
step, never the field draw, so `DrawReplay` is not entered) and the area-load transition (Panel `0x8005019C` right
after a warp; `scratch/pages/a03`). Their producers (Font, Panel, UiSprite, SubdividedGround and the A03 HUD
`0x80109EEC`) stay until those frames get a section.

**Cart ride and narration replayed; their producers deleted (2026-11-11).** The frames that still composed were
A03's cart ride and the first 29 frames of the new game's narration, not an area-load screen: the Panel that
composed after `warp 3/0` is the ride's intro dialog box.

- Ride: G:80108CAC (GAME's G:8010766C) runs A03's draw 0x8010B328 (`0x8004FD30`, `0x80025D98`, A03's HUD
  `0x80109EEC`, Tomba via `0x80109CE0`, `0x8003BCF4`, the ground walker `0x801109D4(0x800F2418)`, `0x8003C048`) in
  place of the field draw, then the page step. `Render::ride` overrides 0x8010B328 and selects `DrawSection::Variant::Ride`;
  the field draw's tracks (view, Tomba, node pools, records) cover it.
- Narration: `Sop::fieldUpdate` runs the crawl `NarrationScroller::kEntry` while its startup delay (sm+0x60) holds
  the draw tail off; that frame now selects `Variant::Narration`, the crawl alone.

Shown (`scratch/a03cart`, unpaced, player settings): the ride with in-betweens on and off ends in identical RAM and
scratchpad at frame 1500 (`dg0`, `dg1`); mid-ride crops `dg1/ride_halfway.png` (Tomba and the cart) and `r5/track.png` (the track's
bends) step between their neighbours; the cave wall moves within 3 pixels of halfway, Tomba's hair within 1.75 native
pixels of the screen midpoint (always above it; fixed below, ride entry). With every state render unregistered, nothing composes over the warps 0/0, 1/0, 4/0, 3/0 (`cw`), the ride
(`cr`), walk, pause, options and map (`cg`), items, pause and load (`cp`), boot to the title and attract (`cf`) or
the new game through SOP and OPN (`co2`).

Deleted: the state renders and per-call saves of `Font::glyphEmit`, `UiSprite::quads`/`compose`, `Panel::build` and
`SubdividedGround`, the A03 HUD wrapper `GuestKeyedEmitters::a03Hud`, and with them `ListStateProducer` and
`ListJobWriter`/`ListJob` (`list_job.h` keeps `ListCall`). The overrides stay for their keys, the widescreen clip and
the update-block notes the replay rebuilds. The final build's census, with the remaining state renders registered
(ScreenFade, BillboardState, GuestKeyedEmitters, DialogGlyphs), composes nothing in the same plans (`fw`, `dg1`,
`fg`, `fp`, `ff`, `fo`). The only in-betweens not replayed are the 130 of the card page, refused at its BIOS calls.

**Last state renders deleted (2026-11-11).** ScreenFade, BillboardState, DialogGlyphs and the GuestKeyedEmitters
renders and saves are gone, and with them `AnchoredPackets`, `quad_state.h` (`QuadStateRender`, `runGuestEmitter`),
`ObjectDraw`'s part states and `saveEmitterState`, `Session::stateProducers`/`renderMemory`, `Runtime::stateProducer`
and the emitters' host-memory branches. Tomba registers no state render; the overrides keep their scopes for the
owner ledger. Census on that build (`scratch/a03cart`, unpaced, player settings, `debug compose`): warps 0/0, 1/0,
4/0, 3/0 (`e2w`), walk, pause, options and map (`e2g`), items, pause and load (`e2p`, 130 card-page in-betweens
refused as before), boot to the title and attract (`e2f`), new game through SOP and OPN (`e2o`): no interval
composed. The one in-between after a GAME scene start is shown whole in `e2o` as it was in `fo`.

**Ride: Tomba's in-betweens sat a native line high (2026-11-11).** Cause: `game/core/frame/draw_motion.cpp` blended
the view MATRIX at 0x1F8000F8 with its translation straight, but the view's translation is -R·C; while the camera
turns and follows the cart, the straight translation puts an object moving with the camera 13-17 units (camera
space) above its chord, about one native line. psxport `TransformRule::Translation::View` moves the camera instead,
and the view track uses it. Separately, A03's 0x80109870 copies Tomba's pose matrix from his cart's node +0x98,
which no track blended, so his turn stepped at the logic rate; the node pools' +0x98 MATRIX is now tracked
(`node transforms`). Shown (`scratch/a03cart`, unpaced, player settings): Tomba's hair centroid in-betweens move
from 6-15 px (8x) above the midpoint (`h2`) to within 1.2 px (`h3`, `h3/ride_halfway.png`); the far track's bias
from -1.95 to +0.58 px; ride RAM and scratchpad identical with in-betweens on and off (`dg2`, `dg3`); village walk
in-betweens within 1.4 px of halfway (`v1/village_halfway.png`).
