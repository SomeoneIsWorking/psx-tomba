# Tomba! 2 gameplay in-betweens on ReplayFrame

Moves Tomba! 2's gameplay 60 fps in-betweens from producer composition (`docs/producers.md`) to psxport's
replay route (`external/psxport/docs/plans/replay-interpolation.md`, `presentation.md` Interpolation, Replay):
the guest's own field draw re-run in a `GuestRedraw` at blended inputs. Spyro 1's
`titles/spyro1/core/frame/draw_section.*` in psx-spyro is the reference title.

Sources: `M:<addr>` = Ghidra C of MAIN.EXE, `G:<addr>` = GAME.BIN, `Axx:<addr>` = area overlay, all from
`external/psxport/tools/decomp_pipeline.py` into `scratch/replay-map/` (bodies present). `closure` = the
static `jal` closure over the Ghidra inventories of MAIN.EXE, GAME.BIN and each of A00-A0L
(`scratch/replay-map/closure.py`, `writers.py`): it follows direct calls only, so the `jalr` sites below are
not covered by it. "inferred" marks a claim no decomp line shows.

## The draw section

Per frame, `tomba::FrameDriver::stepFrame` (`game/core/frame/frame_driver.cpp`) does, in order:
`presentation.commit` (seals the record DrawOTag'd at the end of the previous frame), `devControl_.applyArmed`,
`Scheduler::step` (slots 0, 1, 2), `musicCoord.tick`, DrawSync, `tickSleepCountdown`, `finishPass`
(PutDispEnv, PutDrawEnv, `Engine::drawOTag(env + 0x1FFC)`, flip, ClearOTagR of the new buffer),
`FrameCut::notePassEnded`.

In gameplay slot 0 is the GAME stage, whose field state runs the field frame G:80108B0C
(`Engine::fieldFrame`, `game/core/engine/engine_object_leaves.cpp`):

1. frame counters `0x1F80017C` (u16) and `0x800BF878` (u32) +1;
2. when `0x1F800136 == 0` (not paused) the update block: `0x80059D28`, `0x80069B28`, `0x80026368`,
   `0x8007A904`, `0x80025588`, `0x8004FE84`, `0x80026C88`, `0x80022A80`, camera `0x8006EC44`
   (`CutsceneCamera::update`), `0x80050DE4`, `0x8001CAC0`;
3. when `0x1F800136 < 2` the field draw M:8003F9A8 (`Render::frame`, `game/render/render_frame.cpp`);
4. `0x8010810C` (`Engine::submitPage810c`), `0x80077D8C` (`postRenderTick`), `0x80075A80`.

**The section is M:8003F9A8**, eleven steps, none taking an argument but the scenery table:

| step | what it draws | reads |
|---|---|---|
| `0x8004FD30` | HUD gauge (`HudGaugeEmitter`) | gauge state |
| `0x80025D98` | HUD record `0x800ED058`: life/fruit sprites via `0x80025744` (`UiSprite::quads`), map `0x80113628` (A02) / `0x801140A0` (A07), page `0x80025B78` (DR_AREA + sprites) | `0x800ED058..`, `0x800BF87E/F`, `0x800BF870` (area), `0x800BF880`, `0x800BF816` |
| `0x8003BF00` | render list A: `count 0x1F80015C`, cursor `0x1F800154`, base `0x800F2738` | nodes by type byte +0xB |
| `0x8003EEC0` | linked list from `[0x800F2738]` (next +0x24) | nodes |
| `0x8003B588` | Tomba, node `0x800E7E80`: hit-flash colour, then `0x800597AC` (his part transforms), then `0x8003CCA4` | node, `0x1F800247` |
| `0x8003BB50` | render list B: `0x1F800144` / `0x1F80013C`, base `0x800F2410` | nodes; rope/chain `0x80122974`, shadow `0x8002AE0C` |
| `0x8003BCF4` | render list C: `0x1F800150` / `0x1F800148`, base `0x800F26C8` | nodes; per-area render fns |
| `0x8003D0BC(0x800F2418)` | the area's scenery walker by `0x800BF870` (A00 `0x801401B8`, A01 `0x80132358`, ... A0L `0x8010B0B8`) | static scenery tables |
| `0x8003F024` | effect table `0x800EC188`, 40 x 0x40, live while byte +0 != 0, each via `0x8003D23C` | effect records |
| `0x8003DF04` | tile-grid backdrop node `0x800ED018` by area (`TileGridLayer`) unless `0x800BF873` | grid node scroll words |
| `0x8003C048` | linked list from `[0x800F2624]` (next +0x24): text labels `0x80039F4C`, sub-part walks, per-area leaves, `QuadRtptSubmit` | nodes |

The room draw M:8003FA44 (G:80108BE4, `Engine::fieldFrameX`) is the same steps without `0x8003D0BC` and
`0x8003DF04`, `0x8003C048` before `0x8003F024`. It is the section's second variant.

Node layout as the steps read it (M:8003CCA4, M:8003CDD8, M:800597AC, M:8003C2D4): +1 live, +8 drawn command
count, +9 command count, +0xB draw type, +0xD path bits, +0x2C/+0x30/+0x34 position (s32, 16.16, the high
halfwords at +0x2E/+0x32/+0x36 are what the draw reads), +0x54/+0x56/+0x58 angles, +0x5A billboard angle,
+0xB8/+0xBA/+0xBC scale, render commands at +0xC0 + 4i. A render command: +8..+0xD part angles, +0x18..+0x29
world rotation (9 s16), +0x2C/+0x30/+0x34 world position (s32), +0x38..+0x3C part offset, +0x3F OT offset,
+0x40 model list.

### No preamble

Unlike Spyro 1 there is nothing to flip: the env pointer `0x800ED8C8`, its OT and the packet pool cursor
`0x800BF544` are already the current frame's when the section starts (`finishPass` flipped and cleared at the
end of the last frame, `rotatePacketPool` ran at this frame's top). `draw()` dispatches the steps (or the
orchestrator itself, through `GuestRedraw::dispatch` so every override stays in force) and returns
`[0x800ED8C8] + 0x1FFC`, the head `Engine::drawOTag` will walk.

