# Repository codemap

Placement authority for both titles. This map does not report progress, goals, issues or evidence.
Execution order belongs in `docs/migration.md`; capability state in `docs/project-state.md`. Guest
address ownership is the last section, [Guest address index](#guest-address-index).

```text
CMakeLists.txt
├── cmake/tomba2_port.cmake              Tomba! 2 product composition
│   └── game/                            Tomba! 2 native title owners
├── titles/tomba1/cmake/tomba1_port.cmake
│   └── titles/tomba1/game/              Tomba! 1 native title owners
└── external/psxport/                    shared Lightrec executor and PSX platform
```

`external/psxport` is a live symlink to the workspace framework checkout; its layout and ownership
chains are described in `external/psxport/docs/codemap.md`. Nothing under it is title-owned.

---

## `game/` — Tomba! 2

One `Engine` per `Core`, reached through the `TombaCtx` aggregate (`game/core/entry/game_ctx.h`) with the
`eng()`, `rend()`, `fade()`, `rngOf()`, `trigOf()`, `mathOf()`, `mtxOf()`, `inv()`, `saveMenuOf()`
accessors. Guest state lives in `Core::mem_*`; the guest register file is `Core::r[]`.

### `game/core/` — application composition, the frame turn, and the override catalog

Split by concept. Each subdirectory owns one kind of work; the class owners did not change, only
where their bodies live.

| Subdirectory | Owns |
|---|---|
| `entry/` | Process entry and the title's own facts: `main` (composes `psx::host::ProductHost`), `TombaCatalog`, the boot-stub and MAIN.EXE identities, the MAIN.EXE handoff, `TombaRuntime`, the `TombaCtx` aggregate, the measured `GameConfig` table, and the `GameHooks` table. |
| `engine/` | The stage driver `Engine` and its seven parts: field run, frame ticks, task machine, object leaves, state dispatch, scene frame, and the `TaskSm` lens. |
| `frame/` | The per-frame turn: `TombaFrameDriver`'s transaction, the publisher cards (`BootCards`), the movie policy, the frame-rate decision (`FrameCadence`), and the after-frame probes (`FrameDiagnostics`). |
| `overrides/` | The one native-override declaration catalog and registration, plus the guest call conventions (`guest_jal`, `guest_resume`). |
| `debug/` | Developer control: dev warp (`DevWarp`), dev areas, `AutoDrive`, the title's control-channel commands, and `VerificationCounters`. |
| `hle/` | `LibapiIntr` — the guest's libapi interrupt-mask primitives. |
| `assets/` | `Asset` (LZ/texgroup/VRAM/stage preload) and `Str` (resident string leaves). |

| Class | Responsibility |
|---|---|
| `Game` (global; from psxport) | The framework's per-session aggregate: `Pad`, `Cd`, `Fmv`, `DbgServer`, `presentation`, `runtime`, `frameDriver`. |
| `TombaCtx` (`entry/game_ctx.{h,cpp}`) | The title's opaque per-`Core` subsystem aggregate; created by `createTombaContext`, reached from the framework as `Core::gameCtx`. |
| `TombaRuntime` (`entry/tomba_runtime.{h,cpp}`) | The one `GameRuntime`: boot (`bootInit`), the frame driver, the control-channel commands (`controlCommand`: `warp`, then the title's commands), the MAIN.EXE handoff in `registerOverrides`, renderer capability, temporal policy and widescreen declaration. |
| `tomba::title::measuredConfig()` / `hooks()` (`entry/title_facts.h`) | The measured Tomba!2 facts and the hook table, reachable by name from `TombaRuntime` and the tests. Replaces the deleted `tomba::legacy` shim, which psxport never named. |
| `TombaFrameDriver` (`frame/frame_driver.{h,cpp}`) | One pass of the retail main loop FUN_80050B08 per frame (`stepFrame`; loop prologue `enterLoop`, called by `bootInit`): input, timing, task scheduling, the guest's double-buffered present, exactly one presentation fence. Retail loop facts: `docs/re/frame-loop.md`. |
| `Engine` (`engine/engine.{h,cpp}` and `engine_*.cpp`) | The game's stage driver: field run, frame ticks, task machine, object leaves, state dispatch, `frameUpdate`, `drawOTag` (which hands a B-key bug report its ordering table: `captureBugReportReference`). |
| `FrameCadence` (`frame/frame_cadence.{h,cpp}`) | The one owner of the frame-rate decision, published into the guest's own quota byte. |
| `FrameCut` (`frame/frame_cut.{h,cpp}`) | Whether a sealed frame record is a cut (stage, area, sub-scene or SOP intro shot changed, or the camera re-placed by FUN_8006EA00); answers `TombaRuntime::sealedFrameIsCut`. |
| `FrameDiagnostics` (`frame/frame_diagnostics.{h,cpp}`) | Per-frame title state probes and counters reported after the frame. |
| `AutoDrive` (`debug/auto_drive.{h,cpp}`) | Title-aware stage/area automation applied at the frame boundary. |
| `LibapiIntr` (`hle/libapi_intr.{h,cpp}`) | libapi interrupt-mask primitives; mirrors the host VBlank count to the guest word at the title frame boundary. |
| `Asset` / `Str` (`assets/`) | LZ decompress, texture-group unpack, CPU→VRAM upload, the stage preload chain; and the resident native string leaves, registered image-aware. |
| `tomba::native::declareOverride` / `declareOverlayOverride` / `bindResident` / `activateOverlay` / `activateModeOverlay` / `activateAreaSlotOverlay` / `retireOverlay` / `loadAreaSlotFile` (`overrides/native_override_catalog.*`) | The one native-override declaration catalog; a declaration is keyed by image identity plus guest address, never by address alone. |
| `TombaCatalog` (`entry/tomba_catalog.{h,cpp}`) | The multi-title host's catalog: one entry, Tomba! 2, selected by the disc's boot executable SCUS_944.54. Tomba! 1 is not catalogued (its own binary still runs it). |
| `tomba::title::bootStubIdentity()` / `mainExecutableIdentity()` (`entry/tomba_identity.{h,cpp}`) | The two executables' size, SHA-256 and PS-X EXE header facts, built from the compile definitions `cmake/tomba2_port.cmake` reads out of `config/tomba2-images.json`. |
| `tomba::loadMainExecutable` (`entry/main_handoff.{h,cpp}`) | The stub-to-MAIN.EXE handoff: reads `\MAIN.EXE;1` from the disc, authenticates it against the manifest identity, loads it and sets the entry registers. |
| `BootCards` (`frame/boot_cards.{h,cpp}`) | The SCEA card and the LOGO movie that precede the first game frame, stepped one frame per `stepFrame` so the picker never blocks. |
| `MoviePolicy` (`frame/movie_policy.{h,cpp}`) | Whether a native movie plays (`PSXPORT_NO_FMV`, headless sink). One answer for the cards and Demo's OP.STR. |
| `TombaConfig` (`entry/game_config.cpp`) | The measured Tomba! 2 compatibility facts (guest addresses and sizes) the frame driver requires. |
| `VerificationCounters` (`debug/verification_counters.h`) | Title-owned verification tallies surfaced through the diagnostics channel. |
| `DevWarp` / `tomba::applyColdWarp` (`debug/dev_warp.h`) | Dev warp arming (`arm` from the control channel, `applyArmed` from the frame driver): the control-channel request applied at a frame boundary through the engine's own transition owners. |
| `StepReturn` (`engine/task_sm.h`) | The generic task state-machine vocabulary the engine's per-state dispatch is written against. |

### `game/input/`
`Engine::padEdgeFence` — the port of `FUN_800788AC`, the pad-edge fence that keeps one edge from
re-triggering within a frame.

### `game/ai/` — per-object and per-area behavior bodies

Namespace `tomba::ai` owns the zoned-attacker cluster; the older clusters keep their own feature
namespaces (`tomba::ai::actorbump`, `::actorcontact`, `::ropeswing`, `::swaynode`, `::tiltfollow`,
`::areaseq`, `::objcontact`, `::ropenode`, `::tiltpart`). Every `beh_*` file defines one static
member of `Behaviors` or one guest-ABI leaf; `Behaviors` (`game/ai/behaviors.h`) is the address-keyed
dispatch surface.

| Class | Responsibility |
|---|---|
| `Behaviors` | The per-object / per-area behavior handler surface; one member per handler, one file per member. |
| `tomba::ai::ActorZonedAttacker` | The A00-overlay zoned-attacker sub-behavior cluster (gate, type init, attack pick, approach, sub-state, idle, zone classify). |
| `ActorBump`, `ActorObjectContact`, `ActorMeleeEngage`, `MeleeProximity` | The contact-resolution behaviors: bump, object contact, melee engagement and its proximity test. |
| `AssemblyNode`, `AssemblyChild`, `AssemblyCompanion`, `AssemblyRider`, `RiddenAssembly` | Multi-part actor assembly: node/child structure, companions, and riders. |
| `AttackOrbitSubstate`, `ReleaseTriggerMotion`, `SubstateEdgeLeaves`, `PlacedPropSm` | Orbital attack, release-trigger motion, sub-state edge transitions, and placed-prop state machines. |
| `RopeSwing`, `SwaySchedule`, `TiltFollower` | Rope nodes and swing, sway schedules, and tilt followers. |
| `zoned_attacker_abi.h` | The zoned attacker's shared guest ABI vocabulary: register slots, leaf and global addresses, the two leaf-call shorthands, and the guest-stack frame contracts. |

### `game/object/` — guest object structure and the per-object dispatchers

| Class | Responsibility |
|---|---|
| `Actor` | The named-field lens over a guest object node. |
| `BehaviorDispatch` | The per-object behavior-handler dispatcher. |
| `ObjectList` | The per-frame entity-list walkers. |
| `Animation` | The per-object animation VM. |
| `Array8Dispatch` | The per-node array-8 handler dispatch. |
| `ActorReward` | The item/reward actor behavior. |
| `CubeTextLedger` | The cube-text banner's live text ledger. |

### `game/player/` — Tomba's actor and its collision

| Class | Responsibility |
|---|---|
| `tomba::player::ActorTomba` (`actor_tomba.{h,cpp}`) | The Tomba actor: a named-field view over the master guest block plus its owned per-frame logic — the G-block driver, the growth/motion/transition cascade, and the auxiliary-list walks that decide which item Tomba is standing on. |
| `tomba::player::ActorInteraction` (`actor_interaction.{h,cpp}`) | What touching an item means: the proximity/type-4/sub-hitbox checks that pick a touched item, and one leaf per guest item kind (interact mode, type 8, type 7). `ActorTomba`'s walks dispatch into it; it reads the G block through its owner and calls back through `ActorTomba::growthYSnap`. |
| `tomba_state.h` | The shared Tomba vocabulary both owners need: the G-block field lens (`TombaState`) and the guest addresses every Tomba subsystem keys off. |
| `Collision` | The collision-grid subsystem. |
| `tomba::targeting::ActorTargeting` | Whether the actor may acquire a target: near enough and in the right direction. |

### `game/world/` — field object population and geometry

| Class | Responsibility |
|---|---|
| `Spawn` | Entity spawn and despawn. |
| `Pool` | Object-pool and per-area control-block initialization. |
| `AreaSlots` | The area-slot state machine. |
| `ObjectTable` | The 40-slot fixed object table dispatcher. |
| `Placement` | Field object placement. |
| `Entity` | The per-object entity state machine. |
| `CollisionResolve` | Actor-vs-object cylinder collision resolve. |
| `GraphicsBind` | The object render-bind subsystem. |
| `tomba2::world::RenderRecordPool` (`render_record_pool.*`) | The render-record free stack FUN_8007AAE8 pops (count s16 0x800ED098, cursor 0x800E7E74) and each record's incarnation: every pop begins a new life, and `object(record)` is the producer object for it, so a record freed by one object and popped for the next never blends the two. Owned by `GraphicsBind` (`records`). |

### `game/scene/` — stage machines and loading

| Class | Responsibility |
|---|---|
| `Demo` | The DEMO / front-end menu stage state machine. |
| `Sop` | The SOP (intro cutscene) field stage machine. |
| `StartBinStage` | Task-0's disc file tables and the boot preloads for both execution models. |
| `ScriptInterp`, `tomba::scene::ScriptObject`, `ScriptOpcode` vocabulary (`script_opcode.h`, `script_globals.h`) | The cutscene script interpreter, its typed lens over the driven object, and its vocabulary. |
| `FieldTransition`, `BgSceneTransitionSm`, `AreaFadeSequencer`, `SceneTransition`, `TransitionState3` | The scene-transition machines: the sm[0x4a]==5 area fade, the SOP background fade, the A0L fade sequencer, the scene/sub-scene swap, and the mid-transition entity walk. |
| `SceneEvents` | The scene-event arm subsystem. |
| `ParallaxBg`, `parallax_scroll.h` | The SOP field-mode parallax background and its guest scroll reduction. |
| `ModeStateArm` | The mode-state arm primitive pair. |
| `tomba::stage::loadOverlay` (`level_load.h`) | Load START/DEMO/GAME into the shared stage slot, retire replaced generations, publish the loaded image. |
| `tomba::scene::stepCardLoadMachine` (`card_load_machine.h`) | The memory-card LOAD/SAVE page sub-machine and CRD image residency in the AREA slot. |
| `startup.cpp` | Engine startup/init, reimplemented from the game's own main entry point. |

### `game/audio/` — the sound driver's native leaves

Namespace `tomba::audio`. `Sequencer` is split by responsibility across
`sequencer.cpp` (tick + scheduler), `sequencer_channel_flags.cpp`, `sequencer_pitch_envelope.cpp`,
`sequencer_voice_write.cpp`, `sequencer_tone_records.cpp`, `sequencer_voice_alloc.cpp` and
`sequencer_voice_state.cpp`; each group declares its own overrides through a `declare*Overrides()`
entry point declared in `sequencer.h` and called from `Sequencer::registerOverrides`.

| Class | Responsibility |
|---|---|
| `Sequencer` | libsnd's per-VBlank sequencer: the tick wrapper, the sequence/channel scheduler, and the channel leaves it routes to. |
| `tomba::audio::record::ChannelRecord` | The typed lens over one per-(sequence, channel) record. |
| `tomba::audio::libsnd::*` (`libsnd_globals.h`) | The sound driver's global cluster, one named offset per word. |
| `Sfx` | The sound-FX trigger dispatcher. |
| `AudioDispatch` | The field audio dispatch/settle cluster. |
| `MusicCoord` | The per-field music coordination tick. |
| `MusicList` | The Sound Test catalogue and the in-game area BGM driver. |
| `NativeMusic` | The real-time SEP/VAB music player mixed into the SPU sink. |

### `game/camera/`

| Class | Responsibility |
|---|---|
| `tomba::camera::CutsceneCamera` (`cutscene_camera.{h,cpp}`) | The cutscene/free camera: follow modes, head/pitch/heading solve, distance and shake, and the per-mode motion the look builder's points feed. |
| `tomba::camera::LookAngleBuilder` (`camera_look_builder.{h,cpp}`) | The look-point/heading construction the camera solve is built from — yaw/distribution accumulation, the four point builders, the table joins, and the rotation/distance solve. A collaborator of `CutsceneCamera`, not part of it. |
| `camera_state.h` | The camera guest-memory lens (`CameraState`): the master/scratch blocks and the camera object slot, shared by the camera owner and its look builder. |
| `camera_guest_math.h` | The camera's shared guest math vocabulary — the fixed-point helpers and sign conventions every camera solve agrees on. |
| `tomba::camera::ModeDescriptor` / `RenderModePrologue` (`camera_mode.h`) | The camera driver's mode table, in one place. |

### `game/cd/` — libcd's guest-visible CD surface

| Class | Responsibility |
|---|---|
| `LibcdDirCache` | Writes libcd's guest directory/file cache from host ISO9660 sectors (`CdNewMedia`, `CdCacheFile`). |
| `LibcdNative` | The native wrapper around the substrate's libcd chain for the file-table build. |

### `game/items/`

| Class | Responsibility |
|---|---|
| `Inventory` | The item / inventory-collection subsystem. |

### `game/math/`

| Class | Responsibility |
|---|---|
| `Math` | The hot per-frame GTE-transform cluster: rotation-matrix compose and apply. |
| `GteTransform3` | The three-axis GTE transform. |
| `psx::gte` (psxport `gte_registers.h`) | GTE register numbers and command words; Tomba's emitters, the state producer (`list_state_producer.*`, over psxport `EmitMemory`, `GteControl` and `emitHostOrderingTable`) and `list_job.*` (psxport `StateWriter`/`StateReader`) share psxport's owners. |
| `Mtx` | libgte matrix helpers. |
| `Trig` | libgte trig helpers (rsin, rcos, ratan2, angle compare). |
| `Rng` | The LFSR pseudo-random generator (guest `FUN_8009A450`). |
| `Bit` (`mathlib.h`) | The bitmap flag bit-test primitives. |

### `game/ui/` — guest-time UI ports

Each class is a native body of a guest UI function and writes the same guest packets it does.

| Class | Responsibility |
|---|---|
| `Font` | Engine font/text initialization and the glyph leaves. |
| `Panel` | The panel fill quad (`FUN_8004FFB4`) and the dialog backdrop packet (`FUN_8007FCC8`). |
| `UiSprite` | UI sprite submission (`FUN_8007E6DC` family). |
| `OptionsPage` | The OPTIONS page backdrop packet (`FUN_8007FC24`). |
| `SaveMenu` | The save-menu handler dispatch. |
| `DialogTextStream` | The dialog typewriter stream. |

### `game/render/` — guest-time render ports

The picture is the guest's GP0 output, replayed from psxport's frame record; nothing here draws to
the host. Every class is a native body of a guest render function and writes the guest state (GTE,
scratchpad, packet pool, OT) that function writes. Those declared as producers key their packets for
60 fps interpolation (table below).

| Class | Responsibility |
|---|---|
| `Render` (`render.h`) | The per-Core umbrella for the render-walk, per-object, billboard, text-label, effect-modifier and libgpu ports. |
| `tomba2::render::OrderingTable`, `PacketPool` (`guest_ordering_table.h`) | The guest's OT and packet pool: the OT base global 0x800ED8C8 and pool cursor 0x800BF544 (and the register pages guest code addresses them from), the inlined AddPrim `link`, `chainToHead` for a prebuilt chain, the pool cursor and `allocate`, and the depth-to-bucket compression with its two gates (`inDepthRange` [4, 0x7FF]; `inDepthRangeExclusive` (4, 0x7FF), the A00 field GT3/GT4 pair only). Every native packet emitter in `render/` and `ui/` links and allocates through it; tested by `tests/test_ordering_table.cpp`. |
| `NodeXform` | The scene-node world-transform builder. |
| `ObjModelView` | The shared model-view setup leaf every effect-mesh draw runs first. |
| `Cull` | Visibility culling and LOD, with the widescreen re-include collection. |
| `tomba2::horizontal_cull::Visibility` (`horizontal_visibility_cull.h`) | The title's recovered horizontal visibility cull over the draw window, owned once; every native X gate goes through it. |
| `QuadRtptSubmit` | The two shared GTE quad-submit leaves. |
| `LibgpuDrawEnv` | The DRAWENV → DR_ENV compiler. |
| `ScreenFade` (`screen_fade.h`) | The fade leaf FUN_8007E9C8, native: the full-screen GP0 0x62 fill and its DR_MODE, the fill spanning the draw window. Every guest fade, flash and the pause dim goes through it. |
| `tomba2::render::LetterboxBars` (`letterbox_bars.h`) | The cutscene letterbox FUN_80026864: its height machine and the two black rects, drawn across the canvas margins. |
| `HudGaugeEmitter` | The HUD gauge DR_AREA emitter. |
| `tomba2::render::ModelPacket` (`model_packet.*`) | The steps every GT3/GT4 model list emitter shares: RTPT/RTPS with the GTE FLAG and NCLIP gates, the per-corner screen cull through `Visibility`, depth staging and the three depth modes (decoded by `depthOf`, `litDepth` or `flaggedDepth`), the near clamp, OT bucket, last UV, U scroll, DPCS shade and link; `emitModelList` walks a list, names each record `modelElement(list, index)` and advances the pool. `modelListJob` saves a list call as its state (see State producers). Tested byte-for-byte against the guest bodies by `tests/test_authentic_model_emitters.cpp`. |
| `tomba2::render::LitModelEmitter` (`lit_model_emitter.*`) | The lit GT3/GT4 list emitter of A01 and A05-A08: per-corner light falloff by DPCS. A01's copy adds two flag bits (hide while 0x1F80009C is set, 0x80 buckets deeper), A06's the hide on bit 2, selected by `FlagBits`. |
| `tomba2::render::UnlitModelEmitter` (`unlit_model_emitter.*`) | The unlit GT3/GT4 list emitter of MAIN.EXE, SOP, A01, A06 and A0A-A0J: one body differing only by data (`Variant`: scratch stage, colour masks, GT4 corner order, frame size, `DepthRule` (mode code, staging, near clamp, the flagged copy's GT4 drop), hide flag, depth cue, U scroll). |
| `tomba2::render::SwayModelEmitter` (`sway_model_emitter.*`) | A01's other two scenery pairs: vertex sway through guest rsin/rcos, UV scroll, and the depth-cue pair with its scratchpad LCG. |
| Packet emitters (`fx_sprite_anchored.h`, `fx_sprite_swarm.h`, `fx_sprite_publish.h/.cpp`, `tile_grid_layer.h`, `sop_ground.h`, `rain_streaks.h`, `objlist_walk.cpp`, `subpart_walk*.cpp`, `perobj_*.cpp`, `text_label.cpp`, `compose_tint_gate.cpp`, `effect_mod.cpp`, `overlay_gt3gt4.*`, `overlay_ground_gt3gt4.*`, `overlay_type_dispatch.cpp`, `render_walk_dispatch.cpp`) | One port per guest draw family; `FxSpritePublish` owns the sprite family's scratchpad handoff, scene camera load and anchor projection. |
| Widescreen (`wide_window.h`, `margin_render.h`, `widescreen_margin_quad.h`, `wide_re_*.cpp`, `horizontal_visibility_cull.h`) | The horizontal draw window (`drawWindow`: [-M, 320 + M) on the record path) and margin width (`marginColumns`), the margin re-include set, the margin-quad port, and the retained-W libgpu leaves. |

### Producers

A producer is a native override whose packet stores carry the key `(producer, object, element, part)`
(psxport `presentation.md`, Interpolation). The object is the drawn thing: a render command, a
sub-part, a scenery table slot or a map cell. Model emitters name each primitive as an element of that
object, `modelElement(list, index)` (`model_element.h`): GT3 or GT4 list and the index in it, so a
culled primitive does not shift its neighbours and two commands drawing one shared model
(`*(0x800ECF58 + type*4) + modelOffset`) stay apart. Declared through
`declareOverride`/`declareOverlayOverride`'s `producer` argument.

| Guest address | Image | Owner | Arg | Object |
|---|---|---|---|---|
| 0x8003CDD8 | MAIN | `Render::cmdListDispatch` | A0 | per render command (`node+0xC0+4i`); opened in the body, since `PerObjBillboard` calls it directly |
| 0x8003F174 | MAIN | `Render::subPartWalk` | A0 | per sub-part |
| 0x8003F07C | MAIN | `Render::sharedTransformWalk` | A0 | per sub-part |
| 0x801401B8 | A00 | `OverlayGroundGt3Gt4::entityLoop` | A0 | per scenery table slot |
| 0x80115598 | A00 | `TileGridLayer::emit` | A0 | the grid node (every tile sprite and the draw mode packet are one object) |
| 0x8010C26C | SOP | `TileGridLayer::emitSop` | A0 | the grid node |
| 0x801142EC, 0x801141B0, 0x80116B9C, 0x80116778 | A0A, A0B, A0D, A0F | `TileGridLayer::emitUnbiased` | A0 | the grid node |
| 0x80109FE0 | SOP | `SopGround::draw` | A0 | per ground block (`blocks + index*4`; the visible list is rebuilt each frame, the block is not) |
| 0x80078CA8 | MAIN | `Font::glyphEmit` | A3 | per glyph: its character's address (the string's own scope keeps the icon glyphs' packets) |
| 0x80116904 | A08 | `RainStreaks::draw` | A0 | per drop: its trail slot `0x801485E8 + 4i` (fixed seed, so drop i is one lattice point) |

### State producers

A state producer saves what it drew at each frame (`core.frameStates.save` under the object's scope) and has a
`render(from, to, t)` the composer runs for the object between two frames (psxport `presentation.md`, Frame
model). Registered by `registerStateRenders(Core&)` from `tomba_runtime.cpp`, keyed by the producer address.
Tomba's frame cuts stay `tomba::FrameCut`.

| Producer | Object | Saved state | Render |
|---|---|---|---|
| Every GT3/GT4 emitter address in the element-namer table below (`UnlitModelEmitter`, `LitModelEmitter`, `SwayModelEmitter`, `OverlayGt3Gt4`, `OverlayGroundGt3Gt4`) | the emitter call: `(emitter, the object that reached it)`, or `(emitter, record list)` from a scenery walker (`EmitterObject`, `list_job.h`) | a `ListJob`: the call registers, the pool cursor and arena size, the OT slot, the 32 GTE control registers, the record bytes and the guest words the body reads (scroll, LCG seed, light, sway phase) | `ListStateProducer`: the GTE rotation and translation registers and the sway phase move by t, then the emitter's own body runs over `HostMemory` (`EmitMemory`) and the packets it links are read from the host OT |
| 0x80115598, 0x8010C26C, 0x801142EC, 0x801141B0, 0x80116B9C, 0x80116778 `TileGridLayer` | the grid node | a `ListJob`: the node bytes with the two scroll words as wrapped inputs (modulus the map's pixel width and height) | the grid walk over host memory at the scroll moved the short way round the map; its draw mode packet gives the sprites their texture page |
| 0x80116904 `RainStreaks` | one drop, by trail slot | the OT slot and bucket and the two screen points of its streak | the two points move by t; the line is decoded from the same words and takes the DR_TPAGE's blend mode |
| 0x80078CA8 `Font::glyphEmit` | one glyph, by character address | the OT slot and bucket and the sprite's four command words | the position word moves by t; the string's texture page is the constant glyphEmit passes to SetDrawMode |

Not state producers: `SopGround::draw` 0x80109FE0, `Render::cmdListDispatch` 0x8003CDD8, `Render::subPartWalk`
0x8003F174, `Render::sharedTransformWalk` 0x8003F07C and `OverlayGroundGt3Gt4::entityLoop` 0x801401B8 open
scopes only; their packets come from the emitters above. A packet a scope's guest code writes itself stays as the
guest drew it. Keyed blend still runs for what is left keyed (psxport `FramePresenter::presentRecords`); see
issue 0033.

Element namers (not producers; they name elements of the innermost open object):

| Guest address | Image | Owner | Reached from |
|---|---|---|---|
| 0x8007FDB0 / 0x8008007C | MAIN | `UnlitModelEmitter` gt3 / gt4 | 0x800803DC via 0x8003F698 |
| 0x801316A8 / 0x80131BB0 | A01 | `LitModelEmitter` gt3 / gt4 | scenery walker 0x80132358; 0x80132DC0 via 0x8003F698 |
| 0x80130838 / 0x80130D9C, 0x8012F8D8 / 0x8013000C | A01 | `SwayModelEmitter` sway / scroll, cue pair | scenery walker 0x80132358 |
| 0x80132690 / 0x801329C4 | A01 | `UnlitModelEmitter` gt3 / gt4 (`kA01Cue`) | 0x80132DC0 via 0x8003F698 |
| 0x8013544C / 0x8013590C | A05 | `LitModelEmitter` gt3 / gt4 | 0x801362CC via 0x8003F698 |
| 0x8013C0D8 / 0x8013C5B4 | A06 | `LitModelEmitter` gt3 / gt4 (`kA06Flags`) | scenery walker 0x8013CD34; 0x8013D568 via 0x8003F698 |
| 0x8013BA44 / 0x8013BD40 | A06 | `UnlitModelEmitter` gt3 / gt4 (`kA06`) | scenery walker 0x8013CD34 |
| 0x8013CF00 / 0x8013D1E4 | A06 | `UnlitModelEmitter` gt3 / gt4 (`kA06Scroll`) | 0x8013D568 via 0x8003F698 |
| 0x8012CDF4 / 0x8012D2B4 | A07 | `LitModelEmitter` gt3 / gt4 | scenery walker 0x8012DA14; drawer 0x8012E1A0 |
| 0x801311D0 / 0x801313A0 | A07 | `OverlayGt3Gt4` gt3 / gt4 (A00's body) | drawer 0x8012E1A0 |
| 0x80112DEC / 0x80112FBC | A0L | `OverlayGt3Gt4` gt3 / gt4 (A00's body) | 0x8010B1B8 (A00's 0x80146478) |
| 0x801246A4 / 0x8012496C, 0x8012C7E0 / 0x8012CAA8 | A02, A07 | `OverlayGroundGt3Gt4` gt3 / gt4 (`kFarthest`) | scenery walkers 0x80124CB8, 0x8012DA14 |
| 0x8010AA4C / 0x8010AD40 | A0L | `OverlayGroundGt3Gt4` gt3 / gt4 (`kA0L`) | scenery walker 0x8010B0B8 |
| 0x80129BAC / 0x8012A06C | A08 | `LitModelEmitter` gt3 / gt4 | scenery walker 0x8012A7CC |
| 0x80140FBC / 0x801411D8 | A08 | `OverlayGt3Gt4` gt3 / gt4 (A00's body plus a UV scroll) | scenery walker 0x8012A7CC |
| 0x801465EC / 0x801467BC | A00 | `OverlayGt3Gt4` gt3 / gt4 | 0x80146478 via 0x8003F698 |
| 0x8013FB88 / 0x8013FE58 | A00 | `OverlayGroundGt3Gt4` gt3 / gt4 | `entityLoop` |
| 0x801099B4 / 0x80109C80 | SOP | `UnlitModelEmitter` gt3 / gt4 | `SopGround::draw` |
| 0x80112A24 / 0x80112CF0 | A0B | `UnlitModelEmitter` gt3 / gt4 (SOP's body) | not traced |
| 0x80113788 / 0x80113A54 | A0C | `UnlitModelEmitter` gt3 / gt4 (SOP's body) | not traced |
| 0x801103F4 / 0x80110698 and the same body in A0B-A0F, A0H-A0J | A0A-A0J | `UnlitModelEmitter` gt3 / gt4 (`kFlagged`) | not traced |
| 0x8010BC40 / 0x8010BF28 (A0G), 0x8010B3DC / 0x8010B6C4 (A0I), 0x8010AF58 / 0x8010B240 (A0H), 0x8010A3AC / 0x8010A69C (A0J) | A0G-A0J | `UnlitModelEmitter` gt3 / gt4 (`kSopNear`, `kSopNearWide`, `kSopNearLifted`) | not traced |

The scenery walkers (A01, A02, A05-A08, A0L) are guest code and open no scope; there an emitter call is its
own object (producer = the emitter, object = its record list, which is the table slot's).

An object keyed by a render record (`cmdListDispatch`, both sub-part walks) is the record's current life,
`RenderRecordPool::object(record)`: despawn pushes the record back on the free stack and the next spawn
pops it, so the next object on it gets another key.

Not yet producers, by census rank: SOP effect 0x8010BF54, A08 0x80117628 and 0x80117E84 (whole models
through FUN_8011731C / 0x80027768, whose primitives carry no element names yet), A01 0x80132DC0, UI
0x8007E6DC, billboard 0x8003C2D4, and the GT3/GT4 emitter copies of the overlays not listed above
whose bodies differ from the natives: A02 0x80124DB8/0x80125100, A03 0x80110094/0x80110290, A04
0x801175FC/0x801178A0 and 0x8013D0F8/0x8013D568, A05 0x8013484C/0x80134B74 and 0x8013AC90/0x8013AF0C, A07
0x8012DBB4/0x8012DE60, A0A 0x8010F97C/0x8010FD74, A0D 0x80112DE4/0x801131B0, A0E 0x80113AEC/0x80113DE0,
A0F 0x80114A5C/0x80114E70, A0G 0x8010C3A4/0x8010C664, A0K 0x80114F1C/0x8011562C and 0x801163E0/0x80116708
(issue 0028).

Each per-object drawer (A01 0x80132DC0, A05 0x801362CC, A06 0x8013D568, A08 0x8012A9DC) calls one GT3/GT4
pair per branch, so one sub-part never carries two lists of a kind (issue 0030).

---

## Who owns it

Each chain names the class and method at every hop.

### The frame turn

```text
psxport FrameLoopShell::step
  → binds the per-Core process globals (gte/projprim/spu/mdec/xa) once per session
  → tomba::TombaFrameDriver::stepFrame            frame/frame_driver.cpp; one pass of FUN_80050B08
      → Engine::cadence().beginLogicFrame         frame/frame_cadence.cpp (dwell counter = 0)
      → rotatePacketPool                          packet pool of the current parity
      → Game::hle.deliverEvent                    (psxport)
      → Pad::serviceFrame                         (psxport; writes the guest pad packet)
      → Engine::frameUpdate                       FUN_800788AC + per-vblank audio
      → Game::presentation.commit                 (psxport; exactly one fence; seals last pass's record)
      → OtAttr::beginLogicFrame                   packet spans reset after the census read them
      → PcScheduler::step                         FUN_80051E60, the guest task machine
      → guest DrawSync, PcScheduler::tickSleepCountdown (FUN_800506D0)
      → finishPass                                present state 0x1F80019C: PutDispEnv, PutDrawEnv,
                                                  Engine::drawOTag, parity flip, ClearOTagR
      → Engine::frameCut().notePassEnded          frame/frame_cut.cpp (scene identity of this record)
      → Engine::cadence().endLogicFrame
      → FrameDiagnostics::afterFrame, AutoDrive::afterFrame
  → gpu_present_frame_capture                     (psxport; once, after whichever presenter ran)
  → asserts the fence advanced exactly once
```

**While a movie or a blocking load runs.** Tomba's movies block: the boot movies
(`native_boot.cpp`, psxport) and `OP.STR` (`Demo`, `game/scene/demo.cpp`) call
`Fmv::play`, which owns the whole turn until the movie ends. Nothing else services the pad meanwhile,
so `Fmv::playToEnd` calls `Pad::pollHostInput` before every `Fmv::step`; that is the only way the
player's Start reaches the skip check, and the control channel does not answer until the movie ends.
A loading call runs inside the guest's own `Engine::frameUpdate`, inside the same `stepFrame`; there
is no separate loading turn and no loading-only screen.

### Host input → `Pad` → the guest pad buffer

```text
psx::input::HostInput                        runtime/psx/host_input.*  — the SDL drain, key state,
                                                                    gamepads, and the keyboard gate
  → Pad::pollHostInput                       runtime/psx/pad_input.cpp — the ONE host pump; also the
                                                                    P / '.' debug keys. It asks
                                                                    gpu_vk_windowed()
                                                                    (runtime/psx/gpu_vk.h) whether it
                                                                    is windowed or headless first.
      → Pad::serviceFrame                    resolves host / forced / REPL / replay into Pad::buttons,
                                             samples edges, records or replays, and writes the guest
                                             packet through Pad::fillBuffer
          → called once per logic frame from TombaFrameDriver::stepFrame
          → movie skip: Fmv::step reads Start held in Pad::buttons (psxport)
          → the debug channel's pause/step comes from Pad::pollHostInput
```

The title never reads host input itself: `game/input/pad_edge_fence.cpp` consumes the already-resolved
mask. Engine per-VBlank pad edges are `Engine::frameUpdate`'s work.

### Guest draw → presentation

```text
Render::frame / frameX                       render/render_frame.cpp — the guest render orchestrator;
                                               its walks reach the guest-time ports in game/render/,
                                               which write the packet pool and OT
  → OrderingTable::link / PacketPool::allocate render/guest_ordering_table.cpp — every native emitter's
                                               AddPrim and pool bump
Engine::drawOTag                             game/game_tomba2.cpp
  → gpu_dma2_linked_list                     (psxport) the guest OT goes to the GPU device as GP0
  → Game::presentation.commit                (psxport)
      → widescreen                            wide_window / cull / margin_render decide what the
                                               widened canvas shows; nothing is ever stretched
  → gpu_present_frame_capture                 once, at the tail of the turn
```

### CD and streaming

```text
Game::cd                                    runtime/psx/cd.* — the CD subsystem
  → CdcNative / CdcCommandPhase / CdPosition (psxport) — the drive state machine
  → StockReadLanding, XA streaming           (psxport)
  → title side: LibcdDirCache                game/cd/libcd_dir_cache.cpp — writes the guest's own
                                               directory cache from host ISO9660 sectors
             LibcdNative                     game/cd/libcd_native.cpp — the substrate libcd chain
             tomba::stage::loadOverlay       game/scene/level_load.cpp — START/DEMO/GAME
             tomba::native::activateModeOverlay / activateAreaSlotOverlay
                                               — publish a loaded image and bind its overrides
  → Game::cd.audioTrace                      the per-frame audio/CD phase trace the driver logs
```

### Audio

```text
Engine::musicCoord.tick                     game/audio/music_coord.cpp — coordinates the field's
                                               audio requests
  → Sfx                                       game/audio/sfx.cpp — the sound-FX trigger dispatcher
  → AudioDispatch                             game/audio/audio_dispatch.cpp — the field audio
                                               dispatch/settle cluster
  → MusicList / NativeMusic                   the Sound Test catalogue and the real-time SEP/VAB synth
  → Sequencer::frameTick                      game/audio/sequencer.cpp — libsnd's per-VBlank tick
                                               wrapper, then Sequencer::seqChannelDispatch, which
                                               routes each channel's flags bit to the matching leaf
  → Spu                                       (psxport) — the actual SPU voice state
```

### The debug / control channel

```text
lucent::http::Server                         (psxport) — the loopback listener, always open
  → psx::dbg::DbgServer                      runtime/psx/dbg_server.* — the command surface
      → framework commands                   (psxport) — memory, input, screenshot, render toggles
      → GameRuntime::controlCommand → tomba::TombaRuntime::controlCommand
          → warp                          DevWarp::arm on the TombaFrameDriver; other commands fall through to replCommand
          → GameRuntime::replCommand → tomba::TombaRuntime::replCommand
          → debug/repl_commands.cpp       the title's commands, which reach Tomba! 2 classes and
                                               guest layouts without the framework naming either
      → Debug warp / area selection          debug/dev_warp.* and debug/dev_areas.cpp — a
                                               request is armed and the frame driver applies it at a
                                               frame boundary through the game's own transition and
                                               load owners; it sets load mode 0x800BF89C = 4, leaving
                                               the scripted opening, before the load
```

---

## `titles/tomba1/` — Tomba! 1

Everything lives under `titles/tomba1/`; it reuses only the generic psxport executor and services and
never a Tomba! 2 address, overlay assumption, owner or renderer policy.

| Path | Responsibility |
|---|---|
| `game/app/` | The process-lifetime composition only. |
| `game/core/` | Startup, the frame transaction, CD/DMA, and the deliberately native behavior, declared by complete image identity. |
| `game/render/` | Executable-derived projection, visibility, edge coverage and 2D layout for widescreen. No native renderer, producers, depth, interpolation or 60 fps. |
| `tests/` | Tomba! 1's unit tests, including the 35-field CRT0 boundary and the widescreen projection. |

---

## Placement rules

| Concern | Owner |
|---|---|
| Lightrec decode/emission, CPU synchronization, bounded exits, image generations, invalidation, generic override/original-call dispatch, host input, CD/GPU/SPU/MDEC services, the frame loop shell, movies, presentation | shared `external/psxport` |
| A Tomba! 2 guest address, gameplay rule, object, scene, native producer, or override selection | the `game/` subsystem that owns that responsibility |
| Tomba! 2 per-frame input, timing, scheduler, render order, or the presentation boundary | `frame/frame_driver.cpp` |
| Tomba! 2 libetc VBlank guest address and callback write order | `hle/libapi_intr.*`, sequenced by the frame driver |
| Tomba! 2 stage-aware automation, guest-layout probes, title-specific REPL inspection | `debug/auto_drive.*`, `frame/frame_diagnostics.*`, `debug/repl_commands.cpp` |
| A Tomba! 1 guest address, runtime, gameplay rule, projection or layout policy | the matching `titles/tomba1/game/` subsystem |
| Offline guest-code emission, generated registries, interpreter-first gameplay selection | nowhere; these have no target owner |
| Epic intent | `docs/project-goals.md` |
| Capability coverage | `docs/project-state.md` |
| Atomic work | `docs/issues/` |

---

## Guest address index

> Hand-maintained guest-address to native-owner index. Keep it in step when an owner lands.

Before reimplementing any `FUN_xxxx`, look it up here, or grep the `tomba::native::declareOverride*` call sites.
A native may exist already. **LIVE** = reachable by a real call from either a native_boot
dispatch root or ordinary (non-native-tagged) game/engine code — free-function syntax
(`ov_foo(...)`), qualified static syntax (`Class::method(...)`), or C++ instance-call
syntax (`obj.method(...)`, `ptr->method(...)`, bare in-class `method(...)`). **ORPHAN** =
native exists but no call site of any of those forms was found anywhere in the tree — it
is genuinely dead code until something calls it.

Totals: 739 rows over 633 owned addresses, 731 LIVE / 8 ORPHAN. 261 override declaration sites.

**A row can come from a DEFINITION or from an INSTALL SITE.** An address whose handler is a file-local static in an anonymous namespace (no address in its name, no tag, no quoted registry name) has no findable definition — the `tomba::native::declareOverride` / `tomba::native::declareOverride*` call site is its only ownership record, and the file holding that call site is where you debug it from. Those rows say so in the summary column.

**This map answers WHERE code lives.** The picture is the guest's GP0 output on the GPU device; a render row here is a guest-time port that writes guest packets, never a host-side picture.

| addr | status | symbol | file:line | depends-on (still-PSX) | summary |
|------|--------|--------|-----------|------------------------|---------|
| 0x8001D364 | LIVE | `AudioDispatch::voiceFetchBits` | game/audio/audio_dispatch.cpp:54 | 0x8001D2A8 | AudioDispatch::voiceFetchBits — native ownership of FUN_8001D364 (Ghid… |
| 0x8001D71C | LIVE | `AudioDispatch::zoneTransitionSetup` | game/audio/audio_dispatch.cpp:111 | 0x8001CF2C 0x8001D2A8 | AudioDispatch::zoneTransitionSetup — native ownership of the tiny disp… |
| 0x8001F40C | LIVE | `CollisionResolve::classifyBodyContact` | game/world/collision_resolve.cpp:604 |  | ──────────────────────────────────────────────────────────────────────… |
| 0x8001F9DC | LIVE | `MeleeProximity::isAtApproachAnchor` | game/ai/melee_proximity.cpp:17 | 0x80084080 |  |
| 0x8001F9DC | LIVE | `MeleeProximity::isAtApproachAnchorFramed` | game/ai/melee_proximity.cpp:66 |  |  |
| 0x8001F9DC | LIVE | `MeleeProximity::registerOverrides` | game/ai/melee_proximity.cpp:103 |  |  |
| 0x8001FAE0 | LIVE | `ActorTargeting::tryAcquireTarget` | game/player/actor_targeting.cpp:97 |  | ORACLE: guest 0x8001FAE0 |
| 0x80020364 | LIVE | `ActorTomba::stepModeInteract` | game/player/actor_tomba.cpp:692 |  | postInteractWalk case 0xF/0x14/0x56 (mode=0) / 0x2F (mode=2). |
| 0x800205CC | LIVE | `ActorTomba::type8Interact` | game/player/actor_tomba.cpp:814 |  | postInteractWalk case 8. |
| 0x80022060 | LIVE | `ActorTomba::proximityCheck` | game/player/actor_tomba.cpp:338 |  | cylinder proximity + Y-band check. |
| 0x80022190 | LIVE | `ActorTomba::subHitboxCheck` | game/player/actor_tomba.cpp:393 |  | per-sub-hitbox collision variant. |
| 0x80022760 | LIVE | `ActorTomba::interactWalk` | game/player/actor_tomba.cpp:271 |  | ======================================================================… |
| 0x80022C78 | LIVE | `ActorTomba::growthYSnap` | game/player/actor_tomba.cpp:892 |  | leaf, no guest-stack frame. Operates on G (postFrameWaterCheck's |
| 0x800235A0 | LIVE | `ActorTomba::type7Interact` | game/player/actor_tomba.cpp:869 |  | postInteractWalk case 7. |
| 0x80023A04 | LIVE | `CollisionResolve::resolveByContactPolicy` | game/world/collision_resolve.cpp:794 |  | ──────────────────────────────────────────────────────────────────────… |
| 0x80023D48 | LIVE | `CollisionResolve::cylinderResolve` | game/world/collision_resolve.cpp:290 |  | ORACLE: guest 0x80023D48 |
| 0x8002423C | LIVE | `CollisionResolve::landOnObjectTop` | game/world/collision_resolve.cpp:497 |  | ──────────────────────────────────────────────────────────────────────… |
| 0x80024794 | LIVE | `interact_scan` | game/player/interact_scan.cpp:71 |  | (player) -> 1 if something was activated this call, else 0. |
| 0x80025588 | LIVE | `Engine::sceneEventFifo` | game/core/engine.cpp:670 |  | Native FUN_80025588 — the field EVENT/COMMAND-QUEUE state machine (str… |
| 0x800263E8 | LIVE | `Pool::seedAreaObjects` | game/world/pool.cpp:173 |  | area object-record seeding. Selects a per-area byte sequence (table 0x… |
| 0x80026470 | LIVE | `BgSceneTransitionSm::midTransitionGate` | game/scene/bg_scene_transition_sm.cpp:90 |  | Common guard shared by FUN_80026470/80026510/800264BC — three inline a… |
| 0x80026470 | LIVE | `BgSceneTransitionSm::audioStub26470` | game/scene/bg_scene_transition_sm.cpp:96 |  |  |
| 0x800264BC | LIVE | `BgSceneTransitionSm::audioStub264BC` | game/scene/bg_scene_transition_sm.cpp:106 |  |  |
| 0x80026510 | LIVE | `BgSceneTransitionSm::audioStub26510` | game/scene/bg_scene_transition_sm.cpp:101 |  |  |
| 0x8002655C | LIVE | `BgSceneTransitionSm::body` | game/scene/bg_scene_transition_sm.cpp:123 |  |  |
| 0x80026864 | LIVE | `LetterboxBars::tick` | game/render/letterbox_bars.cpp:45 | 0x8007FCC8 | cutscene letterbox, 8-slot array type 1; bars span the canvas margins |
| 0x80026C88 | LIVE | `ObjectTable::dispatch` | game/world/object_table.cpp:139 | 0x80026C88 |  |
| 0x80027254 | LIVE | `ObjectTable::handler27254` | game/world/object_table.cpp:43 |  |  |
| 0x80027768 | SUBSTRATE | — | — | 0x80027768 | The shared packed-mesh writer; runs as the guest body. |
| 0x80027CB4 | LIVE | `FxSpriteAnchored::emitUniformScale` | game/render/fx_sprite_anchored.cpp:59 |  | seaside hut-roof flames; projection via `FxSpritePublish` |
| 0x80027E5C | LIVE | `FxSpriteAnchored::emitByteScale` | game/render/fx_sprite_anchored.cpp:74 |  | weapon-impact burst sprite; projection via `FxSpritePublish` |
| 0x800281EC | LIVE | `FxSpriteSwarm::emitPerParticle` | game/render/fx_sprite_swarm.cpp:45 |  | per-particle flames; projection via `FxSpritePublish` |
| 0x8002918C | LIVE | `beh_rand_phase_cull` | game/ai/beh_rand_phase_cull.cpp:63 |  |  |
| 0x80029B40 | LIVE | `beh_pos_history_trail` | game/ai/beh_pos_history_trail.cpp:65 |  |  |
| 0x8002B278 | LIVE | `Cull::coneCullBody` | game/render/cull.cpp:265 |  | standalone view-CONE cull (3.9% field hot). a0 = node. The multiply-fo… |
| 0x8002B278 | LIVE | `Cull::coneCull2b278` | game/render/cull.cpp:286 |  |  |
| 0x8003116C | LIVE | `Spawn::spawnAndInitBody` | game/world/spawn.cpp:263 | 0x80028E10 | SPAWN-AND-INIT helper: spawn a type-6 object on list 1 (via the owned … |
| 0x8003116C | LIVE | `Spawn::spawnAndInit` | game/world/spawn.cpp:422 |  |  |
| 0x80031558 | LIVE | `Spawn::spawnEffectChild` | game/world/spawn.cpp:492 | 0x8007A980 | Spawn::spawnEffectChild. One of the near-identical MAIN.EXE "spawn a c… |
| 0x80031708 | LIVE | `ScriptInterp::refreshCachedTailHi` | game/scene/script_interp.cpp:1132 |  | ORACLE: guest 0x80031708 |
| 0x80031744 | LIVE | `ScriptInterp::refreshCachedTailLo` | game/scene/script_interp.cpp:1148 |  | ORACLE: guest 0x80031744 |
| 0x80031780 | LIVE | `Collision::listScan` | game/player/collision.cpp:247 | 0x80031780 | list-tail resolver / reset. Walks the 8-byte-stride linked list rooted… |
| 0x800318A0 | LIVE | `ObjModelView::composeIntoGte` | game/render/obj_model_view.cpp:151 |  | ORACLE: guest 0x800318A0 (tools/dynamic differential evidence equivale… |
| 0x80032A44 | LIVE | `Rng::inRange` | game/math/rng.cpp:13 |  | scaled random. Disas 0x80032A44..0x80032A84 verbatim: `sra v0, 15` on … |
| 0x80036DFC | LIVE | `SaveMenu::runHandler` | game/ui/save_menu.cpp:103 |  | ----------------------------------------------------------------------… |
| 0x80036DFC | LIVE | `SaveMenu::dispatchBody` | game/ui/save_menu.cpp:139 |  | ----------------------------------------------------------------------… |
| 0x80039F4C | LIVE | `ov_textLabelEmit` | game/render/text_label.cpp:154 |  |  |
| 0x80039F4C | LIVE | `Render::textLabelEmit` | game/render/text_label.cpp:159 |  |  |
| 0x8003AD48 | LIVE | `beh_cube_text_spawn` | game/ai/beh_cube_text_spawn.cpp:60 | 0x8003A790 0x8003A9A0 0x8003ABE4 0x8009A730 |  |
| 0x8003B054 | LIVE | `QuadRtptSubmit::rotateQuadCorners` | game/render/quad_rtpt_submit.cpp:42 |  | ──────────────────────────────────────────────────────────────────────… |
| 0x8003B054 | LIVE | `QuadRtptSubmit::registerOverrides` | game/render/quad_rtpt_submit.cpp:245 |  | Wiring (frontier, 2026-07-08): both leaves are reached only via direct… |
| 0x8003B220 | ORPHAN | `hitbox_build_3b220` | game/player/hitbox.cpp:52 |  | Pure native body. Mirrors the guest instruction path's exact in-memory… |
| 0x8003B320 | LIVE | `QuadRtptSubmit::submitQuad` | game/render/quad_rtpt_submit.cpp:131 |  | ──────────────────────────────────────────────────────────────────────… |
| 0x8003BB50 | LIVE | `Render::objListWalk1` | game/render/objlist_walk.cpp:96 | 0x8002AE0C 0x8003C5F8 0x8003C788 0x80122974 | ======================================================================… |
| 0x8003BB50 | LIVE | `ov_objListWalk1` | game/render/objlist_walk.cpp:594 |  |  |
| 0x8003BCF4 | LIVE | `Render::objListWalk2` | game/render/objlist_walk.cpp:243 | 0x80123C14 0x801341E8 0x80136748 | ======================================================================… |
| 0x8003BCF4 | LIVE | `ov_objListWalk2` | game/render/objlist_walk.cpp:597 |  |  |
| 0x8003BF00 | LIVE | `Render::objListWalk3` | game/render/objlist_walk.cpp:401 | 0x8003C5F8 0x8003C788 0x8004CC88 0x8010FC70 | ======================================================================… |
| 0x8003BF00 | LIVE | `ov_objListWalk3` | game/render/objlist_walk.cpp:600 |  |  |
| 0x8003C048 | LIVE | `Render::renderWalk` | game/render/render_walk_dispatch.cpp:154 | 0x80039F4C 0x8003C5F8 0x8003C788 0x8003EF9C 0x8003F174 0x800726D4 … |  |
| 0x8003C048 | LIVE | `ov_renderWalk` | game/render/render_walk_dispatch.cpp:275 |  |  |
| 0x8003C2D4 | LIVE | `Render::billboardCompose1` | game/render/perobj_billboard.cpp:316 |  |  |
| 0x8003C2D4 | LIVE | `ov_billboardCompose1` | game/render/perobj_billboard.cpp:667 |  |  |
| 0x8003C464 | LIVE | `Render::billboardCompose2` | game/render/perobj_billboard.cpp:357 | 0x800517BC |  |
| 0x8003C464 | LIVE | `ov_billboardCompose2` | game/render/perobj_billboard.cpp:670 |  |  |
| 0x8003C5F8 | LIVE | `Render::billboardComposeC5F8` | game/render/perobj_billboard.cpp:445 |  | ======================================================================… |
| 0x8003C5F8 | LIVE | `ov_billboardComposeC5F8` | game/render/perobj_billboard.cpp:676 |  |  |
| 0x8003C788 | LIVE | `Render::billboardCompose3` | game/render/perobj_billboard.cpp:404 |  | ======================================================================… |
| 0x8003C788 | LIVE | `ov_billboardCompose3` | game/render/perobj_billboard.cpp:673 |  |  |
| 0x8003C8F4 | LIVE | `Render::billboardEmit` | game/render/perobj_billboard.cpp:484 | 0x8003B054 0x8003B220 | ======================================================================… |
| 0x8003C8F4 | LIVE | `ov_billboardEmit` | game/render/perobj_billboard.cpp:679 |  |  |
| 0x8003CCA4 | LIVE | `Render::perObjRenderDispatch` | game/render/perobj_billboard.cpp:157 |  | ======================================================================… |
| 0x8003CCA4 | LIVE | `ov_perObjRenderDispatch` | game/render/perobj_billboard.cpp:664 |  | Engine/game natives installed into the per-Core image-qualified runtim… |
| 0x8003CDD8 | LIVE | `MarginRenderer::collect` | game/render/margin_render.cpp:13 |  | Record a re-include-eligible node (deduped within the frame). FILTER t… |
| 0x8003CDD8 | LIVE | `Render::cmdListDispatch` | game/render/perobj_dispatch.cpp:124 |  | per-object cmd-list dispatch: composes the WORLD object transform (cam… |
| 0x8003CDD8 | LIVE | `ov_cmdListDispatch` | game/render/perobj_dispatch.cpp:331 |  |  |
| 0x8003D0BC | LIVE | `Render::overlayTypeDispatch` | game/render/overlay_type_dispatch.cpp:70 | 0x8010AA20 0x8010B0B8 0x8010B5BC 0x8010BA40 0x8010C2A4 0x8011024C … |  |
| 0x8003D0BC | LIVE | `ov_overlayTypeDispatch` | game/render/overlay_type_dispatch.cpp:173 |  |  |
| 0x8003D584 | LIVE | `Render::effectColorAdd` | game/render/effect_mod.cpp:207 |  | modulate each colour channel by the node's per-channel amount, rather … |
| 0x8003EEC0 | LIVE | `Render::objListWalk4` | game/render/objlist_walk.cpp:503 | 0x8003B704 | ======================================================================… |
| 0x8003EEC0 | LIVE | `ov_objListWalk4` | game/render/objlist_walk.cpp:603 |  |  |
| 0x8003EF9C | LIVE | `Render::composeTintGate` | game/render/compose_tint_gate.cpp:48 | 0x8003D584 0x8003F07C | ORACLE: guest 0x8003EF9C |
| 0x8003F07C | LIVE | `Render::sharedTransformWalk` | game/render/subpart_walk_shared.cpp:38 | 0x8003F698 | ORACLE: guest 0x8003F07C |
| 0x8003F174 | LIVE | `Render::subPartWalk` | game/render/subpart_walk.cpp:44 | 0x8003F698 | ORACLE: guest 0x8003F174 |
| 0x8003F344 | LIVE | `Render::effectClutSwap` | game/render/effect_mod.cpp:179 |  | stamp the node's CLUT id onto every colour-bearing packet, repointing … |
| 0x8003F3F4 | LIVE | `Render::effectSemiOn` | game/render/effect_mod.cpp:164 |  | turn semi-transparency ON for every colour-bearing packet in the span. |
| 0x8003F4C4 | LIVE | `Render::effectSemiOff` | game/render/effect_mod.cpp:171 |  | the exact inverse: turn semi-transparency OFF. |
| 0x8003F594 | LIVE | `Render::effectFlatTint` | game/render/effect_mod.cpp:189 |  | overwrite the packet's colour word(s) with one flat colour and force s… |
| 0x8003F698 | LIVE | `Render::resolvePerModeEmitter` | game/render/perobj_dispatch.cpp:290 |  | WHICH GUEST EMITTER a cmd with this `flag` resolves to — the ONE encod… |
| 0x8003F698 | LIVE | `Render::perModeDispatch` | game/render/perobj_dispatch.cpp:316 | 0x800803DC | per-mode render dispatcher: routes to the area's per-mode renderer (mo… |
| 0x8003F698 | LIVE | `ov_perModeDispatch` | game/render/perobj_dispatch.cpp:334 |  |  |
| 0x8003F9A8 | LIVE | `Render::frame` | game/render/render_frame.cpp:16 |  | per-frame render orchestrator. The render-queue WALK passes (0x8003bf0… |
| 0x8003FA44 | LIVE | `Render::frameX` | game/render/render_frame.cpp:28 |  | mid-transition render orchestrator twin (reduced pass set). Same rule:… |
| 0x8003FD10 | ORPHAN | `osc_fd10` | game/world/entity.cpp:46 |  | per-object OSCILLATE / FRAME-TOGGLE sub-behavior (PlacedPropSm STATE-1… |
| 0x80040558 | LIVE | `PlacedPropSm::step` | game/ai/placed_prop_sm.cpp:142 |  | ORACLE: guest 0x80040558 |
| 0x80040A58 | LIVE | `SceneEvents::classSize` | game/scene/scene_events.cpp:43 |  |  |
| 0x80040AA4 | LIVE | `CubeTextLedger::spawnPopup` | game/object/cube_text_ledger.cpp:92 |  |  |
| 0x80040B48 | LIVE | `SceneEvents::armBody` | game/scene/scene_events.cpp:71 |  |  |
| 0x80040B48 | LIVE | `SceneEvents::arm` | game/scene/scene_events.cpp:116 |  |  |
| 0x80040B48 | LIVE | `SceneEvents::armOverride` | game/scene/scene_events.cpp:127 |  | override entry (guest ABI: slot in r4, ret in r2). Single canonical bo… |
| 0x80040C00 | LIVE | `CubeTextLedger::deactivateSlot` | game/object/cube_text_ledger.cpp:64 |  |  |
| 0x80040CDC | LIVE | `ScriptInterp::init` | game/scene/script_interp.cpp:110 |  |  |
| 0x80040DE0 | LIVE | `ScriptInterp::loadCurrentEntry` | game/scene/script_interp.cpp:134 |  |  |
| 0x80040E54 | LIVE | `ScriptInterp::loadNextEntry` | game/scene/script_interp.cpp:313 |  | loadNextEntry(obj, kindArg): THE ENTRY ADVANCE. 1:1 with guest 0x80040… |
| 0x80040FA0 | LIVE | `ScriptInterp::advanceStep` | game/scene/script_interp.cpp:424 | 0x80040E54 | VERIFIED + WIRED (frontier tier, 2026-07-10; advanceEntry() now calls … |
| 0x80041098 | LIVE | `beh_script_interp_step` | game/scene/script_interp.cpp:554 |  |  |
| 0x80041098 | LIVE | `ScriptInterp::step` | game/scene/script_interp.cpp:559 |  |  |
| 0x800412CC | LIVE | `ScriptInterp::callFnptr` | game/scene/script_interp.cpp:510 |  |  |
| 0x8004139C | LIVE | `ScriptInterp::stepAngleToward` | game/scene/script_interp.cpp:679 |  | leaf angle-stepper (no guest frame). See script_interp.h for the seman… |
| 0x80041438 | LIVE | `ScriptInterp::turnFacing` | game/scene/script_interp.cpp:710 |  | thin wrapper: turnFacing(obj, targetAngle, step) = stepAngleToward(obj… |
| 0x80041438 | LIVE | `ScriptInterp::turnFacingFramed` | game/scene/script_interp.cpp:716 |  | Guest-ABI twin — mirrors FUN_80041438's own sp-=24 / ra-spill-at-+16 f… |
| 0x80041468 | LIVE | `ScriptInterp::op31TurnTowardTarget` | game/scene/script_interp.cpp:976 | 0x80085690 | op31 — FUN_80041468 (opcode table index 31). See script_interp.h for t… |
| 0x8004190C | LIVE | `Engine::animTick` | game/core/engine.cpp:1172 |  | Engine::animTick — FUN_8004190C. Ticks the animation VM (native |
| 0x8004201C | LIVE | `ScriptInterp::op04SceneFlagRendezvous` | game/scene/script_interp.cpp:258 |  | the SCENE-FLAG RENDEZVOUS opcode (table index 4). 1:1 with authenticat… |
| 0x80042090 | LIVE | `ScriptInterp::op05WaitFrames` | game/scene/script_interp.cpp:208 |  | VERIFIED + WIRED (frontier tier, 2026-07-10; return-value fix 2026-07-… |
| 0x800420AC | LIVE | `ScriptInterp::op06TestSceneFlag` | game/scene/script_interp.cpp:217 |  | VERIFIED + WIRED (frontier tier, 2026-07-10). 1:1 with authenticated e… |
| 0x80042170 | LIVE | `ScriptInterp::matchesActiveByKind` | game/scene/script_interp.cpp:1168 |  | ORACLE: guest 0x80042170 |
| 0x80042258 | LIVE | `SceneEvents::delayedTrigger` | game/scene/scene_events.cpp:133 |  | ORACLE: guest 0x80042258 |
| 0x80042258 | LIVE | `SceneEvents::delayedTriggerOverride` | game/scene/scene_events.cpp:199 |  |  |
| 0x80042310 | LIVE | `ActorTomba::resetLoadGate` | game/player/actor_tomba.cpp:1144 |  | resetLoadGate — guest FUN_80042310. See actor_tomba.h for the full RE … |
| 0x80042448 | LIVE | `SceneEvents::applyFlagOp` | game/scene/scene_events.cpp:173 |  | ORACLE: guest 0x80042448 |
| 0x80042448 | LIVE | `SceneEvents::applyFlagOpOverride` | game/scene/scene_events.cpp:202 |  |  |
| 0x80042728 | LIVE | `BgSceneTransitionSm::readyForProgress` | game/scene/bg_scene_transition_sm.cpp:277 |  |  |
| 0x80042758 | LIVE | `BgSceneTransitionSm::opSceneEventArmWait` | game/scene/bg_scene_transition_sm.cpp:290 | 0x80040B48 0x80042728 | - Cutscene-script opcode leaves (adjacent to readyForProgress in the g… |
| 0x80042884 | LIVE | `BgSceneTransitionSm::opClearSceneFlag80a` | game/scene/bg_scene_transition_sm.cpp:359 |  | opClearSceneFlag80a (FUN_80042884) — one-shot opcode leaf: clear the s… |
| 0x80042E10 | LIVE | `ScriptInterp::op34ClaimGate` | game/scene/script_interp.cpp:395 |  | VERIFIED + WIRED (frontier tier, 2026-07-10; §9 re-verify caught+fixed… |
| 0x80042EA4 | LIVE | `ScriptInterp::stepEventPulse` | game/scene/script_interp.cpp:731 |  | see script_interp.h for the full semantics writeup. |
| 0x80042EA4 | LIVE | `ScriptInterp::stepEventPulseFramed` | game/scene/script_interp.cpp:765 |  | Guest-ABI twin — mirrors FUN_80042EA4's own sp-=24 / ra-spill-at-+16 f… |
| 0x80043108 | LIVE | `ScriptInterp::op36MoveTowardScriptTarget` | game/scene/script_interp.cpp:783 | 0x80084080 0x80085690 | op36 — FUN_80043108 (opcode table index 36). See script_interp.h for t… |
| 0x80044090 | LIVE | `ScriptInterp::mirrorGlobalStatusByte` | game/scene/script_interp.cpp:1184 |  | ORACLE: guest 0x80044090 |
| 0x80044BD4 | LIVE | `Demo::s0PreYield` | game/scene/demo.cpp:665 |  |  |
| 0x80044BD4 | LIVE | `FieldTransition::areaLoadBd4` | game/scene/field_transition.cpp:36 |  | Native replacement for FUN_80044bd4(0x800452c0, area, mode, 1): seed t… |
| 0x80044BD4 | LIVE | `Sop::transitionAreaEnter` | game/scene/sop.cpp:168 |  | Synchronous TRANSITION area-DATA load — replaces the cooperative |
| 0x80044BD4 | LIVE | `StartBinStage::advanceWithBootPreload` | game/scene/start_bin_stage.cpp:71 |  | native_sync only — the pc_faithful body splits these writes across the… |
| 0x80044D8C | LIVE | `Asset::lzDecompress` | game/core/asset.cpp:33 |  |  |
| 0x80044E84 | LIVE | `Asset::unpackGroup` | game/core/asset.cpp:78 | 0x80080F6C | PC-owned texture-group unpacker — replaces guest FUN_80044E84 (0x80044… |
| 0x80044E84 | LIVE | `Asset::unpackGroupFaithful` | game/core/asset.cpp:147 | 0x80080F6C 0x80081218 | FAITHFUL texture-group unpacker — FUN_80044E84 with full guest-stack d… |
| 0x80044F58 | LIVE | `Asset::loadTexgroup` | game/core/asset.cpp:232 | 0x8001DC40 | PC-native TEXTURE-GROUP LOADER — owns the asset-load ORCHESTRATION FUN… |
| 0x80044F58 | LIVE | `Asset::preloadTexgroup` | game/core/asset.cpp:327 |  | texture-group load, synchronous. (Mirrors loadTexgroup but driven by e… |
| 0x8004514C | LIVE | `Asset::preloadStage1` | game/core/asset.cpp:418 |  | the stage-1 callback body. SWDATA + DAT load, shared texgroup sub-load… |
| 0x8004514C | LIVE | `Asset::preloadStage1AsTask` | game/core/asset.cpp:445 | 0x8001DC40 0x800754F4 | Task-1 body — FAITHFUL FUN_8004514C, run on a PcScheduler native fiber… |
| 0x80045258 | LIVE | `Asset::loadDescriptorChunk` | game/core/asset.cpp:620 |  | loadDescriptorChunk(descIdx, slot): FAITHFUL FUN_80045258 — a leaf ind… |
| 0x800452C0 | LIVE | `Asset::areaDataLoadAsTask` | game/core/asset.cpp:498 | 0x8001CF2C 0x8001DC40 0x80045080 0x80051F80 0x80051FB4 0x8007566C | Task-1 body — FAITHFUL FUN_800452C0 (the walkable-field AREA-DATA load… |
| 0x80045558 | LIVE | `ov_loadAreaSlotFile` | game/core/asset.cpp:631 |  | (idx): load indexed file idx (0 = OPN, 1 = CRD) into the shared AREA s… |
| 0x80045558 | LIVE | `Asset::registerOverrides` | game/core/asset.cpp:635 |  |  |
| 0x80045580 | LIVE | `ActorTomba::ov_turnBiasCompute` | game/player/actor_tomba.cpp:1059 |  | ov_turnBiasCompute/ov_outerTransitionGate/ov_outerTransitionCommit/ov_… |
| 0x80045580 | LIVE | `ActorTomba::assetReady` | game/player/actor_tomba.cpp:1157 |  | assetReady — guest FUN_80045580. See actor_tomba.h for the full RE wri… |
| 0x8004766C | LIVE | `Collision::snapObjectToTerrain` | game/player/collision.cpp:816 | 0x80047778 0x80047CBC 0x80048034 0x80048134 0x80049968 | Collision::snapObjectToTerrain. THE object-level entry point of the gr… |
| 0x8004798C | LIVE | `Collision::gridStep` | game/player/collision.cpp:719 | 0x8004798C |  |
| 0x80047CBC | LIVE | `Collision::gridQuery` | game/player/collision.cpp:493 | 0x80047CBC |  |
| 0x800498C8 | LIVE | `Collision::gridResolve` | game/player/collision.cpp:572 | 0x800498C8 |  |
| 0x80049968 | LIVE | `Collision::gridSetup` | game/player/collision.cpp:301 | 0x80049968 | collision-grid ROW-POINTER setup. a0 = grid/layer index (&0xff). Reads… |
| 0x800499E8 | LIVE | `Engine::task0Bootstrap` | game/core/engine.cpp:3171 |  | resolve \BIN\START.BIN natively, record its {LBA,size}, switch |
| 0x80049A60 | LIVE | `ActorReward::smWindowScroll` | game/object/actor_sm_reward.cpp:174 |  | ActorReward::smWindowScroll(c) — FUN_80049A60(obj a0, side a1). Scroll… |
| 0x80049E54 | LIVE | `ActorReward::smTallyTick` | game/object/actor_sm_reward.cpp:333 |  | ActorReward::smTallyTick(c) — FUN_80049E54(obj a0, step a1) -> v0. Tic… |
| 0x8004A3D4 | LIVE | `ActorReward::smEventDispatch` | game/object/actor_sm_reward.cpp:389 |  | ActorReward::smEventDispatch(c) — FUN_8004A3D4(obj a0) -> v0. Mechanic… |
| 0x8004B150 | LIVE | `ActorReward::smBlinkA` | game/object/actor_sm_reward.cpp:123 |  | ActorReward::smBlinkA(c) — FUN_8004B150(obj a0, side a1). One-shot ini… |
| 0x8004B208 | LIVE | `ActorReward::smBlinkB` | game/object/actor_sm_reward.cpp:144 |  | ActorReward::smBlinkB(c) — FUN_8004B208(obj a0, side a1). Same shape a… |
| 0x8004B3F4 | LIVE | `Spawn::dropScoreGem` | game/world/spawn.cpp:784 | 0x80071B44 | SCORE-GEM DROP wrapper. Every callsite passes one of the eight fixed A… |
| 0x8004BD64 | LIVE | `GraphicsBind::posComposeBody` | game/world/graphics_bind.cpp:207 |  | per-object POSITION-COMPOSE + render-state refresh. RE'd from disas 0x… |
| 0x8004BD64 | LIVE | `GraphicsBind::posCompose` | game/world/graphics_bind.cpp:235 |  |  |
| 0x8004C238 | LIVE | `beh_visibility_gate_dispatch` | game/ai/beh_visibility_gate_dispatch.cpp:81 | 0x80049A60 0x80049E54 0x8004A118 0x8004A2A0 0x8004A3D4 0x8004B150 … |  |
| 0x8004C324 | LIVE | `state1_gate` | game/ai/beh_visibility_gate_dispatch.cpp:54 |  | --- STATE 1 shared VISIBILITY GATE (the body at 0x8004c324 / c3a4 / c4… |
| 0x8004CE14 | LIVE | `beh_record_list_scanner` | game/ai/beh_record_list_scanner.cpp:61 | 0x80111CCC |  |
| 0x8004D338 | LIVE | `Inventory::addNative` | game/items/inventory.cpp:74 |  | PC-native reimplementation of FUN_8004D338 (inventory_add). Writes are… |
| 0x8004D338 | LIVE | `Inventory::addBody` | game/items/inventory.cpp:113 |  | --- the FUN_8004D338 override + invverify gate -----------------------… |
| 0x8004D338 | LIVE | `Inventory::addEntry` | game/items/inventory.cpp:185 |  |  |
| 0x8004D338 | LIVE | `Inventory::add` | game/items/inventory.cpp:225 |  | --- PC-shape mutators: set the guest ABI regs and route through the st… |
| 0x8004D4C4 | LIVE | `Inventory::giveAndFlagBody` | game/items/inventory.cpp:195 | 0x8004ED0C | give_and_flag(type, amount): native add, then dispatch the PSX flag/ev… |
| 0x8004D4C4 | LIVE | `Inventory::giveAndFlagEntry` | game/items/inventory.cpp:202 |  |  |
| 0x8004D4C4 | LIVE | `Inventory::giveAndFlag` | game/items/inventory.cpp:235 |  |  |
| 0x8004D4F4 | LIVE | `Inventory::giveBody` | game/items/inventory.cpp:211 |  | give_only(type, amount): native add only. |
| 0x8004D4F4 | LIVE | `Inventory::giveEntry` | game/items/inventory.cpp:214 |  |  |
| 0x8004D4F4 | LIVE | `Inventory::give` | game/items/inventory.cpp:230 |  |  |
| 0x8004D7EC | LIVE | `Bit::test7EC` | game/math/mathlib.cpp:26 | 0x8004D7EC | pure bitmap bit-test (~2%, 6.8k calls): byte = bitmap[(int16)(idx/8)] … |
| 0x8004D868 | LIVE | `Bit::test868` | game/math/mathlib.cpp:56 | 0x8004D868 | sibling of FUN_8004D7EC (bit-test) against a fixed third bitmap @0x800… |
| 0x8004EB94 | LIVE | `emitSegmentLayout` | game/render/hud_gauge_emitter.cpp:148 |  | (descAddr, sign_extend16(spanBase + spanBias + bias)) call shape, shar… |
| 0x8004ED0C | LIVE | `Inventory::abGate` | game/items/inventory.cpp:126 |  | Full RAM+scratchpad A/B vs original guest-body call. The pure-leaf cor… |
| 0x8004ED94 | LIVE | `Engine::announcerCue` | game/core/engine.cpp:1193 | 0x8004FA38 | Engine::announcerCue — FUN_8004ED94. `id` sign-extended s16, then time… |
| 0x8004FA38 | LIVE | `Inventory::abGate` | game/items/inventory.cpp:126 |  | Full RAM+scratchpad A/B vs original guest-body call. The pure-leaf cor… |
| 0x8004FB20 | LIVE | `Pool::clearBf548Region` | game/world/pool.cpp:69 |  | zero 700 bytes at 0x800BF548. Trivial memset wrapper. Every field of t… |
| 0x8004FB4C | LIVE | `HudGaugeEmitter::emitItem` | game/render/hud_gauge_emitter.cpp:201 |  |  |
| 0x8004FD30 | LIVE | `HudGaugeEmitter::emitFrame` | game/render/hud_gauge_emitter.cpp:155 |  |  |
| 0x8004FE84 | LIVE | `Engine::sceneRenderListBuilder` | game/core/engine.cpp:840 |  | Native FUN_8004FE84 — a 2-phase scene/render-list builder driver (stru… |
| 0x8004FFB4 | LIVE | `Panel::fillQuad` | game/ui/panel_fill.cpp:77 |  | EQUIVALENCE. This is a REBUILD, not a transcription, so `port_check` c… |
| 0x8005082C | LIVE | `ModeStateArm::arm` | game/scene/mode_state_arm.cpp:10 |  | ModeStateArm::arm — native ownership of FUN_8005082C (Ghidra decomp sc… |
| 0x800508A8 | LIVE | `ModeStateArm::armFromAreaTable` | game/scene/mode_state_arm.cpp:30 |  | ModeStateArm::armFromAreaTable — native ownership of FUN_800508A8 (Ghi… |
| 0x80050970 | LIVE | `BgSceneTransitionSm::bf816Dispatch` | game/scene/bg_scene_transition_sm.cpp:115 |  | tiny dispatcher on the 800BF816 mode byte: 0 = ModeStateArm::armFromAr… |
| 0x800509B4 | LIVE | `Engine::initDisplay` | game/scene/startup.cpp:86 | 0x80050738 |  |
| 0x80050A0C | LIVE | `Engine::initFrameState` | game/scene/startup.cpp:58 |  |  |
| 0x80050A80 | LIVE | `Engine::initCamera` | game/scene/startup.cpp:123 |  | engine CAMERA init: identity camera-rotation matrix at scratchpad 0x1F… |
| 0x80050DE4 | LIVE | `Engine::sceneStateStep` | game/core/engine.cpp:2710 |  | Engine::sceneStateStep — the SCENE-INIT / SCENE-RUN state machine at g… |
| 0x80051128 | LIVE | `NodeXform::propagate` | game/render/node_xform.cpp:337 |  | per-object CHILD-NODE TRANSFORM loop. RE'd from disas: |
| 0x80051300 | LIVE | `NodeXform::propagateRotmat` | game/render/node_xform.cpp:392 |  | per-object CHILD-NODE TRANSFORM loop, rotmat-single-call variant. RE'd… |
| 0x80051464 | LIVE | `NodeXform::propagateAxis` | game/render/node_xform.cpp:427 |  | sibling of propagateRotmat(): identical control flow, but the child's … |
| 0x80051614 | LIVE | `NodeXform::buildFromChild` | game/render/node_xform.cpp:529 |  | RE'd from authenticated executable/overlay evidence guest 0x80051614 (… |
| 0x80051794 | LIVE | `Mtx::identity` | game/math/mtx.cpp:6 |  |  |
| 0x80051794 | LIVE | `Mtx::registerOverrides` | game/math/mtx.cpp:47 |  |  |
| 0x800517BC | LIVE | `NodeXform::seedBlock` | game/render/node_xform.cpp:370 |  | trivial 8-word block seeder: {x,0,y,0,z,0,0,0}. RE'd + cross-checked v… |
| 0x800517F8 | LIVE | `GraphicsBind::renderUpdateBody` | game/world/graphics_bind.cpp:132 | 0x80051300 | per-object RENDER-STATE UPDATE: build the object's transform, then sna… |
| 0x800517F8 | LIVE | `GraphicsBind::renderUpdate` | game/world/graphics_bind.cpp:155 |  |  |
| 0x80051844 | LIVE | `NodeXform::build` | game/render/node_xform.cpp:265 |  | REGISTER FAITHFULNESS (2026-07-08, the f117 residual root cause): fram… |
| 0x800518FC | LIVE | `NodeXform::buildWithOffset` | game/render/node_xform.cpp:301 |  | NodeXform::buildWithOffset — PC-native reimpl of guest FUN_800518FC. |
| 0x800519E0 | LIVE | `GraphicsBind::recordArrayInit` | game/world/graphics_bind.cpp:299 |  |  |
| 0x80051B04 | LIVE | `GraphicsBind::installSceneRecord` | game/world/graphics_bind.cpp:109 |  | two-level scene-data-table pointer resolve. Pure address arithmetic, n… |
| 0x80051B34 | LIVE | `NodeXform::copyMatrixBlock` | game/render/node_xform.cpp:494 |  | frameless leaf, verbatim from authenticated executable/overlay evidenc… |
| 0x80051B70 | LIVE | `GraphicsBind::recordInitBody` | game/world/graphics_bind.cpp:50 |  | per-object render-record INIT. Allocates a record (FUN_8007AAE8), zero… |
| 0x80051B70 | LIVE | `GraphicsBind::recordInit` | game/world/graphics_bind.cpp:98 |  |  |
| 0x80051C8C | LIVE | `NodeXform::buildAxis` | game/render/node_xform.cpp:465 |  | node-level sibling of build(): composes THIS node's own world matrix v… |
| 0x80051D20 | LIVE | `NodeXform::worldPosFromComposed` | game/render/node_xform.cpp:601 |  | sibling of worldPosFromLocal() using node's COMPOSED world matrix and … |
| 0x80051D90 | LIVE | `NodeXform::worldPosFromLocal` | game/render/node_xform.cpp:584 |  | RE'd from authenticated executable/overlay evidence guest 0x80051D90 (… |
| 0x80052078 | LIVE | `Engine::startStage` | game/core/engine.cpp:3151 | 0x80080870 0x80080890 0x800808A0 | -- PC-native task-0 bootstrap: own the START.BIN resolve + stage-0 ove… |
| 0x800520E0 | LIVE | `Engine::initSubsystems` | game/scene/startup.cpp:314 |  |  |
| 0x8005229C | LIVE | `Engine::padFenceTail` | game/input/pad_edge_fence.cpp:145 | 0x80087AEC 0x80087E2C 0x80087EAC | Override wrapper + install (guest ABI is all-implicit — the fence take… |
| 0x8005229C | LIVE | `ov_padFenceTail` | game/input/pad_edge_fence.cpp:351 |  |  |
| 0x800527C8 | LIVE | `beh_actor_tomba_proximity_combat` | game/ai/beh_actor_tomba_proximity_combat.cpp:48 | 0x80041718 0x80041768 0x8004190C 0x80042728 0x800518FC 0x800519E0 … |  |
| 0x80053E50 | LIVE | `ActorTomba::outerTransitionGate` | game/player/actor_tomba.cpp:1180 |  |  |
| 0x80053FDC | LIVE | `ActorTomba::outerTransitionCommit` | game/player/actor_tomba.cpp:1245 |  | outerTransitionCommit — guest FUN_80053FDC(G, mode). See actor_tomba.h… |
| 0x80054198 | LIVE | `SceneTransition::clearSwapBlock` | game/scene/scene_transition.cpp:130 |  | small swap-block ephemeral clear. RE'd from disas 0x80054198..0x800541… |
| 0x80054650 | LIVE | `ActorTomba::settleStep` | game/player/actor_tomba.cpp:928 | 0x8004954C | ======================================================================… |
| 0x80054D14 | LIVE | `Engine::walkStart` | game/core/engine.cpp:1216 |  | Engine::walkStart — FUN_80054D14. |
| 0x80055C9C | LIVE | `gov_turnBiasCompute` | game/player/actor_tomba.cpp:1072 |  | installed via tomba::native::declareOverride() at game/player/actor_to… |
| 0x80056B48 | LIVE | `ActorTomba::velocityIntegrate` | game/player/actor_tomba.cpp:991 |  | ======================================================================… |
| 0x80057DC0 | LIVE | `ActorTomba::growthStep` | game/player/actor_tomba.cpp:552 |  | ======================================================================… |
| 0x80058304 | LIVE | `Engine::gStateMutate` | game/core/engine.cpp:1329 | 0x800310F4 | Engine::gStateMutate — native ownership of FUN_80058304 (Ghidra decomp |
| 0x8005950C | LIVE | `ActorTomba::frameTick` | game/player/actor_tomba.cpp:1327 |  |  |
| 0x80059D28 | LIVE | `Engine::frameStartTick` | game/core/engine.cpp:2934 |  | Engine::frameStartTick — per-frame prologue at guest 0x80059D28 (FIRST… |
| 0x80059ED8 | LIVE | `beh_camera_target_follow` | game/ai/beh_camera_target_follow.cpp:54 | 0x800312D4 0x800489E4 0x8010B238 0x8010BC10 0x8010C5A8 0x8011332C … |  |
| 0x8005A910 | LIVE | `ActorTomba::mode0ActionGate` | game/player/actor_tomba.cpp:1016 |  |  |
| 0x80067DA8 | LIVE | `Engine::uploadModeSprites` | game/core/engine.cpp:1263 | 0x80081218 | Engine::uploadModeSprites — native ownership of FUN_80067DA8 (Ghidra d… |
| 0x8006C80C | LIVE | `CutsceneCamera::yFloor` | game/camera/cutscene_camera.cpp:431 |  | ── yFloor (camera-Y floor clamp, per render mode) ────────────────────… |
| 0x8006C988 | LIVE | `CutsceneCamera::shakeTail` | game/camera/cutscene_camera.cpp:895 |  | ── post-mode TAIL (0x8006C988) — the camera SHAKE state machine ──────… |
| 0x8006CBA8 | LIVE | `CutsceneCamera::initSeedGrp` | game/camera/cutscene_camera.cpp:1105 |  |  |
| 0x8006CBD0 | LIVE | `GraphicsBind::setXformBlkBody` | game/world/graphics_bind.cpp:181 |  | copy a 6-halfword TRANSFORM BLOCK from a1 into the scratchpad camera/t… |
| 0x8006CBD0 | LIVE | `GraphicsBind::setXformBlk` | game/world/graphics_bind.cpp:192 |  |  |
| 0x8006D02C | LIVE | `CutsceneCamera::lookAt` | game/camera/cutscene_camera.cpp:714 |  |  |
| 0x8006D2AC | LIVE | `CutsceneCamera::distSolve` | game/camera/cutscene_camera.cpp:259 |  | ── distSolve (distance/zoom solver) ──────────────────────────────────… |
| 0x8006D654 | LIVE | `CutsceneCamera::pitch` | game/camera/cutscene_camera.cpp:490 |  | ── pitch (vertical-look height smoother) ─────────────────────────────… |
| 0x8006D934 | LIVE | `CutsceneCamera::snapAccXZ` | game/camera/cutscene_camera.cpp:795 |  | ── orchestrators (per-frame camera modes) ────────────────────────────… |
| 0x8006D950 | LIVE | `CutsceneCamera::snapAccY` | game/camera/cutscene_camera.cpp:799 |  |  |
| 0x8006D960 | LIVE | `CutsceneCamera::trackXZ` | game/camera/cutscene_camera.cpp:74 |  | ── follow accumulators ───────────────────────────────────────────────… |
| 0x8006DA54 | LIVE | `CutsceneCamera::trackY` | game/camera/cutscene_camera.cpp:82 |  |  |
| 0x8006DAD8 | LIVE | `CutsceneCamera::posBuildB` | game/camera/cutscene_camera.cpp:120 |  |  |
| 0x8006DC38 | LIVE | `CutsceneCamera::posBuildA` | game/camera/cutscene_camera.cpp:111 |  | ── scripted-camera look-angle builders (0x8006DC38/DAD8/DF88/DEF0 — us… |
| 0x8006DCF4 | LIVE | `CutsceneCamera::heading` | game/camera/cutscene_camera.cpp:620 |  | ── heading (heading tracker) ─────────────────────────────────────────… |
| 0x8006DEF0 | LIVE | `CutsceneCamera::headBuildB` | game/camera/cutscene_camera.cpp:138 |  |  |
| 0x8006DF88 | LIVE | `CutsceneCamera::headBuildA` | game/camera/cutscene_camera.cpp:128 |  |  |
| 0x8006E010 | LIVE | `CutsceneCamera::angleStep` | game/camera/cutscene_camera.cpp:380 |  | ── angleStep ─────────────────────────────────────────────────────────… |
| 0x8006E0F0 | LIVE | `CutsceneCamera::mainFollow` | game/camera/cutscene_camera.cpp:850 |  |  |
| 0x8006E1C0 | LIVE | `CutsceneCamera::pushMode` | game/camera/cutscene_camera.cpp:1131 |  |  |
| 0x8006E1E4 | LIVE | `CutsceneCamera::restoreMode` | game/camera/cutscene_camera.cpp:1138 |  |  |
| 0x8006E228 | LIVE | `CutsceneCamera::trackFollow` | game/camera/cutscene_camera.cpp:875 |  |  |
| 0x8006E294 | LIVE | `CutsceneCamera::snapFollowA` | game/camera/cutscene_camera.cpp:826 |  |  |
| 0x8006E2FC | LIVE | `CutsceneCamera::snapFollowB` | game/camera/cutscene_camera.cpp:841 |  |  |
| 0x8006E360 | LIVE | `CutsceneCamera::pitchFollow` | game/camera/cutscene_camera.cpp:835 |  |  |
| 0x8006E3B0 | LIVE | `CutsceneCamera::snapFollow` | game/camera/cutscene_camera.cpp:802 |  |  |
| 0x8006E3F4 | LIVE | `CutsceneCamera::simpleFollow` | game/camera/cutscene_camera.cpp:866 |  |  |
| 0x8006E464 | LIVE | `CutsceneCamera::rotBuild` | game/camera/cutscene_camera.cpp:226 |  |  |
| 0x8006E8F8 | LIVE | `CutsceneCamera::resetFollowAccum` | game/camera/cutscene_camera.cpp:1125 |  | ── Wiring pass (2026-07-08 frontier follow-up) ───────────────────────… |
| 0x8006E918 | LIVE | `CutsceneCamera::initPlace` | game/camera/cutscene_camera.cpp:1073 |  |  |
| 0x8006EA00 | LIVE | `CutsceneCamera::snapToMasterOffsetY200` | game/camera/cutscene_camera.cpp:1159 |  | pushes a real 32-byte guest frame (r29-=32, s0/s1/ra spilled at +16/+2… |
| 0x8006EA7C | ORPHAN | `CutsceneCamera::init` | game/camera/cutscene_camera.cpp:1448 |  |  |
| 0x8006EC44 | LIVE | `CutsceneCamera::update` | game/camera/cutscene_camera.cpp:1216 |  |  |
| 0x8006EC44 | LIVE | `CutsceneCamera::updateFaithful` | game/camera/cutscene_camera.cpp:1258 | 0x8006C988 0x8006EA7C | pc_faithful mirror of guest 0x8006EC44 (authenticated executable/overl… |
| 0x8006EF38 | LIVE | `CutsceneCamera::orbitTick` | game/camera/cutscene_camera.cpp:1190 |  | pushes the same shape of 32-byte frame (r29-=32, s0/s1/ra spilled at +… |
| 0x8006EFF4 | LIVE | `Bit::testFE48` | game/math/mathlib.cpp:84 |  | u32 flag-bit TEST on the fixed 32-bit word at 0x800BFE48. Pure 5-instr… |
| 0x8006F00C | LIVE | `Bit::setFE48` | game/math/mathlib.cpp:98 |  | sibling of setFE34: u32 flag-bit SET on 0x800BFE48 (the word testFE48 … |
| 0x8006F02C | LIVE | `Bit::setFE34` | game/math/mathlib.cpp:91 |  | u32 flag-bit SET on the fixed 32-bit word at 0x800BFE34. 7-instruction… |
| 0x8006F04C | LIVE | `Bit::processLinkRequest` | game/math/mathlib.cpp:114 |  | child-link REQUEST-mailbox arbiter. disas 0x8006F04C..0x8006F0E0: |
| 0x8006F2D0 | LIVE | `beh_pad_child_linker` | game/ai/beh_pad_child_linker.cpp:63 | 0x8004766C 0x80047B5C 0x8006F138 |  |
| 0x80070018 | LIVE | `ActorReward::update` | game/object/actor_sm_reward.cpp:701 |  |  |
| 0x800702C0 | LIVE | `ActorReward::resolvePosition` | game/object/actor_sm_reward.cpp:915 |  |  |
| 0x80070650 | LIVE | `ActorReward::approachTargetX` | game/object/actor_sm_reward.cpp:1027 |  | ActorReward::approachTargetX(c) — FUN_80070650(obj a0). Trivial ease: … |
| 0x80071A3C | LIVE | `beh_area_event_dispatch` | game/ai/beh_area_event_dispatch.cpp:45 | 0x800716B4 0x80071768 0x801178E4 0x8011B79C |  |
| 0x80072A78 | LIVE | `Placement::placeAreaObjects` | game/world/placement.cpp:144 | 0x80072A78 |  |
| 0x80072DDC | LIVE | `Placement::spawnWithParent` | game/world/placement.cpp:211 | 0x80072DDC |  |
| 0x80073194 | LIVE | `ScriptInterp::advanceGauge` | game/scene/script_interp.cpp:1198 | 0x80074590 | ORACLE: guest 0x80073194 |
| 0x80073260 | LIVE | `SceneTransition::resetSwap` | game/scene/scene_transition.cpp:113 |  |  |
| 0x800732C0 | LIVE | `SceneTransition::beginSwap` | game/scene/scene_transition.cpp:148 |  |  |
| 0x80073300 | LIVE | `SceneTransition::completeSwap` | game/scene/scene_transition.cpp:155 |  |  |
| 0x80073328 | LIVE | `SceneTransition::stepSwapWaiter` | game/scene/scene_transition.cpp:163 | 0x80073328 |  |
| 0x800735F4 | LIVE | `Spawn::tickLinkedOverlay` | game/world/spawn.cpp:875 |  | per-object controller that owns exactly ONE linked "variant overlay" c… |
| 0x80073750 | LIVE | `Font::measureLineWidth` | game/ui/font.cpp:202 |  | pure string measurer (disas 0x80073750..0x80073798, no sub-calls): |
| 0x800739AC | LIVE | `beh_scene_ui_trigger` | game/ai/beh_scene_ui_trigger.cpp:60 | 0x800737F8 0x800738B0 0x80074BF8 |  |
| 0x80073CD8 | LIVE | `beh_typed_init_scene_trigger` | game/ai/beh_typed_init_scene_trigger.cpp:107 |  |  |
| 0x800741DC | LIVE | `beh_pickup_collect_trigger` | game/ai/beh_pickup_collect_trigger.cpp:213 |  |  |
| 0x80074590 | LIVE | `Sfx::trigger` | game/audio/sfx.cpp:16 | 0x80074BF8 0x80074EEC 0x80075E04 |  |
| 0x80074810 | LIVE | `Sfx::triggerPanned` | game/audio/sfx.cpp:165 | 0x80074590 | ORACLE: guest 0x80074810 |
| 0x80074810 | LIVE | `Sfx::registerOverrides` | game/audio/sfx.cpp:196 |  |  |
| 0x8007496C | LIVE | `AreaSlots::updateCell` | game/world/area_slots.cpp:277 | 0x80092E3C | AreaSlots::updateCell — FUN_8007496C body. sigArg carries {idx: low by… |
| 0x80074A38 | LIVE | `AreaSlots::primeCountdown` | game/world/area_slots.cpp:265 |  | AreaSlots::primeCountdown — FUN_80074A38 body. Pure 1-store leaf: tabl… |
| 0x80074A38 | LIVE | `AreaSlots::registerOverrides` | game/world/area_slots.cpp:382 |  |  |
| 0x80074AF0 | LIVE | `AreaSlots::ackIfMatch` | game/world/area_slots.cpp:250 |  | AreaSlots::ackIfMatch — FUN_80074AF0 body. Pure 21-instruction primiti… |
| 0x80074BC4 | LIVE | `AudioDispatch::settleField` | game/audio/audio_dispatch.cpp:86 | 0x8001CF2C 0x80074B44 0x80074E48 | AudioDispatch::settleField — native ownership of FUN_80074BC4 (Ghidra … |
| 0x80074F24 | LIVE | `Pool::selectStateIndex` | game/world/pool.cpp:350 |  | per-area STATE-INDEX select + apply. Early-out if scratchpad 0x1F80013… |
| 0x80075024 | LIVE | `AudioDispatch::selectStateRemap` | game/audio/audio_dispatch.cpp:150 | 0x800750D8 | AudioDispatch::selectStateRemap — native ownership of FUN_80075024. Ma… |
| 0x80075070 | LIVE | `AudioDispatch::publishStateFade` | game/audio/audio_dispatch.cpp:188 | 0x80075CEC | AudioDispatch::publishStateFade — native ownership of FUN_80075070. Pu… |
| 0x800750A4 | LIVE | `AudioDispatch::selectState` | game/audio/audio_dispatch.cpp:102 |  | AudioDispatch::selectState — native ownership of FUN_800750A4 (Ghidra … |
| 0x800750D8 | LIVE | `AudioDispatch::dispatch3Way` | game/audio/audio_dispatch.cpp:32 | 0x8001CF2C | AudioDispatch::dispatch3Way — native ownership of FUN_800750D8 (Ghidra… |
| 0x80075130 | LIVE | `Font::init` | game/ui/font.cpp:129 |  | font / text system init orchestrator. No args, no return. Mirrors the … |
| 0x80075240 | LIVE | `Pool::reset75240` | game/world/pool.cpp:192 |  | reset the control block at 0x800BE1F8: call 0x80075D58 leaf, seed clam… |
| 0x800752B4 | LIVE | `Font::glyphClassFill` | game/ui/font.cpp:103 |  | glyph-class table fill. Iterates i = 0..23 over the 24-entry table. Th… |
| 0x800753AC | LIVE | `preload_build_vram` | game/core/asset.cpp:383 | 0x80075448 | cel/sprite VRAM build, synchronous. FUN_800753ac is itself an async CD… |
| 0x800753D4 | LIVE | `preload_cel` | game/core/asset.cpp:349 | 0x80096480 0x80096980 0x80096A40 | cel-load, SYNCHRONOUS. Original: FUN_80096480 (slot alloc + BAV cel lo… |
| 0x800753D4 | LIVE | `preload_build_vram` | game/core/asset.cpp:383 | 0x80075448 | cel/sprite VRAM build, synchronous. FUN_800753ac is itself an async CD… |
| 0x80075448 | LIVE | `preload_build_vram` | game/core/asset.cpp:383 | 0x80075448 | cel/sprite VRAM build, synchronous. FUN_800753ac is itself an async CD… |
| 0x800754F4 | LIVE | `preload_build_vram` | game/core/asset.cpp:383 | 0x80075448 | cel/sprite VRAM build, synchronous. FUN_800753ac is itself an async CD… |
| 0x80075824 | LIVE | `MusicCoord::voiceMixTick` | game/audio/music_coord.cpp:140 |  | Per-frame VOICE-CHANNEL VOLUME MIXER — port of FUN_80075824 (RE'd via … |
| 0x80075A80 | LIVE | `AreaSlots::updateTail` | game/world/area_slots.cpp:44 | 0x80074BF8 0x80074E48 0x8008E0C0 0x80092660 0x80098F90 0x80099490 … | AreaSlots::updateTail — the last direct child of ov_field_frame at gue… |
| 0x80075CEC | LIVE | `BgSceneTransitionSm::audioFadeTarget` | game/scene/bg_scene_transition_sm.cpp:75 |  | - Native ports of the tiny sub-leaves this SM calls ------------------… |
| 0x80075D24 | LIVE | `MusicCoord::setGain2` | game/audio/music_coord.cpp:247 |  | MusicCoord::setGain2 — FUN_80075D24 body. See music_coord.h for the RE… |
| 0x80075D24 | LIVE | `MusicCoord::registerOverrides` | game/audio/music_coord.cpp:277 |  |  |
| 0x80075F0C | LIVE | `Animation::applyFrame` | game/object/animation.cpp:627 |  | ──────────────────────────────────────────────────────────────────────… |
| 0x80076904 | LIVE | `Animation::loadFrame` | game/object/animation.cpp:393 |  |  |
| 0x80076904 | LIVE | `Animation::registerOverrides` | game/object/animation.cpp:692 |  |  |
| 0x80076D68 | LIVE | `Animation::stepFramed` | game/object/animation.cpp:247 |  | Animation::stepFramed — GUEST-ABI ENTRY ONLY for FUN_80076D68 (RE: aut… |
| 0x8007703C | LIVE | `Cull::enqueueByClass` | game/render/cull.cpp:316 |  | Cull::enqueueByClass — PC-native FUN_8007703C body. Class-keyed queue … |
| 0x8007712C | LIVE | `Cull::decide` | game/render/cull.cpp:73 |  | Pure (read-only) cull decision — reproduces FUN_8007712c's control flo… |
| 0x8007712C | LIVE | `Cull::performBaseCull` | game/render/cull.cpp:160 |  | Cull::performBaseCull — byte-exact PC-native FUN_8007712C body (no mar… |
| 0x8007712C | LIVE | `Cull::objectCull` | game/render/cull.cpp:375 |  |  |
| 0x8007712C | LIVE | `Cull::performBaseCullFramed` | game/render/cull.cpp:585 |  | performBaseCullFramed — mirrors FUN_8007712C's OWN real 40-byte guest-… |
| 0x80077768 | LIVE | `Trig::angleCmp` | game/math/trig.cpp:80 |  |  |
| 0x8007778C | LIVE | `Cull::wrapFrame` | game/render/cull.cpp:560 |  | camera-relative cull WRAPPER. Computes obj-cam delta (wrapping s16, si… |
| 0x8007778C | LIVE | `Cull::cullWrapper` | game/render/cull.cpp:608 |  |  |
| 0x800777FC | LIVE | `Cull::cullWrapperFlag2` | game/render/cull.cpp:676 |  | UNFRAMED — the public entry point EXISTING native beh_ callers (beh_id… |
| 0x80077870 | LIVE | `Cull::cullWrapperFlag1` | game/render/cull.cpp:640 |  | cull-wrapper variant: byte-identical to cullWrapper (obj in c->r[4], d… |
| 0x800778E4 | LIVE | `Cull::cullWrapperOffsetY` | game/render/cull.cpp:784 |  |  |
| 0x800779D0 | LIVE | `Cull::cullWrapperOffset` | game/render/cull.cpp:740 |  |  |
| 0x80077A4C | LIVE | `Cull::cullWrapperOffsetFlag1` | game/render/cull.cpp:761 |  |  |
| 0x80077ACC | LIVE | `Cull::cullWrap77acc` | game/render/cull.cpp:710 |  | UNFRAMED — the public entry point EXISTING native callers (beh_record_… |
| 0x80077B38 | LIVE | `GraphicsBind::setGeomBody` | game/world/graphics_bind.cpp:163 |  | set an object's GEOMETRY-BLOCK pointer from a table. RE'd from disas 0… |
| 0x80077B38 | LIVE | `GraphicsBind::setGeom` | game/world/graphics_bind.cpp:171 |  |  |
| 0x80077B5C | LIVE | `Animation::advanceLinkChain` | game/object/animation.cpp:509 |  | ──────────────────────────────────────────────────────────────────────… |
| 0x80077C40 | LIVE | `Animation::attach` | game/object/animation.cpp:566 | 0x80075FF8 | ──────────────────────────────────────────────────────────────────────… |
| 0x80077E7C | LIVE | `Cull::enqueueQueueA` | game/render/cull.cpp:338 |  | Cull::enqueueQueueA — PC-native FUN_80077E7C body. Manual push of `obj… |
| 0x80077EBC | LIVE | `Cull::enqueueVisibleClass4` | game/render/cull.cpp:294 |  | Cull::enqueueVisibleClass4 — PC-native FUN_80077EBC body. Manual push … |
| 0x80077EFC | LIVE | `Cull::enqueueQueueC` | game/render/cull.cpp:358 |  | Cull::enqueueQueueC — PC-native FUN_80077EFC body. Manual push onto qu… |
| 0x80077FB0 | LIVE | `eov_isqrt16` | game/math/gte_math.cpp:864 |  | installed via tomba::native::declareOverride() at game/math/gte_math.c… |
| 0x80078240 | LIVE | `Trig::vecLen` | game/math/trig.cpp:112 |  | vecLen (guest FUN_80078240) — the integer 3-D length approximation. Th… |
| 0x80078240 | LIVE | `eov_approxDist3` | game/math/gte_math.cpp:867 |  | installed via tomba::native::declareOverride() at game/math/gte_math.c… |
| 0x800782F0 | LIVE | `SceneTransition::areaMaskTrigger` | game/scene/scene_transition.cpp:28 | 0x800782F0 |  |
| 0x800783DC | LIVE | `Pool::setupViewScroll` | game/world/pool.cpp:215 |  | per-area VIEW/SCROLL setup. Calls a leaf (0x80048D3C), builds the view… |
| 0x80078610 | LIVE | `Pool::finalViewInit` | game/world/pool.cpp:295 |  | final per-area view init: zero two control blocks, seed fixed view par… |
| 0x80078824 | LIVE | `Engine::setAreaStartPos` | game/core/engine.cpp:3200 |  | Engine::setAreaStartPos. Loads the player's per-area spawn |
| 0x800788AC | LIVE | `Engine::padEdgeFence` | game/input/pad_edge_fence.cpp:51 |  | per-frame input-edge fence. See the file header above for the full RE … |
| 0x800788AC | LIVE | `ov_padEdgeFence` | game/input/pad_edge_fence.cpp:348 |  |  |
| 0x80078988 | LIVE | `Font::iconGlyphEmit` | game/ui/font.cpp:775 |  | iconGlyphEmit — FUN_80078988, the SJIS/token ICON-GLYPH string emitter… |
| 0x80078CA8 | LIVE | `Font::glyphEmit` | game/ui/font.cpp:336 | 0x80078988 0x80083DE0 | producer (a3 string, element = byte offset) |
| 0x80078CA8 | LIVE | `Font::registerOverrides` | game/ui/font.cpp:914 |  |  |
| 0x80079324 | LIVE | `Font::drawTextSmall` | game/ui/font.cpp:271 |  | ORACLE: guest 0x80079324 |
| 0x80079324 | LIVE | `ov_drawTextSmall` | game/ui/font.cpp:680 |  | ov_drawTextSmall: sibling of ov_drawText for FUN_80079324 — same guest… |
| 0x80079374 | LIVE | `Font::drawText` | game/ui/font.cpp:245 |  | WIDE-RE TIER DRAFT (2026-07-09), UNWIRED/UNVERIFIED. See header doc fo… |
| 0x80079374 | LIVE | `ov_drawText` | game/ui/font.cpp:668 |  | ov_drawText: extracts drawText's typed args from the guest ABI registe… |
| 0x80079528 | LIVE | `Str::length` | game/core/str.cpp:16 |  | strlen. RE (tools/disas.py 0x80079528 --all 20, cross-checked against |
| 0x80079528 | LIVE | `ov_strLength` | game/core/str.cpp:57 |  |  |
| 0x800796DC | LIVE | `Pool::resetControlBlock` | game/world/pool.cpp:23 |  | zero the 104-byte control block at 0x800BF808, seed two bytes, clear ~… |
| 0x800798F8 | LIVE | `Pool::initTypedPools` | game/world/pool.cpp:81 |  | the 5 typed object pools + list-head init. See pool.h for the pool tab… |
| 0x80079C3C | LIVE | `Spawn::spawnLinkStamp` | game/world/spawn.cpp:69 |  | Link `node` into active list `list` at position `mode` relative to `re… |
| 0x80079C3C | LIVE | `Spawn::entitySpawnBody` | game/world/spawn.cpp:141 |  |  |
| 0x80079DDC | LIVE | `Spawn::spawnPool2Body` | game/world/spawn.cpp:165 |  |  |
| 0x80079F90 | LIVE | `Spawn::poolSpawn` | game/world/spawn.cpp:224 |  |  |
| 0x8007A624 | LIVE | `Spawn::despawn` | game/world/spawn.cpp:303 | 0x8007A624 |  |
| 0x8007A980 | LIVE | `Spawn::dispatch` | game/world/spawn.cpp:197 |  | Run the per-class spawn VARIANT NATIVELY (the 5 bodies are all owned i… |
| 0x8007AAE8 | LIVE | `GraphicsBind::recordAlloc` | game/world/graphics_bind.cpp:78 |  |  |
| 0x8007AAE8 | LIVE | `RenderRecordPool::allocateForGuest` | game/world/render_record_pool.cpp:28 |  | FUN_8007AAE8 as guest code calls it: pops the render-record free stack and begins the record's next life. |
| 0x8007B008 | LIVE | `ObjectList::walkList2` | game/object/object_list.cpp:82 |  |  |
| 0x8007B04C | LIVE | `TransitionState3::walkOnce` | game/scene/transition_state3.cpp:11 |  |  |
| 0x8007B18C | LIVE | `Pool::init` | game/world/pool.cpp:138 |  | top-level object-pool init. Zeroes 520 68-byte slots at 0x800F2740; bu… |
| 0x8007B2C0 | LIVE | `Engine::seedDirectionMasks` | game/scene/startup.cpp:168 |  | direction-mask seeder. Called with 0 at boot (initEntityPool above) an… |
| 0x8007B328 | LIVE | `Engine::initEntityPool` | game/scene/startup.cpp:149 |  | engine SUBSYSTEM init (init-prefix slot, dispatched at native_boot.cpp… |
| 0x8007B3F4 | LIVE | `Engine::reloadEntityPool` | game/scene/startup.cpp:185 |  | re-copy the staged per-area entity-pool control bytes onto the live he… |
| 0x8007D0D0 | LIVE | `DialogTextStream::applyRenderMode` | game/ui/dialog_text_stream.cpp:44 |  | (obj a0) -- LEAF (guest 0x8007D0D0 has no `sp` descent). Cross-checked… |
| 0x8007DC38 | LIVE | `beh_variant_overlay_lifecycle` | game/ai/beh_variant_overlay_lifecycle.cpp:54 | 0x8007C0D0 | NOT port_check-able as it stands: this is a hand-written REBUILD, not … |
| 0x8007E038 | LIVE | `Spawn::spawnOverlayVariantBody` | game/world/spawn.cpp:810 |  | VARIANT-OVERLAY SPAWN primitive. RE'd from disas 0x8007E038..0x8007E10… |
| 0x8007E038 | LIVE | `Spawn::spawnOverlayVariant` | game/world/spawn.cpp:850 |  |  |
| 0x8007E110 | LIVE | `Spawn::sceneEntityBody` | game/world/spawn.cpp:725 |  | SCENE-ENTITY SPAWN primitive. RE'd from disas 0x8007E110..0x8007E1B4. |
| 0x8007E110 | LIVE | `Spawn::sceneEntity` | game/world/spawn.cpp:762 |  |  |
| 0x8007E6DC | LIVE | `ov_compose` | game/ui/ui_sprite.cpp:92 |  | The pause/item menu, the START page and the score popup all paint thro… |
| 0x8007E6DC | LIVE | `UiSprite::compose` | game/ui/ui_sprite_compose.cpp:51 | 0x80083DE0 | (placement r4, indexPtr r5, defBase r6, attrs r7) |
| 0x8007E8DC | LIVE | `UiSprite::drawFromTable` | game/ui/ui_sprite.cpp:40 | 0x8007E1B8 | (x r4, y r5, attr r6, defIndex r7) |
| 0x8007E998 | LIVE | `UiSprite::drawFixedDef152` | game/ui/ui_sprite.cpp:73 | 0x8007E8DC | (x r4, y r5, attr r6) — drawFromTable with the definition index pinned… |
| 0x8007E9C8 | LIVE | `ScreenFade::draw` | game/render/screen_fade.cpp:42 | 0x80083DE0 | The fade leaf: a fill across the draw window and a DR_MODE in the caller's OT slot. |
| 0x8007E9C8 | LIVE | `BgSceneTransitionSm::fadeRect` | game/scene/bg_scene_transition_sm.cpp:61 | 0x8007E9C8 | Guest FUN_8007E9C8(color, P[3], 4). |
| 0x8007FC24 | LIVE | `OptionsPage::pushBackdrop` | game/ui/options_page.cpp:26 |  | ORACLE: guest 0x8007FC24 |
| 0x8007FC24 | LIVE | `OptionsPage::install` | game/ui/options_page.cpp:70 |  |  |
| 0x8007FCC8 | LIVE | `Panel::pushDialogBackdrop` | game/ui/dialog_backdrop.cpp:55 |  | ORACLE: guest 0x8007FCC8 |
| 0x8007FCC8 | LIVE | `ov_push_dialog_backdrop` | game/ui/dialog_backdrop.cpp:84 |  | Guest-ABI entry: x/y/w/h in r4-r7, mode off the caller's stack (see th… |
| 0x8007FD54 | DEAD | — | — | 0x80079374 | The blinking "Loading....." card. Deleted 2026-10-03 with `game/ui/loading_text.*`: its ONLY caller is the `jal` at 0x80044C98, inside `FUN_80044BD4`'s wait loop, and that loop has no reachable native call site — all 22 guest call sites are guest images of load paths the port already runs synchronously. See `docs/issues/kanban-009-...`. |
| 0x8007FDB0 | LIVE | `UnlitModelEmitter::gt3` | game/render/unlit_model_emitter.cpp:95 |  | Resident unlit GT3 list emitter; install site. |
| 0x8008007C | LIVE | `UnlitModelEmitter::gt4` | game/render/unlit_model_emitter.cpp:96 |  | Resident unlit GT4 list emitter; install site. |
| 0x80080F6C | LIVE | `Render::drawSync` | game/render/wide_re_libgpu_leaves.cpp:89 |  | guest 0x80080F6C (0x80080F6C) — DrawSync(mode). VERIFIED & WIRED 2026-… |
| 0x80080F6C | LIVE | `ov_drawSync` | game/render/wide_re_libgpu_leaves.cpp:222 |  |  |
| 0x80081218 | LIVE | `Asset::uploadImage` | game/core/asset.cpp:311 |  | DO NOT REGISTER 0x80081218 IN THE OVERRIDE REGISTRY. It surfaces near … |
| 0x80081458 | LIVE | `Render::clearOTagR` | game/render/wide_re_libgpu_leaves.cpp:153 |  | guest 0x80081458 (0x80081458) — ClearOTagR(OT, entries). VERIFIED & WI… |
| 0x80081458 | LIVE | `ov_clearOTagR` | game/render/wide_re_libgpu_leaves.cpp:225 |  |  |
| 0x80081560 | LIVE | `Engine::drawOTag` | game/game_tomba2.cpp:142 |  | Native ownership of DrawOTag (libgpu FUN_80081560, the per-frame draw … |
| 0x800815D0 | LIVE | `nativePutDrawEnv` | game/render/wide_re_gpu_putdrawenv.cpp:265 |  | nativePutDrawEnv (0x800815D0) — libgpu PutDrawEnv(drawEnvPtr). DRAFT. … |
| 0x80081CF8 | LIVE | `buildDrawAreaRect` | game/render/hud_gauge_emitter.cpp:111 |  | ----------------------------------------------------------------------… |
| 0x80081CF8 | LIVE | `emitDrawAreaAndLink` | game/render/hud_gauge_emitter.cpp:123 |  | Emit the DR_AREA packet built from the sp+rectOff rect into the packet… |
| 0x80081FB0 | LIVE | `LibgpuDrawEnv::setDrawEnv` | game/render/libgpu_draw_env.cpp:108 |  | GUEST_ADDRESS: 80081FB0 authenticated executable/overlay evidence |
| 0x80082220 | LIVE | `nativeDrawMode` | game/render/wide_re_gpu_putdrawenv.cpp:184 |  | nativeDrawMode (0x80082220) — DR_TPAGE mode-word builder. DRAFT. RE'd … |
| 0x80082240 | LIVE | `nativeClipTopLeft` | game/render/wide_re_gpu_putdrawenv.cpp:112 |  | nativeClipTopLeft (0x80082240) — SetDrawAreaTopLeft(x,y) word builder.… |
| 0x800822D8 | LIVE | `nativeClipBottomRight` | game/render/wide_re_gpu_putdrawenv.cpp:141 |  | nativeClipBottomRight (0x800822D8) — SetDrawAreaBottomRight(x,y) word … |
| 0x80082370 | LIVE | `nativeDrawOffset` | game/render/wide_re_gpu_putdrawenv.cpp:170 |  | nativeDrawOffset (0x80082370) — SetDrawingOffset(x,y) word builder. DR… |
| 0x8008238C | LIVE | `nativeTextureWindow` | game/render/wide_re_gpu_putdrawenv.cpp:212 |  | nativeTextureWindow (0x8008238C) — DR_TWIN word builder. DRAFT. RE'd f… |
| 0x80082424 | LIVE | `Render::gpuDmaSend` | game/render/wide_re_gpu_dma_queue.cpp:600 |  | guest 0x80082424 (0x80082424) — GpuDmaSend(arrayPtr, count). VERIFIED … |
| 0x80082424 | LIVE | `ov_gpuDmaSend` | game/render/wide_re_gpu_dma_queue.cpp:675 |  |  |
| 0x80082734 | LIVE | `Render::gpuLoadImageStream` | game/render/wide_re_gpu_loadimage_streamer.cpp:135 |  | guest 0x80082734 (0x80082734) — libgpu LoadImage()-internal chunked GP… |
| 0x80082734 | LIVE | `ov_gpuLoadImageStream` | game/render/wide_re_gpu_loadimage_streamer.cpp:276 |  |  |
| 0x80082C68 | LIVE | `libgpuDmaStatusReset` | game/render/wide_re_libgpu_leaves.cpp:257 |  | libgpuDmaStatusReset (0x80082C68) — GPU-DMA status-block RESET. RE-VER… |
| 0x80082D04 | LIVE | `Render::gpuDmaQueueEnqueue` | game/render/wide_re_gpu_dma_queue.cpp:169 |  | guest 0x80082D04 (0x80082D04) — GpuDmaQueueEnqueue(fn, argValOrPtr, si… |
| 0x80082D04 | LIVE | `ov_gpuDmaQueueEnqueue` | game/render/wide_re_gpu_dma_queue.cpp:666 |  |  |
| 0x80082FB4 | LIVE | `Render::gpuDmaQueueDrain` | game/render/wide_re_gpu_dma_queue.cpp:351 |  | guest 0x80082FB4 (0x80082FB4) — GpuDmaQueueDrain(). VERIFIED & WIRED 2… |
| 0x80082FB4 | LIVE | `ov_gpuDmaQueueDrain` | game/render/wide_re_gpu_dma_queue.cpp:669 |  |  |
| 0x80083364 | LIVE | `Render::gpuDmaQueueSync` | game/render/wide_re_gpu_dma_queue.cpp:483 |  | guest 0x80083364 (0x80083364) — GpuDmaQueueSync(mode). VERIFIED & WIRE… |
| 0x80083364 | LIVE | `ov_gpuDmaQueueSync` | game/render/wide_re_gpu_dma_queue.cpp:672 |  |  |
| 0x80083DE0 | LIVE | `libgpuSetDrawMode` | game/render/wide_re_libgpu_leaves.cpp:295 |  | libgpuSetDrawMode (0x80083DE0) — libgpu **SetDrawMode(DR_MODE* p, int … |
| 0x80083E80 | LIVE | `Trig::rsin` | game/math/trig.cpp:4 |  |  |
| 0x80083E80 | LIVE | `Trig::registerOverrides` | game/math/trig.cpp:149 |  | UNREGISTERED (2026-07-15): rsin/ratan2 are NOT safe as overrides. Thei… |
| 0x80083F50 | LIVE | `Trig::rcos` | game/math/trig.cpp:86 |  |  |
| 0x80084080 | LIVE | `Math::sqrtLzc` | game/math/gte_math.cpp:718 |  | ──────────────────────────────────────────────────────────────────────… |
| 0x80084110 | LIVE | `Math::matMul` | game/math/gte_math.cpp:123 |  |  |
| 0x80084220 | LIVE | `Math::applyMatlv` | game/math/gte_math.cpp:676 |  | ──────────────────────────────────────────────────────────────────────… |
| 0x80084250 | LIVE | `GteTransform3::rotate3AndPackIr` | game/math/wide_re_gte_transform3.cpp:48 |  |  |
| 0x80084360 | LIVE | `Math::matLoadLV` | game/math/gte_math.cpp:749 |  | ──────────────────────────────────────────────────────────────────────… |
| 0x80084470 | LIVE | `Math::applyMatrixLV` | game/math/gte_math.cpp:181 |  | ──────────────────────────────────────────────────────────────────────… |
| 0x800844C0 | LIVE | `Math::applyMatrixSV` | game/math/gte_math.cpp:305 |  | ──────────────────────────────────────────────────────────────────────… |
| 0x80084520 | LIVE | `Math::matColScale` | game/math/gte_math.cpp:804 |  | ORACLE: guest 0x80084520 |
| 0x800847B0 | LIVE | `vertexHeaderRepack` | game/render/wide_re_libgpu_leaves.cpp:343 |  | vertexHeaderRepack (0x800847B0) — 20-byte SoA->AoS vertex-header REPAC… |
| 0x800847F0 | LIVE | `Math::rotMatSoft` | game/math/gte_math.cpp:508 |  |  |
| 0x80084A80 | LIVE | `Math::rotMatSoftInverse` | game/math/gte_math.cpp:550 |  | ──────────────────────────────────────────────────────────────────────… |
| 0x80084D10 | LIVE | `Math::rotX` | game/math/gte_math.cpp:476 |  |  |
| 0x80084EB0 | LIVE | `Math::rotY` | game/math/gte_math.cpp:472 |  |  |
| 0x80085050 | LIVE | `Math::rotZ` | game/math/gte_math.cpp:468 |  |  |
| 0x800851F0 | LIVE | `Math::rotMatSoftYXZ` | game/math/gte_math.cpp:641 |  | ──────────────────────────────────────────────────────────────────────… |
| 0x80085480 | LIVE | `Math::rotmat` | game/math/gte_math.cpp:380 |  |  |
| 0x80085690 | LIVE | `Trig::ratan2` | game/math/trig.cpp:28 |  |  |
| 0x80085C9C | LIVE | `tomba::LibapiIntr::setIntrMask` | game/core/libapi_intr.cpp:116 |  |  |
| 0x80086230 | LIVE | `tomba::LibapiIntr::initVblankCallbacks` | game/core/libapi_intr.cpp:126 |  | FUN_0x80086230 — VBlank-callback subsystem init: clear the 8-slot VSyn… |
| 0x80086288 | LIVE | `tomba::LibapiIntr::runVblankCallbacks` | game/core/libapi_intr.cpp:148 |  | FUN_0x80086288 — the VBlank handler itself: bump the tick counter, the… |
| 0x80086320 | LIVE | `tomba::LibapiIntr::clearWords` | game/core/libapi_intr.cpp:177 |  | FUN_0x80086320 — the word-fill helper: writes N words of a constant. |
| 0x80086604 | LIVE | `Engine::activeModeCtx` | game/scene/startup.cpp:337 |  | Engine::activeModeCtx. Accessor: returns the active mode/draw-env cont… |
| 0x80086604 | LIVE | `ov_engineActiveModeCtx` | game/core/engine.cpp:3233 |  | installed via tomba::native::declareOverride() at game/core/engine.cpp… |
| 0x80086620 | LIVE | `eng_init_mode_ctrl` | game/scene/startup.cpp:201 |  | engine MODE control: file-local helper (only called from Engine::initS… |
| 0x80086738 | LIVE | `Engine::installModeHandlers` | game/scene/startup.cpp:346 |  | Engine::installModeHandlers. Installs the mode handler table at 0x8010… |
| 0x80086738 | LIVE | `ov_engineInstallModeHandlers` | game/core/engine.cpp:3236 |  | installed via tomba::native::declareOverride() at game/core/engine.cpp… |
| 0x80086764 | LIVE | `Engine::runModeEnter` | game/scene/startup.cpp:364 |  | Engine::runModeEnter. If both bit0 flags in the mode ctx (*0x800ABE98)… |
| 0x80086764 | LIVE | `ov_engineRunModeEnter` | game/core/engine.cpp:3239 |  | installed via tomba::native::declareOverride() at game/core/engine.cpp… |
| 0x80087A60 | LIVE | `Engine::initInput` | game/scene/startup.cpp:236 | 0x80080890 0x800808A0 0x80085B10 0x800873F0 0x80087400 | a thin wrapper that just calls FUN_80086970; owned as initInput(). |
| 0x80088B00 | LIVE | `Engine::initAlloc` | game/scene/startup.cpp:269 | 0x80086738 0x80089160 0x8009A340 | engine ALLOCATOR / dispatch-table init. `s1` / `s2` are the struct-spa… |
| 0x8008913C | LIVE | `Engine::allocRecordForSelector` | game/scene/startup.cpp:42 |  | returns the base of record[0] or record[1] of the 240-byte-stride, 2-e… |
| 0x8008913C | LIVE | `ov_allocRecordForSelector` | game/scene/startup.cpp:387 |  |  |
| 0x8008A110 | LIVE | `LibcdNative::posToInt` | game/cd/libcd_native.cpp:34 |  |  |
| 0x8008B8F0 | LIVE | `LibcdNative::searchFile` | game/cd/libcd_native.cpp:23 |  |  |
| 0x8008BBE8 | LIVE | `LibcdDirCache::newMedia` | game/cd/libcd_dir_cache.cpp:26 |  |  |
| 0x8008BBE8 | LIVE | `LibcdNative::newMedia` | game/cd/libcd_native.cpp:12 |  |  |
| 0x8008BF50 | LIVE | `LibcdDirCache::cacheFile` | game/cd/libcd_dir_cache.cpp:68 |  |  |
| 0x8008BF50 | LIVE | `LibcdNative::cacheFile` | game/cd/libcd_native.cpp:17 |  |  |
| 0x80090160 | LIVE | `Sequencer::channelStreamAccumulate` | game/audio/sequencer.cpp:1612 |  | channelStreamAccumulate — true leaf (no stack frame). Faithful to gues… |
| 0x800909C0 | LIVE | `Sequencer::frameTick` | game/audio/sequencer.cpp:138 |  | libsnd per-VBlank tick wrapper. WIDE-RE DRAFT, UNWIRED (see header). |
| 0x80090BD0 | LIVE | `Sequencer::seqChannelDispatch` | game/audio/sequencer.cpp:233 | 0x80090E40 0x80091050 0x80091910 0x80092080 | SsSeqCalled — the per-VBlank sequence/channel scheduler. Faithful to |
| 0x80090E40 | LIVE | `Sequencer::channelPitchSlideTick` | game/audio/sequencer.cpp:591 |  | channelPitchSlideTick — pitch-slide/portamento per-tick interpolator (… |
| 0x80091050 | LIVE | `Sequencer::channelReleaseClear` | game/audio/sequencer.cpp:173 |  | "release"/note-off housekeeping: zeroes the per-channel status byte at… |
| 0x800910F0 | LIVE | `Sequencer::channelPitchSelectDispatch` | game/audio/sequencer.cpp:158 |  | thin arg-repacking wrapper: sign-extend (seq,chan) to 32-bit and tail-… |
| 0x80091810 | LIVE | `Sequencer::channelVoiceKeyOn` | game/audio/sequencer.cpp:1678 |  | channelVoiceKeyOn — true leaf (no stack frame). Faithful to guest 0x80… |
| 0x80091910 | LIVE | `Sequencer::channelStopFlagSet` | game/audio/sequencer.cpp:207 |  | sets the per-channel status byte at +20 to 1, clears flags bit3 (value… |
| 0x80091970 | LIVE | `Sequencer::channelNoteInit` | game/audio/sequencer.cpp:987 | 0x800931A0 | channelNoteInit — per-channel note retrigger (SsSeqCalled flags bit2 r… |
| 0x80092080 | LIVE | `Sequencer::channelEnvelopeRampTick` | game/audio/sequencer.cpp:783 |  | channelEnvelopeRampTick — ADSR/envelope ramp (SsSeqCalled flags bit6 A… |
| 0x80092310 | LIVE | `Sequencer::channelToneRecordCopy` | game/audio/sequencer.cpp:1744 |  | channelToneRecordCopy — stack frame present (sp-32, spill r16@16/r17@2… |
| 0x80092420 | LIVE | `Sequencer::channelToneRecordCopyWide` | game/audio/sequencer.cpp:1812 |  | channelToneRecordCopyWide — stack frame present (sp-32, spill r16@16/r… |
| 0x800931C0 | LIVE | `Sequencer::voiceStateFlush` | game/audio/sequencer.cpp:2371 | 0x80097E10 0x80098DB0 0x80098F90 0x80099970 0x8009A1D0 | the sound driver's per-frame SPU voice-state flush. 12,000 substrate d… |
| 0x80094150 | LIVE | `Sequencer::voiceAllocateOrSteal` | game/audio/sequencer.cpp:1935 |  | voiceAllocateOrSteal — true leaf (no stack frame). Faithful to guest 0… |
| 0x80094474 | LIVE | `Sequencer::channelNotePeriodCompute` | game/audio/sequencer.cpp:2183 |  | channelNotePeriodCompute — true leaf (no stack frame). Faithful to gue… |
| 0x80094B50 | LIVE | `Sequencer::channelKeyRegisterMerge` | game/audio/sequencer.cpp:496 |  | channelKeyRegisterMerge — true leaf (no stack frame). Faithful to gues… |
| 0x80095530 | LIVE | `Sequencer::channelVoiceRegisterWrite` | game/audio/sequencer.cpp:1138 |  | channelVoiceRegisterWrite — the "SPU voice-register write leaf" channe… |
| 0x80095A9C | LIVE | `Sequencer::channelVolumeSnapshot` | game/audio/sequencer.cpp:469 |  | channelVolumeSnapshot — true leaf (no stack frame). Faithful to guest … |
| 0x80095B90 | LIVE | `Sequencer::channelKeyEventScan` | game/audio/sequencer.cpp:537 |  | channelKeyEventScan — stack frame present (sp-32, spill ra/s16/s17/s18… |
| 0x800962B0 | LIVE | `Sequencer::channelVoiceSelectPrep` | game/audio/sequencer.cpp:1066 |  | channelVoiceSelectPrep — called mid-loop by channelVoiceRegisterWrite(… |
| 0x80096370 | LIVE | `Font::bank2Store` | game/ui/font.cpp:94 |  | font-bank2 store. `*kFontBank2Addr(sb) = bank; jr ra`. Leaf; does NOT … |
| 0x800963A0 | LIVE | `Font::bankSelect` | game/ui/font.cpp:81 |  | font-bank selector. If ((bank-1)&0xff) < 24, store the bank byte at |
| 0x80096878 | LIVE | `bav_cleanup_tail` | game/ui/bav_loader.cpp:90 |  | cleanup tail at 0x80096878: release lock (a0=0 path) + decrement refco… |
| 0x80099450 | LIVE | `bav_lock_ready` | game/ui/bav_loader.cpp:80 |  | -- lock helpers (FUN_80099478 / FUN_80099450), inlined --- |
| 0x80099478 | LIVE | `bav_lock_ready` | game/ui/bav_loader.cpp:80 |  | -- lock helpers (FUN_80099478 / FUN_80099450), inlined --- |
| 0x800998E4 | LIVE | `AreaSlots::classifySlotStates` | game/world/area_slots.cpp:336 |  | ORACLE: guest 0x800998E4 |
| 0x8009A3E0 | LIVE | `Str::copyBytes` | game/core/str.cpp:37 |  | memcpy(dst, src, n). RE from authenticated executable/overlay evidence… |
| 0x8009A3E0 | LIVE | `ov_copyBytes` | game/core/str.cpp:72 |  |  |
| 0x8009A640 | LIVE | `Str::compareBytes` | game/core/str.cpp:79 |  | FUN_0x8009A640 — byte compare, sibling of the memcpy already owned her… |
| 0x800A33C8 | LIVE | `tbl_strp` | game/ai/beh_cube_text_spawn.cpp:45 |  | string-table entry pointer: mem32(0x800a33c8 + (node[0x60]*3 << 2) + 4… |
| 0x800BE224 | LIVE | `MusicCoord::musicFadeIn` | game/audio/music_coord.cpp:48 |  | PC-added helper (NOT a port of any FUN_XXXX): snap the game's CD-volum… |
| 0x800BED80 | LIVE | `MusicCoord::dialogToneActive` | game/audio/music_coord.cpp:34 |  |  |
| 0x800BF842 | LIVE | `Engine::postRenderTick` | game/core/engine.cpp:2843 |  | Engine::postRenderTick — 3-state fx-trigger + countdown on byte 0x800B… |
| 0x801062E4 | LIVE | `Demo::stageMain` | game/scene/demo.cpp:554 | 0x800810F0 | DEMO stage entry (0x801062E4) — own the prologue PC-native, then hand … |
| 0x801062E4 | LIVE | `Demo::stageBodyFaithful` | game/scene/demo.cpp:1040 | 0x8001CF00 0x80044BD4 0x80045080 0x8005082C 0x80051F80 0x80052078 … |  |
| 0x8010637C | LIVE | `Engine::stagePrologue` | game/core/engine.cpp:2397 |  | GAME stage TOP-LEVEL ENTRY 0x8010637C — task-0's stage driver: a one-t… |
| 0x8010637C | ORPHAN | `Engine::stageBodyFaithful` | game/core/engine.cpp:2433 | 0x80051F80 0x801086E0 0x80108720 0x80108784 | pc_faithful GAME stage body (fiber task; see engine.h). Byte shape: |
| 0x801063C0 | LIVE | `Demo::s0` | game/scene/demo.cpp:397 |  | s0 0x801063C0 — run-once INIT then loaders; FALLS THROUGH into s1 same… |
| 0x801063F4 | LIVE | `Engine::frame` | game/core/engine.cpp:2335 |  | One native loop iteration of the guest body 0x801063F4: dispatch sm[0x… |
| 0x801063F4 | ORPHAN | `Engine::stageMain` | game/core/engine.cpp:2466 |  | OLD guest-loop entry (prologue + guest-continuation into the guest loo… |
| 0x8010641C | LIVE | `Demo::s1` | game/scene/demo.cpp:75 | 0x80106F80 | s1 0x8010641C — wait/advance: v0 = inner menu input machine 0x80106f80… |
| 0x80106464 | LIVE | `Demo::s2` | game/scene/demo.cpp:97 | 0x8001CF2C 0x8010696C | s2 0x80106464 — sub-machine v0 = 0x8010696c(). Outcome 1 -> go to s7 (… |
| 0x80106478 | LIVE | `Engine::areaLoadState` | game/core/engine.cpp:228 | 0x8001CF2C 0x8004D8B0 0x80078824 0x8007E8DC 0x8007ED5C 0x8007EE74 … | Engine::areaLoadState — native ownership of FUN_80106478 (the |
| 0x8010649C | LIVE | `StartBinStage::runFaithful` | game/scene/start_bin_stage.cpp:111 |  | The COMPLETE overlay guest 0x8010649C task body. Frame: sp -= 456; Loa… |
| 0x801064E8 | LIVE | `Demo::s3` | game/scene/demo.cpp:135 | 0x800750D8 0x80106AC4 | s3 0x801064E8 — sub-machine v0 = 0x80106ac4() (mirror of 0x8010696c). … |
| 0x80106580 | LIVE | `demo_frame_s4` | game/scene/demo.cpp:839 |  | Substate s4 (0x80106580) — LOAD GAME. The body runs the load sub-machi… |
| 0x801065DC | LIVE | `demo_frame_s5` | game/scene/demo.cpp:821 |  | Substate s5 (0x801065DC) — LEAVE DEMO: the body is `jal 0x80052078(2)`… |
| 0x801065EC | LIVE | `Demo::s6` | game/scene/demo.cpp:331 | 0x8007B45C 0x80106690 0x80106824 | s6 0x801065EC — page sub-machine 0x8007b45c(); if sm[0x50]==3 fire the… |
| 0x8010696C | LIVE | `Demo::s2SubMachine` | game/scene/demo.cpp:244 | 0x80106690 0x80106824 |  |
| 0x80106AC4 | LIVE | `Demo::s3SubMachine` | game/scene/demo.cpp:167 | 0x80106690 0x80106824 |  |
| 0x80106AC4 | LIVE | `Demo::registerOverrides` | game/scene/demo.cpp:323 |  |  |
| 0x80106F80 | LIVE | `demo_menu_machine` | game/scene/demo.cpp:606 | 0x8001CF00 0x8008CCE0 0x8008CD40 0x8009C820 0x8009C8BC 0x80106F80 | s1's inner menu input machine (0x80106F80): an 8-way state machine on … |
| 0x80107AFC | LIVE | `FieldTransition::main` | game/scene/field_transition.cpp:54 |  | the MAIN door/sub-scene transition (sm[0x4c]==1..4). sm[0x4e]: |
| 0x80107D3C | LIVE | `FieldTransition::d3c` | game/scene/field_transition.cpp:123 |  | transition variant (sm[0x4c]==5/6). sm[0x4e]: 0 load, 1 effect |
| 0x80107E20 | LIVE | `FieldTransition::e20` | game/scene/field_transition.cpp:149 |  | transition variant (sm[0x4c]==7). sm[0x4e]: 0 setup+load, 1 |
| 0x80107F3C | LIVE | `FieldTransition::f3c` | game/scene/field_transition.cpp:181 |  | transition variant (sm[0x4c]==8), a 7-state machine. NB case 0 |
| 0x8010810C | LIVE | `Engine::submitPage810c` | game/core/engine.cpp:471 | 0x801084F8 | page-1 dim-fade branch (task+0x6B == 1, "draw main pause menu" — |
| 0x801086E0 | LIVE | `Engine::stageAreaInit` | game/core/engine.cpp:137 |  | sm[0x48] == 0 — area INIT: advance to running (sm[0x48]=2), reset the |
| 0x80108720 | LIVE | `Engine::stageResumeInit` | game/core/engine.cpp:157 |  | sm[0x48] == 1 — area RESUME-INIT (re-enter a running area, sub-mode 1)… |
| 0x8010882C | LIVE | `Engine::stageRunning` | game/core/engine.cpp:550 |  | sm[0x48]==2 RUNNING, per-frame variant: dispatch sm[0x4a] handler. han… |
| 0x8010882C | LIVE | `Engine::submode0` | game/core/engine.cpp:602 | 0x80109450 | GAME sub-mode-0 bridge 0x8010882c (sm[0x4c]/sm[0x4e] dispatch) — nativ… |
| 0x801088D8 | LIVE | `Engine::submode1Faithful` | game/core/engine.cpp:2215 | 0x80044BD4 0x8005245C 0x80107230 0x8010766C 0x80107790 | pc_faithful walkable-field area machine — mirror of overlay guest 0x80… |
| 0x80108A60 | LIVE | `FieldTransition::step` | game/scene/field_transition.cpp:246 |  | sm[0x4a]==5 transition dispatcher on sm[0x4c]. 0/9 = done |
| 0x80108B0C | LIVE | `Engine::devTeleportApply` | game/core/engine.cpp:1000 |  | FIELD PER-FRAME UPDATE 0x80108b0c — native control flow (the field fra… |
| 0x80109164 | LIVE | `Sop::areaLoad` | game/scene/sop.cpp:85 | 0x8001DC40 | Owned synchronous area-DATA load (replaces the body of LAB_80109164 |
| 0x80109164 | LIVE | `Sop::areaLoadFaithful` | game/scene/sop.cpp:870 | 0x8001DC40 0x80044E84 | pc_faithful SOP area-load task body — mirror of overlay guest 0x801091… |
| 0x801092B4 | LIVE | `Sop::fieldUpdate` | game/scene/sop.cpp:507 |  | SOP per-frame FIELD UPDATE — native ownership of FUN_801092b4 (decomp |
| 0x80109450 | LIVE | `Sop::fieldMode` | game/scene/sop.cpp:592 |  | SOP FIELD-MODE MACHINE — native ownership of FUN_80109450 (decomp |
| 0x8010957C | LIVE | `AreaFadeSequencer::step` | game/scene/area_fade_sequencer.cpp:10 | 0x8007E9C8 0x8010CC68 0x8010D030 | A0L per-node fade sequencer run by `Engine::fieldRun` while sm[0x4e]==0xb. |
| 0x801099B4 | LIVE | `UnlitModelEmitter::gt3` (SOP) | game/render/unlit_model_emitter.cpp:98 |  | SOP unlit GT3; A0B and A0C copy its body. |
| 0x80109C80 | LIVE | `UnlitModelEmitter::gt4` (SOP) | game/render/unlit_model_emitter.cpp:99 |  | SOP unlit GT4. |
| 0x80109FE0 | LIVE | `SopGround::draw` | game/render/sop_ground.cpp:28 | 0x801099B4 0x80109C80 | SOP ground blocks; producer, object = block |
| 0x8010A0E0 | LIVE | `Sop::scenePrepass` | game/scene/sop.cpp:434 |  | SOP scene cam-frustum prepass — native ownership of FUN_8010A0E0 (Ghid… |
| 0x8010A3AC | LIVE | `Sop::sceneGridGather` | game/scene/sop.cpp:300 |  | sceneGridGather — native port of guest FUN_8010A3AC (Ghidra decomp |
| 0x8010A3AC | LIVE | `UnlitModelEmitter::gt3` (A0J) | game/render/unlit_model_emitter.cpp:143 |  | SOP GT3 with a near clamp (kSopNearLifted). |
| 0x8010A69C | LIVE | `UnlitModelEmitter::gt4` (A0J) | game/render/unlit_model_emitter.cpp:147 |  | SOP GT4 with a near clamp (kSopNearLifted). |
| 0x8010AA4C | LIVE | `OverlayGroundGt3Gt4::gt3` (A0L) | game/render/overlay_ground_gt3gt4.cpp:132 |  | Ground GT3 copy (kA0L). |
| 0x8010AB20 | LIVE | `UnlitModelEmitter::gt3` (A0J) | game/render/unlit_model_emitter.cpp:143 |  | Flagged unlit GT3 (kFlagged). |
| 0x8010AB38 | LIVE | `beh_sop_overlay_shadow` | game/ai/sop_overlay_shadow.cpp:80 |  |  |
| 0x8010ACFC | LIVE | `beh_sop_intro_pilot` | game/ai/beh_sop_intro_pilot.cpp:120 |  |  |
| 0x8010AD40 | LIVE | `OverlayGroundGt3Gt4::gt4` (A0L) | game/render/overlay_ground_gt3gt4.cpp:278 |  | Ground GT4 copy (kA0L). |
| 0x8010ADC4 | LIVE | `UnlitModelEmitter::gt4` (A0J) | game/render/unlit_model_emitter.cpp:147 |  | Flagged unlit GT4 (kFlagged). |
| 0x8010AE30 | LIVE | `native_sop_overlay_shadow_spawn` | game/ai/sop_overlay_shadow.cpp:62 |  | (parent) -> node ptr (0 on pool exhaustion). |
| 0x8010AF58 | LIVE | `UnlitModelEmitter::gt3` (A0H) | game/render/unlit_model_emitter.cpp:143 |  | SOP GT3 with a near clamp (kSopNearWide). |
| 0x8010AF60 | LIVE | `sopBeatAdvanceWalk` | game/ai/sop_intro_events.cpp:71 |  | ======================================================================… |
| 0x8010B078 | LIVE | `sopBeatAdvanceNarration` | game/ai/sop_intro_events.cpp:135 |  | ======================================================================… |
| 0x8010B11C | LIVE | `sopOrbitPathStep` | game/ai/sop_intro_events.cpp:188 | 0x80077C40 | ======================================================================… |
| 0x8010B240 | LIVE | `UnlitModelEmitter::gt4` (A0H) | game/render/unlit_model_emitter.cpp:147 |  | SOP GT4 with a near clamp (kSopNearWide). |
| 0x8010B2D4 | LIVE | `sopIntroEffectTick` | game/ai/sop_intro_events.cpp:326 | 0x800519E0 0x8007778C 0x80077C40 | ======================================================================… |
| 0x8010B3DC | LIVE | `UnlitModelEmitter::gt3` (A0I) | game/render/unlit_model_emitter.cpp:143 |  | SOP GT3 with a near clamp (kSopNear). |
| 0x8010B44C | LIVE | `sopIntroEffectSpawn` | game/ai/sop_intro_events.cpp:277 |  | ======================================================================… |
| 0x8010B588 | LIVE | `overlay_subtick` | game/ai/beh_sop_intro_lifted.cpp:70 | 0x8010B588 | (sopLiftedSubtick, sop_intro_events.cpp) is VERIFIED + WIRED (2026-07-… |
| 0x8010B588 | LIVE | `sopLiftedSubtick` | game/ai/sop_intro_events.cpp:542 |  | GUEST FRAME (2026-07-10 §9 fix): overlay guest 0x8010B588 pushes `addi… |
| 0x8010B6BC | LIVE | `UnlitModelEmitter::gt3` (A0H) | game/render/unlit_model_emitter.cpp:143 |  | Flagged unlit GT3 (kFlagged). |
| 0x8010B6C4 | LIVE | `UnlitModelEmitter::gt4` (A0I) | game/render/unlit_model_emitter.cpp:147 |  | SOP GT4 with a near clamp (kSopNear). |
| 0x8010B798 | LIVE | `beh_sop_intro_lifted` | game/ai/beh_sop_intro_lifted.cpp:118 |  |  |
| 0x8010B960 | LIVE | `UnlitModelEmitter::gt4` (A0H) | game/render/unlit_model_emitter.cpp:147 |  | Flagged unlit GT4 (kFlagged). |
| 0x8010B990 | LIVE | `beh_sop_intro_narration` | game/ai/beh_sop_intro_narration.cpp:154 |  |  |
| 0x8010BB40 | LIVE | `UnlitModelEmitter::gt3` (A0I) | game/render/unlit_model_emitter.cpp:143 |  | Flagged unlit GT3 (kFlagged). |
| 0x8010BC40 | LIVE | `UnlitModelEmitter::gt3` (A0G) | game/render/unlit_model_emitter.cpp:143 |  | SOP GT3 with a near clamp (kSopNear). |
| 0x8010BDE4 | LIVE | `UnlitModelEmitter::gt4` (A0I) | game/render/unlit_model_emitter.cpp:147 |  | Flagged unlit GT4 (kFlagged). |
| 0x8010BEAC | LIVE | `beh_orbit_spark_effect` | game/ai/sop_intro_events.cpp:581 |  | ======================================================================… |
| 0x8010BF28 | LIVE | `UnlitModelEmitter::gt4` (A0G) | game/render/unlit_model_emitter.cpp:147 |  | SOP GT4 with a near clamp (kSopNear). |
| 0x8010C26C | LIVE | `TileGridLayer::emitSop` | game/render/tile_grid_layer.cpp:251 | 0x80083DE0 0x80081218 | SOP tile grid + CLUT palette cycles; producer, object = map cell |
| 0x8010E258 | LIVE | `ActorObjectContact::resolveHitOrProximity` | game/ai/actor_object_contact.cpp:115 |  | ORACLE: overlay guest 0x8010E258 |
| 0x8010E904 | LIVE | `ActorTomba::postFrameWaterCheck` | game/player/actor_tomba.cpp:597 |  | ======================================================================… |
| 0x8010EA80 | LIVE | `ActorBump::respondToContact` | game/ai/actor_bump.cpp:100 |  | ORACLE: overlay guest 0x8010EA80 |
| 0x801103F4 | LIVE | `UnlitModelEmitter::gt3` (A0A) | game/render/unlit_model_emitter.cpp:143 |  | Flagged unlit GT3 (kFlagged). |
| 0x80110698 | LIVE | `UnlitModelEmitter::gt4` (A0A) | game/render/unlit_model_emitter.cpp:147 |  | Flagged unlit GT4 (kFlagged). |
| 0x80112188 | LIVE | `ActorMeleeEngage::doIt` | game/ai/actor_melee_engage.cpp:30 | 0x80022C78 0x80055844 0x80084080 |  |
| 0x80112188 | LIVE | `ActorMeleeEngage::registerOverrides` | game/ai/actor_melee_engage.cpp:334 |  |  |
| 0x80112A24 | LIVE | `UnlitModelEmitter::gt3` (A0B) | game/render/unlit_model_emitter.cpp:98 |  | SOP's GT3 body. |
| 0x80112A60 | LIVE | `aux_list_walk` | game/ai/area_seaside_perframe.cpp:73 |  | Walk the aux render list, dispatching FUN_80112A60(item) per item type… |
| 0x80112CF0 | LIVE | `UnlitModelEmitter::gt4` (A0B) | game/render/unlit_model_emitter.cpp:99 |  | SOP's GT4 body. |
| 0x80112DEC | LIVE | `OverlayGt3Gt4::gt3` (A0L) | game/render/overlay_gt3gt4.cpp:177 |  | A00's GT3 body. |
| 0x80112FBC | LIVE | `OverlayGt3Gt4::gt4` (A0L) | game/render/overlay_gt3gt4.cpp:291 |  | A00's GT4 body. |
| 0x801130C4 | LIVE | `ActorTomba::postInteractWalk` | game/player/actor_tomba.cpp:441 |  | ======================================================================… |
| 0x80113150 | LIVE | `UnlitModelEmitter::gt3` (A0B) | game/render/unlit_model_emitter.cpp:143 |  | Flagged unlit GT3 (kFlagged). |
| 0x801133F4 | LIVE | `UnlitModelEmitter::gt4` (A0B) | game/render/unlit_model_emitter.cpp:147 |  | Flagged unlit GT4 (kFlagged). |
| 0x80113748 | LIVE | `UnlitModelEmitter::gt3` (A0D) | game/render/unlit_model_emitter.cpp:143 |  | Flagged unlit GT3 (kFlagged). |
| 0x80113788 | LIVE | `UnlitModelEmitter::gt3` (A0C) | game/render/unlit_model_emitter.cpp:98 |  | SOP's GT3 body. |
| 0x801139EC | LIVE | `UnlitModelEmitter::gt4` (A0D) | game/render/unlit_model_emitter.cpp:147 |  | Flagged unlit GT4 (kFlagged). |
| 0x80113A54 | LIVE | `UnlitModelEmitter::gt4` (A0C) | game/render/unlit_model_emitter.cpp:99 |  | SOP's GT4 body. |
| 0x80113C5C | LIVE | `Behaviors::areaSeasidePerframe` | game/ai/area_seaside_perframe.cpp:102 | 0x8002288C |  |
| 0x80113EB4 | LIVE | `UnlitModelEmitter::gt3` (A0C) | game/render/unlit_model_emitter.cpp:143 |  | Flagged unlit GT3 (kFlagged). |
| 0x80114158 | LIVE | `UnlitModelEmitter::gt4` (A0C) | game/render/unlit_model_emitter.cpp:147 |  | Flagged unlit GT4 (kFlagged). |
| 0x801141B0 | LIVE | `TileGridLayer::emitUnbiased` (A0B) | game/render/tile_grid_layer.cpp:263 |  | Tile grid with no V bias; producer, object = map cell. |
| 0x801142EC | LIVE | `TileGridLayer::emitUnbiased` (A0A) | game/render/tile_grid_layer.cpp:263 |  | Tile grid with no V bias; producer, object = map cell. |
| 0x80114458 | LIVE | `UnlitModelEmitter::gt3` (A0E) | game/render/unlit_model_emitter.cpp:143 |  | Flagged unlit GT3 (kFlagged). |
| 0x801146FC | LIVE | `UnlitModelEmitter::gt4` (A0E) | game/render/unlit_model_emitter.cpp:147 |  | Flagged unlit GT4 (kFlagged). |
| 0x80114E74 | LIVE | `ActorTomba::type4GuardedCheck` | game/player/actor_tomba.cpp:377 |  | type-4 guarded proximity. |
| 0x8011534C | LIVE | `TileGridLayer::scrollStep` | game/render/tile_grid_layer.cpp:175 |  |  |
| 0x80115598 | LIVE | `TileGridLayer::emit` | game/render/tile_grid_layer.cpp:245 | 0x80083DE0 |  |
| 0x801157CC | LIVE | `UnlitModelEmitter::gt3` (A0F) | game/render/unlit_model_emitter.cpp:143 |  | Flagged unlit GT3 (kFlagged). |
| 0x80115A70 | LIVE | `UnlitModelEmitter::gt4` (A0F) | game/render/unlit_model_emitter.cpp:147 |  | Flagged unlit GT4 (kFlagged). |
| 0x80116778 | LIVE | `TileGridLayer::emitUnbiased` (A0F) | game/render/tile_grid_layer.cpp:263 |  | Tile grid with no V bias; producer, object = map cell. |
| 0x80116904 | LIVE | `RainStreaks::draw` | game/render/rain_streaks.cpp:75 | 0x80083DE0 0x80084220 0x80084660 0x80084690 | A08 rain streaks; producer, element = drop |
| 0x80116B9C | LIVE | `TileGridLayer::emitUnbiased` (A0D) | game/render/tile_grid_layer.cpp:263 |  | Tile grid with no V bias; producer, object = map cell. |
| 0x80117658 | LIVE | `beh_prng_velocity_machine` | game/ai/beh_prng_velocity_machine.cpp:568 |  |  |
| 0x801178A4 | LIVE | `whiteFlashPhaseRamp` | game/ai/beh_a06_multi_actor.cpp:60 |  | the 5-phase white-flash phase ramp SM. See the file header for the pha… |
| 0x80117AAC | LIVE | `whiteFadeHold` | game/ai/beh_a06_multi_actor.cpp:139 |  | 3-state fade-hold-fade-back companion to whiteFlashPhaseRamp. |
| 0x80118240 | LIVE | `beh_typed_init_exit_poker` | game/ai/beh_typed_init_exit_poker.cpp:54 |  |  |
| 0x80118690 | LIVE | `shared_8690` | game/ai/beh_typed_init_exit_poker.cpp:44 |  | Shared block @0x80118690: FUN_80051D90(node[0x10], a1_buf, 0x1F8000C0)… |
| 0x801189E8 | LIVE | `beh_a06_multi_actor` | game/ai/beh_a06_multi_actor.cpp:638 |  | The guest body 0x801189E8 is ONE function that descends sp by 32 and s… |
| 0x80118B10 | LIVE | `AssemblyRider::rideSlotAndReactToStroke` | game/ai/assembly_rider.cpp:167 |  | ORACLE: overlay guest 0x80118B10 |
| 0x8011C164 | LIVE | `beh_typed_variant_router` | game/ai/beh_typed_variant_router.cpp:110 |  |  |
| 0x8011CBD0 | LIVE | `beh_node3_router` | game/ai/beh_node3_router.cpp:37 |  |  |
| 0x8011D578 | LIVE | `beh_variant_actor_sm` | game/ai/beh_variant_actor_sm.cpp:49 |  |  |
| 0x8011D988 | LIVE | `beh_actor_move_sm` | game/ai/beh_actor_move_sm.cpp:57 |  |  |
| 0x80121978 | LIVE | `beh_id_routed_dispatch` | game/ai/beh_id_routed_dispatch.cpp:120 |  |  |
| 0x80122BF4 | LIVE | `beh_id_routed_offset_point` | game/ai/beh_id_routed_dispatch.cpp:68 | 0x800844C0 | FUN_0x80122BF4 — keeps a point pinned 119 world units ABOVE a linked o… |
| 0x80123E9C | LIVE | `ReleaseTriggerMotion::hoverBobCycle` | game/ai/release_trigger_motion.cpp:73 | 0x80077B5C | ----------------------------------------------------------------------… |
| 0x801241BC | LIVE | `ReleaseTriggerMotion::leaderFollowSync` | game/ai/release_trigger_motion.cpp:144 | 0x80051D90 0x80123C94 0x8012400C | ----------------------------------------------------------------------… |
| 0x80124328 | LIVE | `ReleaseTriggerMotion::xSweepCycle` | game/ai/release_trigger_motion.cpp:575 |  | a per-frame X-sweep cycle on the release-trigger object. 6,355 substra… |
| 0x801244E8 | LIVE | `ReleaseTriggerMotion::driftReposition` | game/ai/release_trigger_motion.cpp:203 | 0x80051794 0x80077B5C 0x80084360 0x800847F0 0x80124328 | ----------------------------------------------------------------------… |
| 0x801246A4 | LIVE | `OverlayGroundGt3Gt4::gt3` (A02) | game/render/overlay_ground_gt3gt4.cpp:132 |  | Ground GT3 copy (kFarthest). |
| 0x801246B4 | LIVE | `ReleaseTriggerMotion::arcSwoopMotion` | game/ai/release_trigger_motion.cpp:269 | 0x80077B5C | ----------------------------------------------------------------------… |
| 0x8012496C | LIVE | `OverlayGroundGt3Gt4::gt4` (A02) | game/render/overlay_ground_gt3gt4.cpp:278 |  | Ground GT4 copy (kFarthest). |
| 0x801249D4 | LIVE | `ReleaseTriggerMotion::doubleArcMotion` | game/ai/release_trigger_motion.cpp:383 | 0x80077B5C | ----------------------------------------------------------------------… |
| 0x80124C6C | LIVE | `ReleaseTriggerMotion::circleOrbitMotion` | game/ai/release_trigger_motion.cpp:479 | 0x80077B5C | ----------------------------------------------------------------------… |
| 0x80124E74 | LIVE | `beh_jumptable_release_trigger` | game/ai/beh_jumptable_release_trigger.cpp:134 | 0x8004B0D8 0x8004DAEC 0x80051D90 0x80077B5C 0x80123E9C 0x801241BC … |  |
| 0x80125E0C | LIVE | `beh_pure_substate_dispatch` | game/ai/beh_pure_substate_dispatch.cpp:40 |  |  |
| 0x80125FE0 | LIVE | `TiltFollower::applyHalvedOwnerPartPitch` | game/ai/tilt_follower.cpp:90 |  | ORACLE: overlay guest 0x80125FE0 |
| 0x80127420 | LIVE | `beh_arm_countdown_if_linked_ready_80127420` | game/ai/beh_toy_spawn_family.cpp:95 |  | (obj) — arm a 20-frame countdown if the linked object (obj[+0x10]'s ta… |
| 0x801274BC | LIVE | `beh_distance_band_predicate_801274bc` | game/ai/beh_toy_spawn_family.cpp:113 |  | (obj) — a distance-band predicate. `row` is looked up from a per-slot … |
| 0x80127510 | LIVE | `beh_spawn_toy_child_type2_80127510` | game/ai/beh_toy_spawn_family.cpp:212 |  | (owner, subtype) — spawn a child whose sub-behavior is picked by `subt… |
| 0x8012763C | LIVE | `beh_spawn_toy_child_type4_8012763c` | game/ai/beh_toy_spawn_family.cpp:165 | 0x8004D650 | (owner) — spawn a type-4 companion child, then feed GBASE's mode byte … |
| 0x80127720 | LIVE | `beh_spawn_toy_child_type5_80127720` | game/ai/beh_toy_spawn_family.cpp:135 |  | (owner) — spawn a type-5 companion child via the legacy allocator, no … |
| 0x80127798 | LIVE | `beh_area_transition_machine` | game/ai/beh_area_transition_machine.cpp:198 | 0x80041194 |  |
| 0x80127C58 | LIVE | `cutsceneDirector` | game/ai/beh_a08_scene_actor.cpp:196 | 0x80081218 0x8013DD48 | ── FUN_80127C58 — the 10-state cutscene director ─────────────────────… |
| 0x80127C9C | LIVE | `dat_tail` | game/ai/beh_area_transition_machine.cpp:76 |  |  |
| 0x80127CD0 | LIVE | `cd0_tail` | game/ai/beh_area_transition_machine.cpp:70 |  |  |
| 0x801280D0 | LIVE | `beh_a08_scene_actor` | game/ai/beh_a08_scene_actor.cpp:786 |  |  |
| 0x801281B8 | LIVE | `RopeSwing::swingTickAndBendSegments` | game/ai/rope_swing.cpp:90 |  | ORACLE: overlay guest 0x801281B8 |
| 0x80128760 | LIVE | `beh_linked_advance_branch` | game/ai/beh_linked_advance_branch.cpp:39 |  |  |
| 0x80129BAC | LIVE | `LitModelEmitter::gt3` (A08) | game/render/lit_model_emitter.cpp:137 |  | Lit GT3 list emitter; the A05 and A07 copies share the native. |
| 0x80129C00 | LIVE | `beh_anim_trigger_gates` | game/ai/beh_anim_trigger_gates.cpp:44 |  |  |
| 0x8012A06C | LIVE | `LitModelEmitter::gt4` (A08) | game/render/lit_model_emitter.cpp:141 |  | Lit GT4 list emitter. |
| 0x8012A0B8 | LIVE | `beh_box_seed_phase_gate` | game/ai/beh_box_seed_phase_gate.cpp:46 |  |  |
| 0x8012C7E0 | LIVE | `OverlayGroundGt3Gt4::gt3` (A07) | game/render/overlay_ground_gt3gt4.cpp:132 |  | Ground GT3 copy (kFarthest). |
| 0x8012CAA8 | LIVE | `OverlayGroundGt3Gt4::gt4` (A07) | game/render/overlay_ground_gt3gt4.cpp:278 |  | Ground GT4 copy (kFarthest). |
| 0x8012CDF4 | LIVE | `LitModelEmitter::gt3` (A07) | game/render/lit_model_emitter.cpp:137 |  | A08's lit GT3 body. |
| 0x8012D27C | LIVE | `SwaySchedule::advanceRateThenSway` | game/ai/sway_schedule.cpp:134 |  | ORACLE: overlay guest 0x8012D27C |
| 0x8012D2B4 | LIVE | `LitModelEmitter::gt4` (A07) | game/render/lit_model_emitter.cpp:141 |  | A08's lit GT4 body. |
| 0x8012D404 | LIVE | `beh_cull_tick_render` | game/ai/beh_cull_tick_render.cpp:48 |  |  |
| 0x8012D4EC | LIVE | `beh_jumptable_flag_gate` | game/ai/beh_jumptable_flag_gate.cpp:123 |  |  |
| 0x8012D6AC | LIVE | `step_node18` | game/ai/beh_jumptable_flag_gate.cpp:71 |  | LAB_8012d7d8 (and its inline copy at 0x8012d6ac): node[0x18,0x19,0x1a]… |
| 0x8012D78C | LIVE | `advance_node32` | game/ai/beh_jumptable_flag_gate.cpp:85 |  | LAB_8012d78c: node[0x32]+=4; if (int16)node[0x32] < -0x64e -> tail_set… |
| 0x8012D7D8 | LIVE | `step_node18` | game/ai/beh_jumptable_flag_gate.cpp:71 |  | LAB_8012d7d8 (and its inline copy at 0x8012d6ac): node[0x18,0x19,0x1a]… |
| 0x8012D7FC | LIVE | `despawn_flag_block` | game/ai/beh_jumptable_flag_gate.cpp:56 |  | LAB_8012d82c..8012d840: set bit (4 if node[3]==0 else 8) in DAT_800bf9… |
| 0x8012D844 | LIVE | `tail_set1_and_render` | game/ai/beh_jumptable_flag_gate.cpp:48 |  | LAB_8012d844: v0=1; fall into 8012d848 (node[1]=1); then 8012d84c (jal… |
| 0x8012D848 | LIVE | `tail_set1_and_render` | game/ai/beh_jumptable_flag_gate.cpp:48 |  | LAB_8012d844: v0=1; fall into 8012d848 (node[1]=1); then 8012d84c (jal… |
| 0x8012D84C | LIVE | `tail_set1_and_render` | game/ai/beh_jumptable_flag_gate.cpp:48 |  | LAB_8012d844: v0=1; fall into 8012d848 (node[1]=1); then 8012d84c (jal… |
| 0x8012DA04 | LIVE | `beh_typed_anim_spawn` | game/ai/beh_typed_anim_spawn.cpp:44 |  |  |
| 0x8012EB54 | LIVE | `beh_substate_edge_orchestrator` | game/ai/beh_substate_edge_orchestrator.cpp:46 | 0x8012E8A8 0x8012ED84 0x8012F494 0x8012F5B4 0x8012FD88 0x80130524 … |  |
| 0x8012F8D8 | LIVE | `SwayModelEmitter::cueGt3` | game/render/sway_model_emitter.cpp:216 | 0x80083E80 0x80083F50 | A01 depth-cue GT3; sways through rsin/rcos. |
| 0x8013000C | LIVE | `SwayModelEmitter::cueGt4` | game/render/sway_model_emitter.cpp:242 |  | A01 depth-cue GT4. |
| 0x80130838 | LIVE | `SwayModelEmitter::swayGt3` | game/render/sway_model_emitter.cpp:181 | 0x80083E80 0x80083F50 | A01 sway GT3. |
| 0x80130D9C | LIVE | `SwayModelEmitter::scrollGt4` | game/render/sway_model_emitter.cpp:201 |  | A01 UV-scroll GT4. |
| 0x801311D0 | LIVE | `OverlayGt3Gt4::gt3` (A07) | game/render/overlay_gt3gt4.cpp:177 |  | A00's GT3 body. |
| 0x801313A0 | LIVE | `OverlayGt3Gt4::gt4` (A07) | game/render/overlay_gt3gt4.cpp:291 |  | A00's GT4 body. |
| 0x801316A8 | LIVE | `LitModelEmitter::gt3` (A01) | game/render/lit_model_emitter.cpp:137 |  | A08's lit GT3 body plus the hide and deepen flag bits. |
| 0x801316CC | LIVE | `SubstateEdgeLeaves::tickChildOscillators` | game/ai/substate_edge_native.cpp:19 | 0x80130D5C |  |
| 0x80131BB0 | LIVE | `LitModelEmitter::gt4` (A01) | game/render/lit_model_emitter.cpp:141 |  | A08's lit GT4 body plus the hide and deepen flag bits. |
| 0x80131D08 | LIVE | `beh_two_child_steer` | game/ai/beh_two_child_steer.cpp:47 |  |  |
| 0x80132400 | LIVE | `beh_single_child_cull` | game/ai/beh_single_child_cull.cpp:43 |  |  |
| 0x8013259C | LIVE | `beh_cull_substate_orchestrator` | game/ai/beh_cull_substate_orchestrator.cpp:51 | 0x8013272C 0x80132954 0x80132A88 0x80132D58 0x80132EDC 0x80133184 … |  |
| 0x80132690 | LIVE | `UnlitModelEmitter::gt3` (A01) | game/render/unlit_model_emitter.cpp:143 |  | A01 third-branch unlit GT3 (kA01Cue). |
| 0x801329C4 | LIVE | `UnlitModelEmitter::gt4` (A01) | game/render/unlit_model_emitter.cpp:147 |  | A01 third-branch unlit GT4 (kA01Cue). |
| 0x80133C14 | LIVE | `beh_typed_table_seed_gate` | game/ai/beh_typed_table_seed_gate.cpp:309 |  |  |
| 0x80133D6C | LIVE | `beh_twin_record_steer` | game/ai/beh_twin_record_steer.cpp:69 |  |  |
| 0x80134FD8 | LIVE | `beh_multi_record_phase_machine` | game/ai/beh_multi_record_phase_machine.cpp:65 |  |  |
| 0x801353C8 | LIVE | `common_tail` | game/ai/beh_multi_record_phase_machine.cpp:49 |  | COMMON TAIL (0x801353C8): node[8]++ / FUN_800517F8(node) / node[8]--. |
| 0x8013544C | LIVE | `LitModelEmitter::gt3` (A05) | game/render/lit_model_emitter.cpp:137 |  | A08's lit GT3 body. |
| 0x8013590C | LIVE | `LitModelEmitter::gt4` (A05) | game/render/lit_model_emitter.cpp:141 |  | A08's lit GT4 body. |
| 0x80135D64 | LIVE | `beh_quad_record_table_seed` | game/ai/beh_quad_record_table_seed.cpp:51 |  |  |
| 0x801360F4 | LIVE | `Spawn::spawnTypedChild` | game/world/spawn.cpp:453 |  | TYPED-CHILD SPAWN wrappers (A00 overlay, |
| 0x801360F4 | LIVE | `Spawn::spawnQuadRecordChild` | game/world/spawn.cpp:468 |  |  |
| 0x80136158 | LIVE | `beh_sine_motion_sfx` | game/ai/beh_sine_motion_sfx.cpp:55 | 0x8004766C 0x80048750 |  |
| 0x80136954 | LIVE | `beh_event_record_machine` | game/ai/beh_event_record_machine.cpp:62 |  |  |
| 0x80136D9C | LIVE | `beh_pure_inner_dispatch` | game/ai/beh_pure_inner_dispatch.cpp:38 |  |  |
| 0x801389C8 | LIVE | `AssemblyCompanion::composeRigAndApplyPartScales` | game/ai/assembly_companion.cpp:253 |  | AssemblyCompanion::composeRigAndApplyPartScales, guest FUN_801389C8 — … |
| 0x80138A64 | LIVE | `AssemblyCompanion::endCamHoldAndRearmOnStroke` | game/ai/assembly_companion.cpp:142 |  | ORACLE: overlay guest 0x80138A64 |
| 0x80138FC8 | LIVE | `beh_typed_jumptable_pair` | game/ai/beh_typed_jumptable_pair.cpp:69 | 0x8004ED94 0x80138B04 0x80138C70 |  |
| 0x801395C0 | LIVE | `beh_sibling_angle_track` | game/ai/beh_sibling_angle_track.cpp:61 |  |  |
| 0x80139728 | LIVE | `beh_a06_fade_flash_ramp_80139728` | game/ai/beh_a06_script_fades.cpp:100 |  |  |
| 0x80139838 | LIVE | `Spawn::spawnTypedChild` | game/world/spawn.cpp:453 |  | TYPED-CHILD SPAWN wrappers (A00 overlay, |
| 0x80139838 | LIVE | `Spawn::spawnSiblingAngleChild` | game/world/spawn.cpp:471 |  |  |
| 0x80139A28 | LIVE | `variant4Phase3` | game/ai/beh_a06_scripted_actor.cpp:140 |  | ── FUN_80139A28 — variant-4 case-3 sub-machine (inner script cycle) ──… |
| 0x80139C84 | LIVE | `sub801398E4` | game/ai/beh_a06_scripted_actor.cpp:236 |  | ── FUN_80139C84 — variant-4 outer sub-machine (5 states) ─────────────… |
| 0x8013A330 | LIVE | `beh_lift_platform` | game/ai/beh_lift_platform.cpp:59 |  |  |
| 0x8013A730 | LIVE | `Spawn::spawnTypedChild` | game/world/spawn.cpp:453 |  | TYPED-CHILD SPAWN wrappers (A00 overlay, |
| 0x8013A730 | LIVE | `Spawn::spawnLiftPlatformChild` | game/world/spawn.cpp:477 |  |  |
| 0x8013A900 | LIVE | `beh_child_trig_motion` | game/ai/beh_child_trig_motion.cpp:60 |  |  |
| 0x8013AA14 | LIVE | `beh_a06_scripted_actor` | game/ai/beh_a06_scripted_actor.cpp:506 |  |  |
| 0x8013AC34 | LIVE | `Spawn::spawnTypedChild` | game/world/spawn.cpp:453 |  | TYPED-CHILD SPAWN wrappers (A00 overlay, |
| 0x8013AC34 | LIVE | `Spawn::spawnChildTrigChild` | game/world/spawn.cpp:474 |  |  |
| 0x8013ADBC | LIVE | `beh_box_rearm_sub` | game/ai/beh_box_rearm_sub.cpp:46 |  |  |
| 0x8013AEF0 | LIVE | `beh_a06_spawn_follow_obj_8013AEF0` | game/ai/beh_a06_script_fades.cpp:191 |  | ── FUN_8013AEF0 — spawn a follow-obj and hook it ─────────────────────… |
| 0x8013AFD8 | LIVE | `beh_a06_sound_cmd_wait_8013AFD8` | game/ai/beh_a06_script_fades.cpp:220 | 0x800708B4 | ── FUN_8013AFD8 — kick a sound-command sequence and wait for scratchpa… |
| 0x8013B074 | LIVE | `beh_a06_spawn_subobj_8013B074` | game/ai/beh_a06_script_fades.cpp:257 | 0x8006CBA8 | ── FUN_8013B074 — spawn a subobj + set field/anim params ─────────────… |
| 0x8013B178 | LIVE | `beh_a06_fade_ramp_8013B178` | game/ai/beh_a06_script_fades.cpp:287 |  |  |
| 0x8013B274 | LIVE | `beh_a06_music_cue_8013B274` | game/ai/beh_a06_script_fades.cpp:325 |  |  |
| 0x8013B29C | LIVE | `beh_a06_timer_gate_8013B29C` | game/ai/beh_a06_script_fades.cpp:337 |  | ── FUN_8013B29C — 2-state (init + counted gate) primitive ────────────… |
| 0x8013B2E4 | LIVE | `beh_flagbit_timer_machine` | game/ai/beh_flagbit_timer_machine.cpp:60 |  |  |
| 0x8013B70C | LIVE | `drawInit` | game/ai/beh_seaside_prox_substate.cpp:181 | 0x8013B534 | ======================================================================… |
| 0x8013B868 | LIVE | `subA` | game/ai/beh_seaside_prox_substate.cpp:234 | 0x8006CBA8 0x8006E1C0 0x8006E1E4 | ======================================================================… |
| 0x8013BA44 | LIVE | `UnlitModelEmitter::gt3` (A06) | game/render/unlit_model_emitter.cpp:143 |  | SOP GT3 with lit depth code and hide bit 2 (kA06). |
| 0x8013BAB0 | LIVE | `subB` | game/ai/beh_seaside_prox_substate.cpp:314 | 0x8004766C 0x80048750 | ======================================================================… |
| 0x8013BCC8 | LIVE | `subC` | game/ai/beh_seaside_prox_substate.cpp:377 | 0x80027144 0x8003116C 0x8006E1C0 0x8006E1E4 0x8009A450 | ======================================================================… |
| 0x8013BD40 | LIVE | `UnlitModelEmitter::gt4` (A06) | game/render/unlit_model_emitter.cpp:147 |  | SOP GT4 with lit depth code and hide bit 2 (kA06). |
| 0x8013C0BC | LIVE | `modeArm` | game/ai/beh_seaside_prox_substate.cpp:133 | 0x80081218 | ======================================================================… |
| 0x8013C0D8 | LIVE | `LitModelEmitter::gt3` (A06) | game/render/lit_model_emitter.cpp:140 |  | A08's lit GT3 body plus hide bit 2. |
| 0x8013C1DC | LIVE | `beh_seaside_prox_substate` | game/ai/beh_seaside_prox_substate.cpp:598 |  | 's own prologue is `addiu sp,sp,-0x20` (disas-verified). modeArm/subC'… |
| 0x8013C3F4 | LIVE | `beh_area_threshold_ptr_swap` | game/ai/beh_area_threshold_ptr_swap.cpp:46 |  |  |
| 0x8013C538 | LIVE | `beh_scatter_record_dither` | game/ai/beh_scatter_record_dither.cpp:54 |  |  |
| 0x8013C5B4 | LIVE | `LitModelEmitter::gt4` (A06) | game/render/lit_model_emitter.cpp:144 |  | A08's lit GT4 body plus hide bit 2. |
| 0x8013C9C0 | LIVE | `beh_scatter_ramp_machine` | game/ai/beh_scatter_ramp_machine.cpp:49 |  |  |
| 0x8013CDD4 | LIVE | `WidescreenMarginQuad::emit` | game/render/widescreen_margin_quad.cpp:178 |  |  |
| 0x8013CF00 | LIVE | `UnlitModelEmitter::gt3` (A06) | game/render/unlit_model_emitter.cpp:143 |  | A06 drawer unlit GT3 with U scroll (kA06Scroll). |
| 0x8013D1E4 | LIVE | `UnlitModelEmitter::gt4` (A06) | game/render/unlit_model_emitter.cpp:147 |  | A06 drawer unlit GT4 with U scroll (kA06Scroll). |
| 0x8013DD48 | ORPHAN | `sub8013DD48` | game/ai/beh_a08_scene_actor.cpp:172 | 0x80072DDC | (objAnim, subId) — allocate a spawner obj and hook its handler. |
| 0x8013FB88 | LIVE | `OverlayGroundGt3Gt4::gt3` | game/render/overlay_ground_gt3gt4.cpp:142 |  | ground/scene POLY_GT3 emit. Record = 36 bytes, SAME field layout as th… |
| 0x8013FE58 | LIVE | `OverlayGroundGt3Gt4::gt4` | game/render/overlay_ground_gt3gt4.cpp:279 |  | ground/scene POLY_GT4 emit. Record = 44 bytes: {+0 rgb0(rgb1=rgb0<<4)\|… |
| 0x801401B8 | LIVE | `OverlayGroundGt3Gt4::entityLoop` | game/render/overlay_ground_gt3gt4.cpp:408 |  | the ground-entity render list walker. list=a0: +6 (u8) entry count, +1… |
| 0x8014047C | LIVE | `ActorZonedAttacker::gateCheck` | game/ai/actor_zoned_attacker.cpp:146 |  | ActorZonedAttacker::gateCheck(c) — FUN_8014047c(node) -> bool v0. A ti… |
| 0x80140544 | LIVE | `ActorZonedAttacker::typeInit` | game/ai/actor_zoned_attacker.cpp:188 |  | ActorZonedAttacker::typeInit(c) — FUN_80140544(node). One-shot per-typ… |
| 0x801409C0 | LIVE | `ActorZonedAttacker::pickAttackByRange` | game/ai/actor_zoned_attacker.cpp:261 |  | ActorZonedAttacker::pickAttackByRange(c) — FUN_801409c0(node[, unused … |
| 0x80140FBC | LIVE | `OverlayGt3Gt4::gt3` (A08) | game/render/overlay_gt3gt4.cpp:177 |  | A00's GT3 body plus the UV scroll at 0x80145A6E. |
| 0x801411D8 | LIVE | `OverlayGt3Gt4::gt4` (A08) | game/render/overlay_gt3gt4.cpp:291 |  | A00's GT4 body plus the UV scroll. |
| 0x80143A00 | LIVE | `ActorZonedAttacker::defaultSubStateMachine` | game/ai/actor_zoned_attacker.cpp:439 |  | ActorZonedAttacker::defaultSubStateMachine(c) — FUN_80143a00(node). Th… |
| 0x80144928 | LIVE | `ActorZonedAttacker::approachAndFace` | game/ai/actor_zoned_attacker.cpp:318 |  | ActorZonedAttacker::approachAndFace(c) — FUN_80144928(node) -> v0. A s… |
| 0x80144B50 | LIVE | `ActorZonedAttacker::idleTick` | game/ai/actor_zoned_attacker.cpp:1180 |  | ActorZonedAttacker::idleTick(c) — FUN_80144b50(node). The "idle" state… |
| 0x80145230 | LIVE | `beh_id_compare_motion_dispatch` | game/ai/beh_id_compare_motion_dispatch.cpp:68 | 0x800781E0 0x8014047C 0x80140544 0x801409C0 0x80143A00 0x80144928 … |  |
| 0x801458E0 | LIVE | `AttackOrbitSubstate::orbitTargetMotion` | game/ai/attack_orbit_substate.cpp:43 |  | node[3]==0x81 sub-behavior: 6-phase acquire/orbit machine, see header … |
| 0x80145AF0 | LIVE | `AttackOrbitSubstate::aimAtTargetAnchor` | game/ai/attack_orbit_substate.cpp:145 |  | node[3]==0x80 sub-behavior: aim-point recompute + one-shot attack-wind… |
| 0x80145C78 | LIVE | `ActorZonedAttacker::zoneClassify` | game/ai/actor_zoned_attacker.cpp:1484 |  | classifies (u8 at record+0x2A, s16 at record+0x36) into a {0,1,2} zone… |
| 0x80146478 | LIVE | `OverlayGt3Gt4::submitBlock` | game/render/overlay_gt3gt4.cpp:128 | 0x801465EC 0x801467BC |  |
| 0x801465EC | LIVE | `OverlayGt3Gt4::gt3` | game/render/overlay_gt3gt4.cpp:177 |  | POLY_GT3 (gouraud-textured triangle) emit, GTE-driven, guest-writing. |
| 0x801467BC | LIVE | `OverlayGt3Gt4::gt4` | game/render/overlay_gt3gt4.cpp:291 |  | POLY_GT4 (gouraud-textured quad) emit, GTE-driven, guest-writing. |
| 0x80182000 | LIVE | `preload_build_vram` | game/core/asset.cpp:383 | 0x80075448 | cel/sprite VRAM build, synchronous. FUN_800753ac is itself an async CD… |

### PlatformHle-owned (BIOS / hardware-sync primitives — NOT porting targets)

Owned by a DIFFERENT mechanism than the table above: `PlatformHle` (`external/psxport/runtime/psx/platform_hle.cpp`), wired from the addresses this game states in `GameConfig::hle` (`game/core/entry/game_config.cpp`). No native def exists for these, so the scanner above cannot see them — grepping only that table reports them as unowned. The guest body NEVER runs; installing an override on one is a double-install.

| addr | handler | GameConfig::hle field |
|------|---------|-----------------------|
| 0x80080880 | `scheduler_yield` | `changeThread` |
| 0x80080F6C | `syncComplete` | `drawSync` |
| 0x800834A0 | `gpuTimeoutArm` | `gpuTimeoutArm` |
| 0x800834D4 | `syncComplete` | `gpuTimeoutCheck` |
| 0x8008A96C | `cdReadSync` | `cdReadSync` |
| 0x8008B2D8 | `syncComplete` | `cdInitHandshake` |
| 0x8008B4B8 | `syncComplete` | `cdDataSync` |
| 0x8009CAEC | `syncComplete` | `decDctInSync` |
| 0x8009CB80 | `syncComplete` | `decDctOutSync` |

9 PlatformHle-owned address(es).
