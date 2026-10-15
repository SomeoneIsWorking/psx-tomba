# Features — Tomba! 2 and Tomba! 1

Title hops only. Each row is where a Tomba title joins a psxport feature hop (`psxport F-X.n`, psxport
`docs/features.md`); a feature with no row runs on the framework alone. T2 is Tomba! 2 (`game/`), T1 is
Tomba! 1 (`titles/tomba1/game/`, shortened to `t1/` below).

## F-BOOT

| hop | title | joins | owner |
|---|---|---|---|
| 1 | both | psxport F-BOOT.1 | `game/core/entry/catalog.cpp:createCatalog` — T1 at index 0, T2 (`SCUS_944.54`) at index 1, one picker panel each |
| 2 | T2 | psxport F-BOOT.1 | `game/core/entry/identity.cpp:mainExecutableIdentity` — boot stub and MAIN.EXE facts from `config/images.json` |
| 3 | T1 | psxport F-BOOT.1 | `t1/core/entry/identity.cpp:identity` — `SCUS_942.36` facts from `executable.json` |
| 4 | T2 | psxport F-BOOT.6 | `game/core/entry/runtime.cpp:tomba::Runtime::registerOverrides` — MAIN.EXE handoff (`loadMainExecutable`), stock bindings, natives, state producers |
| 5 | T2 | psxport F-BOOT.6 | `game/core/entry/session.cpp:tomba::Session` — per-Game `Engine`, CD, audio, dev warp, override catalog |
| 6 | T2 | psxport F-BOOT.5 | `game/core/entry/runtime.cpp:tomba::Runtime::startProgram` — CRT0, then `bootInit` (the retail init prefix and task 0) |
| 7 | T1 | psxport F-BOOT.6 | `t1/core/entry/runtime.cpp:tomba1::Runtime::registerOverrides` — stock bindings, `bindTitleLibraryOwners`, task, DMA and GPU-timeout natives |
| 8 | T1 | psxport F-BOOT.6 | `t1/core/state/context.cpp:Context` — task slots and the projection owner |
| 9 | T1 | psxport F-BOOT.5 | `t1/core/entry/runtime.cpp:tomba1::Runtime::startProgram` — CRT0, then `runNativeBootPrefix` |

## F-TURN

| hop | title | joins | owner |
|---|---|---|---|
| 1 | T2 | psxport F-TURN.2 | `game/core/frame/frame_driver.cpp:tomba::FrameDriver::stepFrame` — one pass of FUN_80050B08, one presentation fence |
| 2 | T2 | psxport F-TURN.2 | `game/core/frame/boot_cards.cpp:BootCards` — SCEA card and LOGO movie, one step per frame |
| 3 | T2 | psxport F-TURN.3 | `game/core/sched/scheduler.cpp:tomba::sched::Scheduler::step` — the guest task machine FUN_80051E60 |
| 4 | T2 | psxport F-TURN.2 | `game/core/frame/frame_cadence.cpp:FrameCadence` — the frame-rate decision in the guest's quota byte |
| 5 | T1 | psxport F-TURN.2 | `t1/core/frame/frame_driver.cpp:tomba1::FrameDriver::stepFrame` — scheduled tasks, VBlank delivery, one fence |
| 6 | T1 | psxport F-TURN.2 | `t1/core/task/guest_task_slots.cpp:GuestTaskSlots::runScheduledTasks` — bounded resume across fields |

## F-INPUT

| hop | title | joins | owner |
|---|---|---|---|
| 1 | T2 | psxport F-INPUT.1 | `game/core/entry/runtime.cpp:tomba::Runtime::deliverPad` — `StockPadBuffers` at `0x800BF4F8`/`0x800BF51A` |
| 2 | T2 | title | `game/input/pad_edge_fence.cpp:Engine::padEdgeFence` — FUN_800788AC |
| 3 | T1 | psxport F-INPUT.1 | `t1/core/entry/runtime.cpp:tomba1::Runtime::deliverPad` — `StockPadBuffers` at `0x8009EB58`/`0x8009EB7A` |

## F-CD

| hop | title | joins | owner |
|---|---|---|---|
| 1 | T2 | psxport F-CD.1 | `game/core/entry/runtime.cpp:tomba::Runtime` — stock `CD_cw` `0x8008AC34`, `CdSync` `0x8008A6EC`, `CdReady` `0x8008A96C` |
| 2 | T2 | title | `game/cd/engine_cd_loader.cpp:EngineCdLoader` — the engine's block, file, async and inline loaders served from the disc |
| 3 | T2 | title | `game/cd/engine_cd.cpp:tomba::EngineCd` — streaming CdControl FUN_8001CE90, voice play/stop, deferred area music |
| 4 | T2 | psxport F-CD.3 | `game/core/overrides/native_override_catalog.cpp:NativeOverrideCatalog::activateModeOverlay` — a loaded stage or area overlay published with its overrides |
| 5 | T1 | psxport F-CD.1 | `t1/core/boot/sync_native.cpp:bindTitleLibraryOwners` — CdSync, CdReady, CdControl/F/B over `StockLibcd` |
| 6 | T1 | psxport F-CD.1 | `t1/core/frame/stream_field_turn.cpp:serviceStreamField` — SPU/XA once, then the paced CD pump |
| 7 | T1 | psxport F-CD.3 | `t1/core/entry/runtime.cpp:tomba1::Runtime::stockCdReadLanded` — the code module at `0x800E7388` |