### Chains

Gameplay walks one OT chain per frame, `dma2LinkedList(env + 0x1FFC)` (`Engine::drawOTag`,
`game/resident_natives.cpp`), plus whatever `nativePutDrawEnv` (`0x800815D0`, env + 0x2014) emits.
`linked(head)` is `head == base + p * 0x2070 + 0x1FFC` for parity p in {0, 1}, base `0x800E80A8`; the
DRAWENV packet sits at env + 0x2014 and is named out by that test.

The capture includes every packet linked into this frame's OT before the section ran (the update block's
writers, below): they are in the OT the replay starts from, so they come back at their real (t = 1) place.

### Devices, VRAM, yields

- **Yield**: `ChangeThread` `0x80080880` is reached only from the scheduler primitives `0x80051E60`,
  `0x80051F80`, `0x80051FB4`, `0x80052078`; `0x80051F80`'s callers are the CD loaders
  (`0x8001CE04..0x8001DB8C`), `0x800450BC`, `0x800452C0`, `0x800753D4`. None is in the section's closure in any
  area. The "can Scheduler::yield" comments on `Engine::fieldFrame` and `engine_scene_frame.cpp` are not borne
  out for `0x8003F9A8`; the yields on that path come from step 4.
- **CD, SPU, VSync**: none in the section's closure in any area. The CD path (`0x80074BC4` -> `0x8001CF2C` ->
  CdSync/CD_cw, and VSync) and the sfx queue `0x80074590` are reached from `0x8010810C` and `0x80077D8C`, after
  the section.
