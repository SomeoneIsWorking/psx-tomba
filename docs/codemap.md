# Codemap — Tomba! 2 and Tomba! 1

Where the code lives and what each owner owns. Feature chains are in `docs/features.md`, guest
function owners in `docs/guest-addresses.md`, producers in `docs/producers.md`, Tomba! 1 in
`titles/tomba1/docs/codemap.md`; the framework side is `external/psxport/docs/codemap.md`.

## Owners

| owner | path | owns |
|---|---|---|
| `TombaCatalog`, `createCatalog` | `game/core/entry/tomba_catalog.h` | The product catalog: index 0 Tomba! 1, index 1 Tomba! 2, one picker panel each; the only place both title trees are named |
| `bootStubIdentity`, `mainExecutableIdentity` | `game/core/entry/tomba_identity.h` | SCUS_944.54 and MAIN.EXE size, SHA-256, PS-X EXE header and disc paths, from `config/tomba2-images.json` |
| `loadMainExecutable` | `game/core/entry/main_handoff.h` | The stub-to-MAIN.EXE handoff: read from the disc, authenticate, load, set the entry registers |
| `TombaRuntime` | `game/core/entry/tomba_runtime.h` | The `GameRuntime`: CRT0 and `bootInit`, stock library bindings and overrides, session, frame driver, scheduler, pad delivery, packet pool, producers, frame cuts, widescreen, commands, audio, dev warp, world readout |
| `TombaSession` | `game/core/entry/tomba_session.h` | Per-Game title state: `Engine`, render, CD and its save-state port, scheduler, audio, dev warp, override catalog, state producers, call sites; the `eng()`/`rend()`/`fade()` accessors |
| `TombaSettings` | `game/core/entry/tomba_settings.h` | `PSXPORT_NO_FMV`, `PSXPORT_AUTO_SKIP`, read once from the CVar registry |
| `Engine`, `StepReturn` | `game/core/engine/` | The stage driver: field run, frame ticks, task machine, object leaves, state dispatch, scene frame |
| `TombaFrameDriver` | `game/core/frame/frame_driver.h` | One pass of the retail main loop FUN_80050B08 per frame and exactly one presentation fence (`docs/re/frame-loop.md`) |
| `BootCards` | `game/core/frame/boot_cards.h` | The SCEA card and LOGO movie before the first game frame, one step per `stepFrame` |
| `FrameCadence` | `game/core/frame/frame_cadence.h` | The frame-rate decision, published into the guest's quota byte |
| `FrameCut` | `game/core/frame/frame_cut.h` | Whether a sealed record is a cut (stage, area, sub-scene, SOP shot, camera re-place) |
| `FrameDiagnostics` | `game/core/frame/frame_diagnostics.h` | Per-frame title probes reported after the frame |
| `TombaScheduler` | `game/core/sched/` | The guest task machine FUN_80051E60: three-slot table, yield/spawn/close, stage stanzas, synchronous task wait |
| `NativeOverrideCatalog`, `registerOverrides` | `game/core/overrides/` | Every native-override declaration, keyed by image identity and address; the guest call conventions (`guest_jal.h`, `guest_resume.h`) |
| `games_tomba2_init`, `Engine::frameUpdate`, `Engine::drawOTag` | `game/game_tomba2.h` | Natives outside any subsystem's registration; the per-VBlank update and the OT draw |
| guest structure types | `game/tomba2_types.h` | Native mirrors of the engine's in-RAM structures |
| `DevControl`, `DevWarp`, `DevItems`, `DevFlags`, `DevGate` | `game/core/debug/dev_*` | `warp`, `items`, `flag`: parsed between frames, applied by the frame driver at a frame boundary |
| `TombaDevWarp` | `game/core/debug/dev_areas.h` | The picker's warp list (`TitleDevWarp`) |
| `replCommand` | `game/core/debug/repl_commands.h` | The remaining title control-channel commands |
| `AutoDrive` | `game/core/debug/auto_drive.h` | `PSXPORT_AUTO_SKIP`: the front end driven into GAME free-roam at frame boundaries |
| `VerificationCounters` | `game/core/debug/verification_counters.h` | Title verification tallies on the diagnostics channel |
| `LibapiIntr` | `game/core/hle/` | libapi interrupt-mask primitives and the VBlank count mirror |
| `Asset`, `Str` | `game/core/assets/` | LZ decompress, texture groups, VRAM upload through the GPU device, stage preloads; resident string leaves |
| `Engine::padEdgeFence` | `game/input/pad_edge_fence.cpp` | FUN_800788AC, the per-frame pad edge fence |
| `Behaviors`, `tomba::ai::*` | `game/ai/` | Per-object and per-area behaviours, one `beh_*` file per handler; `zoned_attacker_abi.h` |
| `Actor`, `BehaviorDispatch`, `ObjectList`, `Animation` | `game/object/` | The guest object lens, behaviour dispatch, entity-list walks, animation VM, rewards, cube text |
| `ActorTomba`, `ActorInteraction`, `Collision`, `ActorTargeting` | `game/player/` | Tomba's actor (G block), item interaction, collision grid, targeting |
| `Spawn`, `Pool`, `AreaSlots`, `ObjectTable`, `Placement`, `CollisionResolve`, `GraphicsBind` | `game/world/` | Field object population, placement, collision resolve and render binding |
| `RenderRecordPool` | `game/world/render_record_pool.h` | The render-record free stack FUN_8007AAE8 and each record's life, the object key of record-keyed producers |
| `Demo`, `Sop`, `StartBinStage` | `game/scene/demo.h`, `game/scene/sop.h`, `game/scene/start_bin_stage.h` | The DEMO front end (OP.STR), the SOP intro field, task-0's disc tables and boot preloads |
| `ScriptInterp`, `ScriptObject` | `game/scene/script_*` | The cutscene script interpreter and its vocabulary |
| `FieldTransition`, `SceneTransition`, `AreaFadeSequencer`, `BgSceneTransitionSm`, `TransitionState3` | `game/scene/` | The scene and area transition machines |
| `loadOverlay`, `stepCardLoadMachine` | `game/scene/level_load.h`, `game/scene/card_load_machine.h` | START/DEMO/GAME into the stage slot; memory-card LOAD/SAVE pages and CRD residency |
| `SceneEvents`, `ParallaxBg`, `ModeStateArm` | `game/scene/` | Scene-event arms, the SOP parallax background, mode-state arms |
| `Sequencer` | `game/audio/sequencer*.cpp` | libsnd's per-VBlank sequencer and its channel leaves |
| `Sfx`, `AudioDispatch`, `MusicCoord`, `MusicList` | `game/audio/` | Sound effects, field audio dispatch, music coordination, Sound Test and area BGM |
| `NativeMusic` | `game/audio/native_music.h`, `game/audio/native_audio.h` | The real-time SEP/VAB music player mixed into the SPU sink |
| `TombaAudio` | `game/audio/tomba_audio.h` | The `TitleAudio`: mixes `NativeMusic`, names the song, plays a Sound Test entry |
| `CutsceneCamera`, `LookAngleBuilder` | `game/camera/` | The cutscene/free camera, its look builder, mode table and guest math |
| `LibcdDirCache`, `LibcdNative` | `game/cd/libcd_dir_cache.h`, `game/cd/libcd_native.h` | libcd's guest directory cache from host ISO9660 sectors; the file-table build |
| `EngineCdLoader` | `game/cd/engine_cd_loader.h` | The engine's disc loaders FUN_8008C1EC, FUN_8001DB8C, FUN_8001D940, FUN_8001DC40 |
| `TombaCd` | `game/cd/tomba_cd.h` | The CD chokepoint overrides: streaming CdControl FUN_8001CE90, voice play/stop, deferred area music, slot-2 voice clips, CdInit callbacks |
| `TombaCdState` | `game/cd/tomba_cd_state.h` | The `nativeState` port: deferred area music in the machine save state (section `tomba2.cd`) |
| `Inventory`, `ItemGrant` | `game/items/` | Item collection and the give-and-flag the debug `items` command uses |
| `Math`, `Mtx`, `Trig`, `Rng`, `Bit` | `game/math/` | The GTE transform cluster, libgte matrix and trig leaves, the LFSR, flag bits |
| `Font`, `Panel`, `UiSprite`, `OptionsPage`, `SaveMenu`, `DialogTextStream` | `game/ui/` | Guest-time UI ports: glyphs, panels, 2D sprite writers, options, save menu, dialog stream |
| `Render` | `game/render/render.h`, `game/render/render_frame.cpp` | The per-Core render umbrella and the guest render orchestrator |
| `OrderingTable`, `PacketPool` | `game/render/guest_ordering_table.h` | The guest OT and packet pool: link, allocate, depth-to-bucket, `extremeDepth` |
| `ModelPacket`, `UnlitModelEmitter`, `LitModelEmitter`, `SwayModelEmitter` | `game/render/*model*` | The GT3/GT4 model list emitters and their shared steps |
| `OverlayGt3Gt4`, `OverlayGroundGt3Gt4` | `game/render/overlay_*gt3gt4.*` | The overlay scenery GT3/GT4 emitters |
| `PackedMesh`, `DropShadow` | `game/render/packed_mesh.h`, `game/render/drop_shadow.h` | The packed-mesh writer FUN_80027768 and the drop shadow FUN_8002AB5C |
| `ListJob`, `ListStateProducer` | `game/render/list_job.h`, `game/render/list_state_producer.h` | An emitter call saved as state and replayed over host memory between frames |
| `AnchoredPackets`, `BillboardState`, `CallSiteObjects` | `game/render/anchored_packets.h`, `game/render/billboard_state.h`, `game/render/call_site_objects.h` | Saved state of anchored 2D writers and billboards; the call-site object key |
| `ScreenFade`, `LetterboxBars`, `HudGaugeEmitter` | `game/render/screen_fade.h`, `game/render/letterbox_bars.h`, `game/render/hud_gauge_emitter.h` | The fade leaf FUN_8007E9C8, the cutscene letterbox FUN_80026864, the HUD gauge |
| `NodeXform`, `ObjModelView`, `QuadRtptSubmit`, `LibgpuDrawEnv` | `game/render/` | Node transforms, model-view setup, the shared quad submit, DRAWENV compile |
| `Cull`, `Visibility` | `game/render/cull.h`, `game/render/horizontal_visibility_cull.h` | Per-object cull and LOD; the one horizontal visibility test over the draw window |
| widescreen | `game/render/wide_window.h`, `game/render/margin_render.h`, `game/render/widescreen_margin_quad.h`, `game/render/wide_re_*.cpp` | The draw window and margin width, margin re-include, margin quads, retained-width libgpu leaves |
| packet emitters | `game/render/fx_sprite_*`, `game/render/tile_grid_layer.h`, `game/render/sop_ground.h`, `game/render/rain_streaks.h`, `game/render/*walk*`, `game/render/perobj_*` | One port per remaining guest draw family |
| Tomba! 1 | `titles/tomba1/` | The second title, isolated from `game/`; `titles/tomba1/docs/codemap.md` |
| build | `CMakeLists.txt`, `cmake/tomba2_port.cmake`, `titles/tomba1/cmake/` | `tomba2_port` (both titles), `tomba2_game`, `tomba1_runtime`, the tests and policy gate |
| tests | `tests/` | Unit tests through shipping seams; `test_authentic_*` compare natives against MAIN.EXE and overlays |
| launcher, provisioning | `tools/run.py`, `tools/tomba2_provision.py`, `tools/psxport_fetch.py` | The zero-argument launch, evidence images, the `external/psxport` link |
| agent tools | `tools/gate.py`, `tools/verify_ci.py`, `tools/oracle_compare.py`, `tools/oracle_tomba2.py`, `tools/picture_oracle.py`, `tools/title_prompts.py` | Headless gate, CI verify, oracle comparison, picture checks, title prompts |

## Where new work goes

- A Tomba! 2 guest function replaced natively → the `game/` subsystem that owns it, declared through
  `NativeOverrideCatalog`, with its row in `docs/guest-addresses.md`.
- A producer or state render → `game/render/` or `game/ui/`, registered in `TombaRuntime::registerOverrides`
  and listed in `docs/producers.md`.
- Per-frame order, input, timing or the presentation fence → `game/core/frame/frame_driver.cpp`.
- Guest task scheduling → `game/core/sched/`.
- A developer command → `game/core/debug/`, applied at a frame boundary.
- Per-Game title state → `TombaSession`.
- Anything Tomba! 1 → `titles/tomba1/`, never a Tomba! 2 owner.
- Anything every title shares → psxport, not here.