## F-RENDER

| hop | title | joins | owner |
|---|---|---|---|
| 1 | T2 | psxport F-RENDER.2 | `game/core/entry/runtime.cpp:tomba::Runtime::packetPool` — the guest pool at a fixed base and stride |
| 2 | T2 | psxport F-RENDER.3 | `game/core/entry/runtime.cpp:tomba::Runtime::displayHeight` — 224 lines |
| 3 | T2 | title | `game/resident_natives.cpp:Engine::drawOTag` — the guest OT to the GPU device |

## F-WIDE

| hop | title | joins | owner |
|---|---|---|---|
| 1 | T2 | psxport F-WIDE.1 | `game/core/entry/runtime.cpp:tomba::Runtime::guestWidescreenProjection` — the stock projection policy |
| 2 | T2 | psxport F-WIDE.2 | `game/render/horizontal_visibility_cull.h:Visibility` — every native X gate over the draw window |
| 3 | T2 | psxport F-WIDE.2 | `game/render/cull.cpp:Cull` — per-object cull with the margin re-include |
| 4 | T2 | psxport F-WIDE.2 | `game/render/letterbox_bars.cpp:LetterboxBars` — bars across the canvas margins |
| 5 | T1 | psxport F-WIDE.1 | `t1/core/entry/runtime.cpp:tomba1::Runtime::guestWidescreenProjection` — the stock projection policy |

## F-FPS60

| hop | title | joins | owner |
|---|---|---|---|
| 1 | T2 | psxport F-FPS60.1 | `game/core/entry/runtime.cpp:tomba::Runtime::stateProducer` — `tomba::Session::stateProducers` (`docs/producers.md`) |
| 2 | T2 | psxport F-FPS60.1 | `game/render/list_state_producer.cpp:ListStateProducer` — an emitter call replayed over host memory at t |
| 3 | T2 | psxport F-FPS60.2 | `game/core/frame/frame_cut.h:FrameCut::isCut` — stage, area, sub-scene, SOP shot or camera re-place |

T1 runs at 60 fps natively; it joins no F-FPS60 hop.

## F-FMV

| hop | title | joins | owner |
|---|---|---|---|
| 1 | T2 | psxport F-FMV.1 | `game/core/frame/boot_cards.cpp:BootCards` — the LOGO movie, skipped with `PSXPORT_NO_FMV` |
| 2 | T2 | psxport F-FMV.1 | `game/scene/demo.cpp:Demo` — `MOVIE/OP.STR` from the DEMO stage |

## F-AUDIO

| hop | title | joins | owner |
|---|---|---|---|
| 1 | T2 | psxport F-AUDIO.1 | `game/audio/audio.cpp:tomba::Audio::mixFrame` — `NativeMusic` into the SPU sink |
| 2 | T2 | title | `game/audio/sequencer.cpp:Sequencer` — libsnd's per-VBlank tick and channel leaves |

## F-SAVE

| hop | title | joins | owner |
|---|---|---|---|
| 1 | T2 | psxport F-SAVE.1 | `game/core/entry/runtime.cpp:tomba::Runtime::hostIdentity` — user data under `tomba2` |
| 2 | T2 | psxport F-SAVE.2 | `game/cd/engine_cd_state.cpp:tomba::EngineCdState` — deferred area music in the machine save state |
| 3 | T2 | title | `game/scene/card_load_machine.cpp:stepCardLoadMachine` — the memory-card LOAD/SAVE pages |

## F-DEBUG

| hop | title | joins | owner |
|---|---|---|---|
| 1 | T2 | psxport F-DEBUG.1 | `game/core/entry/runtime.cpp:tomba::Runtime::titleCommand` — `warp`/`items`/`flag`, `tp`, then `replCommand` |
| 2 | T2 | psxport F-DEBUG.1 | `game/core/debug/dev_control.cpp:DevControl::applyArmed` — applied by the frame driver at a frame boundary |
| 3 | T2 | psxport F-DEBUG.2 | `game/core/debug/dev_areas.cpp:tomba::DevAreas` — the picker's warp list |
| 4 | T2 | psxport F-DEBUG.3 | `game/core/entry/runtime.cpp:tomba::Runtime::worldReadout` — Tomba's position and the running stage |
| 5 | T2 | title | `game/core/debug/auto_drive.cpp:AutoDrive` — `PSXPORT_AUTO_SKIP` into GAME free-roam |