- **VRAM upload**: A04 only. `0x80116884` (A04) cycles a 16x1 CLUT row by LoadImage `0x80081218` when its
  countdown at `0x801442C8` expires; the closure reaches it from the section through `0x8003F698` and the A04
  model path (`perobj_dispatch.cpp` names `0x80116B14`, inside `0x801169E8`). A replay that reaches it is a
  device access and is refused: area 4 holds every interval whose replay steps that countdown to zero.
- **VRAM readback**: none. No StoreImage/MoveImage or GPU DMA queue call (`0x80082424`, `0x80082D04`,
  `0x80082FB4`, `0x80083364`) and no direct `0x1F801xxx` access is in the closure.
- **Indirect calls** the closure cannot follow: node function pointers at +0x18 (type 0x20, `0x8003EEC0`,
  `0x8003BB50`, `0x8003BCF4`, `0x8003C048`) and +0x7C (types 0x15..0x17, `0x8003BB50`, `0x8003BCF4`), A03
  `0x80113628`, `0x80041098` (A07, A0A). These are per-area render fns; the probe below is what proves them
  device-free.
- Library leaves reached: libgte and libgpu packet builders only (`0x80081CF8`, `0x80082240`, `0x800822D8`,
  `0x80083C30`, `0x80083DE0`) and the string leaves `0x8009A490`, `0x8009A5B0`, `0x8009A600` (text labels).

### Writers outside the section

Static callers of known OT writers that are not in the section's closure (some are also reached through the
node function pointers, so "outside" here means "has a caller outside", not "only outside"):

| writer | callers outside the section | phase |
|---|---|---|
| `0x8007E9C8` `ScreenFade` | `0x8002655C`, `0x80034548`, GAME `0x80106B98`, `0x80107AFC`, `0x8010810C` (pause page 1), `0x80108E58`, `0x80108EBC` | update, or step 4 (after) |
| `0x8007CC00`/`0x8007CDD4` `DialogGlyphs`, `0x8005019C` `Panel::build` | `0x8007D594`, `0x8007DC38`, `0x8007DDE0` (dialog stream), `0x800737F8`, `0x800738B0` | update (inferred: the dialog stream is an object behaviour) |
| `0x80078CA8` `Font::glyphEmit` | `0x80079324`, `0x80079374` | update or menus |
| `0x80026864` `LetterboxBars` | none static: a node function pointer or the update | inferred update |
| `0x8007E1B8`/`0x8007E6DC` `UiSprite` | 17 sites (`0x800262D8`, `0x800336DC`, ... `0x8007EAE4`) | update and menus |
| `0x80027768` `PackedMesh::write`, `0x80027A4C` `ScaledSprites`, FX sprites `0x80027CB4`/`0x80027E5C`/`0x800286CC` | the `0x80028xxx..0x80030xxx` effect leaves, `0x80033080`, `0x800328BC`, A01 `0x8012EC08`/`0x8012ED88`/`0x8012E5F8`/`0x8012E974` | node render fns (inside, via +0x18) or update |

What runs before the section is captured stepped (it moves at 30 Hz until it has a track or moves into the
section). What links into the OT **after** the section and before `finishPass` would be missing from every
in-between: step 4 only draws on menu pages (page 0, ordinary play, draws nothing: G:8010810C case 0), and
scheduler slots 1 and 2 run after slot 0. Whether a slot-1/2 task links packets during play is not shown
statically; the probe's identity check answers it.

## What moves between logic frames

| input | address / layout | kind | identity | read by | evidence |
|---|---|---|---|---|---|
| view rotation | `0x1F8000F8`, 5 words (9 s16 + pad) | Matrix (`gte::rotationBetween`) | single | every step, copied into GTE R | M:8003CDD8, M:8003C2D4, M:8003D23C; built by `CutsceneCamera::lookAt` (`kCamScratch + 40`) in the update |
| view translation | `0x1F80010C..0x1F800114`, 3 x s32 | Vector | single | every step (added to each object's RT) | same; `lookAt` `applyMatrixLV(.., 0x1F80010C)` |
| camera pitch, yaw | `0x1F8000F0`, `0x1F8000F2` (s16) | Angle12 | single | A0E `GradientBackdrop`, A06 `TileGridLayer::emitClamped` | `lookAt` (`kCamScratch + 32/34`), `docs/producers.md` |
| camera eye, target | `0x1F8000D2..0x1F8000E6` | Derived (not read by the section) | | | `lookAt` reads them in the update |
| Tomba position | `0x800E7EAC/B0/B4`, s32 16.16 | Vector | single | `0x800597AC` inside step `0x8003B588` | M:800597AC (+0x2E/+0x32/+0x36), `camera_state.h kMasterX` |
| Tomba angles | `0x800E7ED4/D6/D8` (+0x54..+0x58), s16 | Angle12 (x3) | single | `0x800597AC` | M:800597AC |
| Tomba part angles | each command at `[0x800E7F40 + 4i]` +8..+0xD, s16 x 3 | Pose or Angle12 per part (open 2) | command address | `0x800597AC` (`0x80085480(cmd + 8)`) | M:800597AC |
| Tomba command matrices | command +0x18..+0x34 | Derived (rebuilt by `0x800597AC` in the section) | | | M:800597AC |
| object command matrices | command +0x18..+0x29 rotation, +0x2C/+0x30/+0x34 position | Matrix + Vector as one ruled track | command address + `RenderRecordPool::object` life | `0x8003CDD8`, `0x8003F07C`, `0x8003F174` | M:8003CDD8; built in the update by `0x800597AC` from `0x80058648`, `0x8005950C`, `0x80060C60` (inferred update callers) |
| billboard nodes (type 0x10) | node +0x2E/+0x32/+0x36, +0x5A angle; particles under node +0x38 | Vector, Angle12; particle records open 3 | node + record life | `0x8003C2D4` -> `0x8003C8F4` | M:8003C2D4; `BillboardState` saves the corners |
| effect table | `0x800EC188 + 0x40i`, i < 40: +0 live, +7 model, +0xC scale, +0x1E/+0x22/+0x26 position s16, +0x28.. angles | Vector, Angle12, Scalar; +0/+7 Step | slot + model byte | `0x8003D23C` | M:8003F024, M:8003D23C |
| tile-grid scroll | node `0x800ED018`, two scroll words | wrapped (map size) | single | `0x8003DF04` area grids | `TileGridLayer` state (`docs/producers.md`) |
| scenery UV scroll, sway phase, A0E arena phase `0x800BF850` | per emitter, as `ListJob` inputs | wrapped / Linear | single per area | the area emitters | `UnlitModelEmitter`, `SwayModelEmitter`, `BackdropPlane` saves |
| HUD slide | `0x800BF87E`, `0x800BF87F` | Scalar or Step (open 4) | single | `0x80025744` | M:80025744 |
| texture clock | `0x1F80017C` (u16, +1 per field frame) | Step | | flipbooks, A0E `BackdropPlane` | G:80108B0C |
| hit flash | `0x800E7E8D` flags, `0x800E7E98..9A` colour, from `0x1F800247` | Step (draw-written) | | `0x8003B588` | M:8003B588 |

### Draw-side writes the sandbox must put back

| state | address | what the section does | evidence |
|---|---|---|---|
| render lists | `0x1F80013C..0x1F80015E` | each list step copies count and cursor to its saved pair and resets them when not paused | M:8003BF00, M:8003BB50, M:8003BCF4 |
| current node | `0x1F80028C` | set per node | M:8003CCA4 |
| Tomba flash | `0x800E7E8D`, `0x800E7E98..9A`, `0x800E7E88` | recomputed, `0x800E7E88` restored | M:8003B588 |
| Tomba transforms | command +0x18..+0x34, node +0x98..+0xB4 | rebuilt | M:800597AC |
| A04 CLUT countdown | `0x801442C8..` | decremented, upload at zero | A04:80116884 |
| packet cursor | `0x800BF544` | advanced | every emitter |
| scratch matrices | `0x1F800000..0x1F80007C`, `0x1F8000C0..C4` | temporaries | every step |

`GuestRedraw` restores guest RAM through its side-effect journal; whether that covers the scratchpad
`0x1F800000..0x1F8003FF` is to be confirmed in `guest_redraw.cpp` before the first replay (every list cursor
above lives there).

## Cuts and when the section does not run

- `tomba::FrameCut` (`game/core/frame/frame_cut.cpp`): stage pointer, area byte, sub-scene, intro shot, camera
  re-place, decided at `notePassEnded`, after that pass's draw already sampled and replayed. The presenter reads it
  through `sealedFrameIsCut` when it seals that pass's record S, and `cutBetween` holds the interval S'→S: S is
  shown as drawn and its replayed in-betweens are dropped. `DrawReplay::endFrame` forgets on the same flag, since
  S's sample may predate the change, so S→S+1 is held too (`NotReplayed`) and pairing starts again at S+1→S+2.
  A warp arrives through the game's own transition and is a scene change.
- Not a replayable frame, `forget()`: a frame whose field frame did not draw (`0x1F800136 >= 2` menu pages, load,
  card machine, SOP, the title menu, boot cards), a switch between `0x8003F9A8` and `0x8003FA44` or between stages,
  a savestate restore (`TitleState::restored`).

## Hazards

1. **Producer host state inside a replay.** `GuestRedraw` runs title overrides (`SideEffectMode::Discard`:
   services execute). Tomba's producers book host state on every call: `core.frameStates.save`, emission
   keys, `ObjectDraw` part numbers, and `Session::callSites` (`CallSiteObjects`) n-th-call counters, which
   `FrameDriver` resets once per frame. Three replays before the real draw shift the real draw's call-site
   objects and triple the saves. Owner: psxport `EmissionScope`/`FrameStates` must not book under a redraw,
   and `CallSiteObjects` must not count under one; or the producers the replay replaces are deleted first.
2. **No places, so no depth and no sub-pixel.** `gpu::decodePackets` is called with `.places = {}`
   (`gp0_draw_state.cpp`): replayed primitives carry no `GtePlace`, so they are whole native pixels with no
   view depth. Tomba's real records draw producer primitives with places and depth bands (A03 band bias, the
   depth test), so a replayed in-between would lose depth and snap to native pixels next to real presents that
   have both. That is judder by the workspace's own rule. Tomba cannot switch before step 1 of the framework plan
   (vertex places) lands.
3. **The presenter's switch is title-wide.** With `replayFrame` non-null, `FramePresenter::presentRecords`
   never composes producer states: an uncovered interval is held (`NotReplayed`). Tomba's SOP intro, DEMO
   attract, title, menus and the room draw compose today (issue 0039); switching gameplay alone would hold
   them all. Owner: `FramePresenter` composes from producer states for an interval whose S had no section run
   (as distinct from a refused replay) until the last producer of the title is deleted.
4. **A04 CLUT upload** (above): refused replays in area 4. Owner: `GuestRedraw` drops a GPU upload issued
   inside a replay (S's own draw uploads it, and the in-between is rastered onto S's VRAM), as the psxrecomp
   design drops device stores.
5. **Writers after the section** (slots 1 and 2): unknown; the probe's identity check finds them.
6. **Cost.** Each in-between re-runs the whole section with its producers; Spyro 1 measured about 36M host
   instructions a replay against 87M for draw and present. Tomba's producers do more per call (`ListJob`
   copies of record bytes), so hazard 1's fix is also the cost fix.

## Steps

0. **Probe (temporary, not committed).** In `Engine::fieldFrame`, before `rend(c)->frame()` when
   `0x1F800136 == 0`, run `0x8003F9A8` in one `GuestRedraw` with no tracks written and compare the captured
   chain with the real DrawOTag walk byte for byte, links included; log refusals. `warp 1/0`, `warp 8/0`,
   `warp 4/0`, a dialog and a cutscene. Answers hazards 4 and 5 and the indirect render fns. Same as the Spyro 1
   probe (psxport plan, Evidence).
1. Framework prerequisites: hazards 1, 2 (vertex places), 3, 4, and dynamic tracks (below).
2. `game/core/frame/draw_section.{h,cpp}`: `tomba::DrawSection` (the two variants, no preamble, `linked` as
   above) and `tomba::DrawMotion` (view, Tomba, command matrices, effects, scrolls). Held per Core in
   `tomba::Session`; `tomba::Runtime::replayFrame` returns it. `Render::frame`/`frameX` call `beforeDraw`
   under the gate above (they are the section's one entry); `FrameDriver::stepFrame` calls `forget` for a frame
   whose field frame did not draw, and on restore.
3. Delete the producers whose objects the replay draws, one family at a time, each judged on its screen.

**Dynamic tracks.** `MotionSet` takes a fixed span of tracks at fixed addresses. Tomba's moving records are
in pools reached through lists (render commands under live nodes in the four lists, effect slots), so the
set changes every frame and pairs by identity, not by position in a span. Spyro 1's moby array needs the same
(`psx-spyro/docs/plans/spyro1-motion-tracks.md`, Moby array). Owner: psxport `MotionSet`, a title-provided
source that lists `(identity, address, fields, rule)` at each sample; a record present in one sample only is
drawn at its later value. One implementation for both titles.

The command-matrix rule blends rotation with `gte::rotationBetween` (`present/gte_control.h`) and
translation by t; `ListJob`'s `InputBlend::Camera` (`game/render/list_job.h`) and `blendGteControl` already
do this for producer state and are the code to share, not copy.

## Proof

Settings and recipes as psxport's plan: `PSXPORT_IRES=0`, `PSXPORT_FPS60=1`,
`PSXPORT_PRESENT_SINK=3840x2160`, window unmapped, unpaced for composition, one paced run.
Adapt `psx-spyro/scratch/replay2/{run,pace,vblank,blink}.py` into `scratch/replay/`, reaching screens through the
`warp` control command (`game/core/debug/dev_warp.cpp`) and held buttons: `warp 1/0` walking and turning (village,
most object families), `warp 8/0` turning (A08 rain and the quarter-turn mesh), `warp 4/0` (A04 CLUT
cycling), a dialog, a cutscene. Each in-between halfway between its neighbours with sub-pixel motion, nothing
missing; `presentseq` advances 0.5 per simulated vblank; `replay:warn` 0; no `NotReplayed` rows in play.

## Open

1. Whether anything links into the OT after the section during play (slots 1 and 2): the probe.
2. Tomba's part angles (command +8..+0xD) come from the animation VM (`Animation::step`); blending them as
   angles may cross a keyframe switch. The VM's keyframe pair and progress layout is not mapped here.
3. The billboard particle records under node +0x38 and their identity (`BillboardState` saves projected
   corners, not the records).
4. HUD slide words `0x800BF87E/F`: who writes them and whether they ramp.
5. Whether the camera shake (`CutsceneCamera`, `kShakeRand` `0x8009A450`) needs a cut limit: shake moves the
   view by up to 16 units a frame and should blend, but a re-place must not.
6. The A03 (`0x80113050`) and A0C (`0x8011AD44`) closures name call targets with no body (`0x89B3FDE0`,
   `0x8CF3A880`); likely data decoded as `jal`, not checked.

## State (2026-11-10)

Built: `game/core/frame/draw_section.*` (both variants, one `GuestRedraw::dispatch` of the orchestrator; the 2D
writers' `CallSiteObjects` counts are put back after it), `draw_motion.*` and `draw_replay.*`, held by
`tomba::Session::drawReplay` and returned by `tomba::Runtime::replayFrame`. `Render::frame`/`frameX` call
`beforeDraw` and are the orchestrators' overrides, so every guest caller replays (GAME's submode frames, DEMO's
attract frame 0x80106EBC). In GAME the section also runs the page step G:8010810C, which links the pause menu after
the draw; a paused draw replays with its inputs held (issue 0055). A variant or stage switch, a frame that did not
draw, a declared cut and a savestate restore forget the earlier sample. Steps 0 and 1 resolved as: no refusal in any run (no device or
yield in the section, slots 1 and 2 linked nothing missing from the frames); hazard 1 is harmless for replayed
intervals (their states are never composed) and the call-site counts are restored; hazard 2's depth comes with the
captured word depths; hazard 3 is `ReplayFrame::replays`.

Tracks found by the frames: the 0x88 node pool holds the effect nodes (`render/fx_sprite_anchored.cpp`), whose
+0x2C/+0x30 are s16 halves and +0x34 a list or scale pair, so that pool moves only its halves; blending it as an
object node stretched its sprites across the screen.

Shown (scratch/replay, unpaced, player settings): village `warp 0/0` running right (Tomba's limbs and the pig in
its barrel halfway, the scene pans evenly), Area 1 cave `1/0` (Tomba pushing the fire block), A04 snow `4/0` and
A08 temple `8/0`, every in-between replayed. Paced village: 698 of 702 vblanks advance 0.5.

The render-record run (`0x800F2740`, 520 of 0x44) has no live marker: `FUN_8007AAE8` and `FUN_8007ADDC` only move
the free stack and leave the record's bytes. `RenderRecordPool` is the run's `MotionLiveness`: each sample marks the
records the free stack (`[0x800E7E74]`, count `0x800ED098`) names as free. Village `0/0` samples 441 live records,
cave `1/0` 286, with 283 and 285 paired as records come and go.

Open: a warp's cut lands on a pass whose load breaks the record chain, so its arrival
is held by the chain break before `cutBetween` is consulted; a cut with an unbroken chain (camera re-place
M:8006EA00) is not yet shown: its callers are all in area overlays (A00 0x80128ECC in the scripted state machine
FUN_80128D04, A06 0x8011B440/0x80126568/0x8012CF68/0x8013201C, A0A 0x8010B794, A0B 0x8010C218, A0C 0x80116C34,
A0D 0x8010B040, A0E 0x8010BA04, A0F 0x8010BF48) and no warp reaches one at arrival. No CLUT upload refusal was seen in A04 in 24 in-betweens (hazard 4 unproven either way); the effect table's
scale (+0xC, `Motion::Low16`) is tracked but no scaling effect has been shown halfway yet. Not tracked, stepping at
30 Hz: the HUD slide (0x800BF87E/F, writer not found), the tile-grid scroll (node 0x800ED018 +0x28/+0x2A wrapped by
the pitch +0x30/+0x32, `TileGridLayer`), A01's sway phase (0x80139004, `+= 0x20 + (rand >> 12)` in the draw walker
A01 FUN_80132358, a 12-bit angle) and billboard particles (node +0x38). A01's UV scroll (0x801388EC/0x801388EE) steps
0x40 texels every third frame inside that walker by design and is not blended; what the update block links before the
section is captured at t = 1 (dialog text, fades, letterbox). In play no producer state render runs any more
(census: `scratch/pause-replay/c3`, boot unskipped through title, attract, warp, walk, pause, options and map);
the state renders left serve SOP, the title menu, the pause menu's own pages and the map screen (`BillboardState`).
The gameplay producers are deleted (issue 0055).

Nothing leaks: one pad replay (`scratch/pause-replay/rec2/session.pad`, boot into the village, walk, jump, turn,
pause and unpause) run with fps60 off twice and on twice leaves main RAM and the scratchpad byte-identical at pad
frames 1000 and 1500 (`scratch/pause-replay/leak_*`, `ramdiff.py`); the on run replayed 1582 in-betweens.

Boot and title: the boot cards are stepped by design (`frame/boot_cards.cpp`). SOP draws through its own
orchestrator (SOP 0x801093A8 and 0x801098FC call the section step 0x8003C048 directly, not 0x8003F9A8), and the title
menu is DEMO 0x80106690/0x80106824, neither a field draw, so both still compose from their producers; replaying them
needs a SOP section and a title section of their own.
