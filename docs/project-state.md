# Project state

Epic intent is in `project-goals.md`, migration order in `migration.md`, atomic work in `issues/`,
placement in `codemap.md`, and which reverse-engineering steps are ground-truth-ready and which are not
in `re-frontier.md`.

## Comparison baseline

The baseline is the unmodified PlayStation releases of *Tomba!* and *Tomba! 2*, and this repository's
pre-migration native/offline-translated hybrid. Every user-visible delta from that baseline is one of
the capability rows below:

- Widescreen: S005 (Tomba! 2), S011 (Tomba! 1).
- 60 fps interpolation: S006 (Tomba! 2 only; Tomba! 1 is widescreen-only, S013).
- Loading removal: S023 (Tomba! 2), S024 (Tomba! 1).
- Picture: S004. Tomba! 2 presents the guest's own GP0 output (the `record` path, recorded per frame;
  `renderpath device` is Beetle's GPU). The display-time native renderer and the producer layer are deleted;
  widescreen draws a wider canvas (S005) and 60 fps replays the guest's draw section (S006).
- Startup: one `tomba2_port` binary and `./run.sh` start both titles through psxport's multi-title host
  with an in-window picker (S026); the separate Tomba! 1 binary and its launcher are gone.
- Platforms: Linux x86-64 (S019); Windows, macOS and Android are missing (S020-S022).

## Current focus

S023 — Tomba! 2 loads complete without loading-only waits or presentation, then S005's remaining 4:3 culls
and 2D layout.

## Capability inventory

| ID | Capability or outcome | State | Factual dependency | Goals |
|---|---|---|---|---|
| S001 | Tomba! 2 reaches representative gameplay as a native/Lightrec product | blocked | — | G001 |
| S002 | Tomba! 2 behaviour compared independently against the original | partial | — | G001 |
| S003 | Tomba! 2 game behaviour owned by readable native subsystems | partial | — | G001 |
| S004 | Tomba! 2 picture produced completely from game-owned scene state | partial | — | G002 |
| S005 | Tomba! 2 true widescreen covers world visibility and 2D layout | partial | — | G002 |
| S006 | Tomba! 2 interpolation covers moving camera, objects and effects | partial | — | G002 |
| S007 | Tomba! 2 accepts native player input through representative gameplay | partial | — | G001 |
| S008 | Tomba! 1 selected executable and disc provenance established | verified | — | G003 |
| S009 | Tomba! 1 identity, isolation and independent startup evidence exist | verified | — | G003 |
| S010 | Tomba! 1 reaches representative gameplay as a native/Lightrec product | blocked | — | G003 |
| S011 | Tomba! 1 true widescreen works in the actual product | missing | S010 | G004 |
| S012 | Tomba! 1 and Tomba! 2 game-engine implementations are isolated | verified | — | G003 |
| S013 | Tomba! 1 exposes widescreen only, no unrelated enhancement modes | verified | — | G004 |
| S014 | Tomba! 2 sound effects and music work throughout the game | partial | — | G001 |
| S015 | Tomba! 2 saves, reloads and survives a full restart | partial | — | G001 |
| S016 | Tomba! 2 movies play correctly | partial | — | G001 |
| S017 | Tomba! 2 area and scene transitions work throughout the game | partial | — | G001 |
| S018 | Both titles removed their offline guest-source product paths | verified | — | G001, G003 |
| S019 | Linux x86-64 asset-free native/Lightrec CI builds and tests the repository | verified | — | — |
| S020 | Windows x86-64 native/Lightrec CI builds and tests the repository | missing | — | — |
| S021 | macOS x86-64 and arm64 native/Lightrec CI builds and tests the repository | missing | — | — |
| S022 | Android arm64-v8a native/Lightrec CI assembles and tests the repository | missing | — | — |
| S023 | Tomba! 2 loads complete without loading-only waits or presentation | partial | — | G005 |
| S024 | Tomba! 1 loads complete without loading-only waits or presentation | missing | — | G005 |
| S025 | Tomba! 2 files in-app bug reports (B key) with a replayable reproduction | partial | — | — |
| S026 | Tomba runs through psxport's multi-title host and in-window picker | partial | — | — |

### S001 — Tomba! 2 reaches representative gameplay as a native/Lightrec product

Blocker: Headless `newgame` runs 3000 frames through the intro and the seaside field with zero faults and zero Lightrec fallback, past the village-vista packet-pool overrun that cull limits wider than the guest's caused; representative gameplay unverified.

### S002 — Tomba! 2 behaviour compared independently against the original

Gap: The free-roam picture matches the console reference at zero shift (`tools/picture_oracle.py`, 2026-11-12); behaviour past free roam is not compared.

### S003 — Tomba! 2 game behaviour owned by readable native subsystems

Gap: Native owners exist across `game/`; 42 declared-at-overlay-address overrides that never installed are fixed and refused by abort (issue 0015, closed).

### S004 — Tomba! 2 picture produced completely from game-owned scene state

Gap: The picture is the guest's GP0 output; guest-time native ports write the render walk, per-object, billboard, overlay, HUD and UI packets (`codemap.md`, `game/render/`, `game/ui/`, `draw-emitters.md`), and guest bodies write the rest. Screen fades are the native fade leaf `FUN_8007E9C8` (`ScreenFade::draw`). At free roam the picture matches the Beetle console reference at zero shift, 5.8% of pixels a different colour, spread over the frame (`tools/picture_oracle.py`, 4:3, 2026-11-12); later screens are not compared.

### S005 — Tomba! 2 true widescreen covers world visibility and 2D layout

Gap: The record path draws a [-M, 320 + M) canvas (`wide_window::drawWindow`); letterbox bars, screen fades, the sky tile grids and every native X gate cover it (`Visibility`), and 4:3 keeps the guest's [0, 320). Native model emitters (`draw-emitters.md`) cull over the draw window. Still culling at the guest's `SX < 320`: the GT3/GT4 emitter bodies that differ from every native, A04 0x801175FC/0x801178A0 and 0x8013D0F8/0x8013D568, A05 0x8013484C/0x80134B74 and 0x8013AC90/0x8013AF0C, A07 0x8012DBB4/0x8012DE60, A0G 0x8010C3A4/0x8010C664, A0K 0x80114F1C/0x8011562C and 0x801163E0/0x80116708, plus SOP's effect 0x8010BF54 and A01's drawer 0x80132DC0. 2D layout (HUD, dialog, menu pages) is not surveyed at 16:9 or 21:9.

### S006 — Tomba! 2 interpolation covers moving camera, objects and effects

Gap: Every screen that draws replays its guest draw section at blended inputs (`DrawReplay`, `DrawSection`; `plans/replay-interpolation.md`): the field and room draws with the pause menu, menu pages, A03's cart ride, a door's fade frames, SOP's narration and draw tail, the title menu and the attract demo. The update block's letterbox, dialog panel and `UiSprite` draws are rebuilt at t (`UpdateBlockDraws`), and palette cycles cross-fade (`PaletteCycles`). Not replayed: memory-card pages (their BIOS calls refuse an in-between, so they show whole), the stepped boot cards, frames that draw nothing (loads) and declared cuts. Paced `warp 0/0` running right: 698 of 701 simulated 60.00 Hz vblanks advance half a tick, 3 repeat (2026-11-11). Open: Tomba's part angles across an animation keyframe switch and the camera shake's cut limit (plan, Open).

### S007 — Tomba! 2 accepts native player input through representative gameplay

Gap: Held pad input walks and jumps Tomba in the seaside field for 402 frames with the reference agreeing; other areas, menus and combat undriven.

### S008 — Tomba! 1 selected executable and disc provenance established

Evidence: `SCUS_942.36`, 559,104 B, SHA-1 `81cbc79f0230aeb4252e058039f47ac95a777f5a`, entry `0x8006B58C`, text `[0x80010000,0x80098000)`, `SYSTEM.CNF` agreement before publication.

### S009 — Tomba! 1 identity, isolation and independent startup evidence exist

Evidence: a 35-field CRT0 boundary.

### S010 — Tomba! 1 reaches representative gameplay as a native/Lightrec product

Gap: Start at the title reaches the first field and the dev `warp` enters other areas through the game's own transition (`titles/tomba1/docs/project-state.md` S004). The real-window play check (2026-10-16) misses two vsyncs in a row at the New Game hand-off (psxport issue 0159) and none in 75 s after the warp. Representative gameplay beyond area 0 and 1 is unshown.

### S011 — Tomba! 1 true widescreen works in the actual product

Missing capability: `titles/tomba1/game/render/widescreen_projection.cpp` exists; no product run yet (depends on S010).

### S012 — Tomba! 1 and Tomba! 2 game-engine implementations are isolated

Evidence: Tomba! 1 owners live only under `titles/tomba1/` and import no root `game/` source; both link into `tomba2_port`, and `tomba_title_link_isolation` finds no external name both trees define from their own sources (psxport's `title_link_isolation` fixture pair must report one).

### S013 — Tomba! 1 exposes widescreen only, no unrelated enhancement modes

Evidence: `titles/tomba1/enhancement_scope.json` is `{"widescreen": true}`; the title surface rejects 60 fps, native rendering and lerp.

### S014 — Tomba! 2 sound effects and music work throughout the game

Gap: Native audio and CD/XA owners exist; the per-vblank sequencer tick runs again on the slots its own trampoline reads, but not yet observed through representative Lightrec gameplay.

### S015 — Tomba! 2 saves, reloads and survives a full restart

Gap: Save and menu owners exist; no product run proves save → restart → reload.

### S016 — Tomba! 2 movies play correctly

Gap: FMV/CD owners exist with pre-migration coverage; unobserved on the Lightrec product.

### S017 — Tomba! 2 area and scene transitions work throughout the game

Gap: The dev `warp` and `door` verbs arrive through the game's own field transition (`DevWarp`, `FieldTransition::main`; `door` through its fade-out and fade-in), and a scene change is a declared cut (`FrameCut`). Title to gameplay runs from boot and through `PSXPORT_AUTO_SKIP`. Transitions are not walked across the whole game.

### S018 — Both titles removed their offline guest-source product paths

Evidence: No emitted guest source, generator, static dispatch or generation-only selftest remains; both CMake graphs build native/Lightrec products.

### S019 — Linux x86-64 asset-free native/Lightrec CI builds and tests the repository

Evidence: Hosted `Linux x86-64 build` job builds `tomba2_port` through psxport's `tools/port/consumer_build.py`.

### S020 — Windows x86-64 native/Lightrec CI builds and tests the repository

Missing capability: No Windows product composition or hosted build; CMake graph and psxport presentation unproven there.

### S021 — macOS x86-64 and arm64 native/Lightrec CI builds and tests the repository

Missing capability: No hosted macOS build; executable-memory publication, icache coherence, ABI and SDL presentation unqualified on arm64.

### S022 — Android arm64-v8a native/Lightrec CI assembles and tests the repository

Missing capability: No application, Gradle/NDK composition, `android-port` consumption, touch layer or SAF setup here.

### S023 — Tomba! 2 loads complete without loading-only waits or presentation

Gap: CD reads complete at once (psxport's instant disc), so boot preloads, the attract item launch, the GAME prologue's first area, the in-field area transition and warps finish inside the caller's display field. The "Loading....." card's only caller was `FUN_80044BD4`'s wait loop; `Scheduler::spawnAndWait` overrides it and runs the task to its end in the caller's field (reached by A00's room init FUN_8010AC20 on area 0 entry 7), so that wait presents no field and no card. Not yet classified load by load: which loads still hold frames the player does not control (a door's fade-out and fade-in, the area transition's minimum time, save and load pages), and how many frames each takes from request to the first gameplay frame. The area transition's 5 s minimum/cancel is open: no writer of `FUN_80127798`'s `node[5]=3` is reachable.

### S024 — Tomba! 1 loads complete without loading-only waits or presentation

Missing capability: No load operation classified for Tomba! 1.

### S025 — Tomba! 2 files in-app bug reports (B key) with a replayable reproduction

Gap: Headless REPL `bugreport` at pad frame 2400 saved the presented shot, the PSX render of the same ordering table (`Engine::captureBugReportReference`), `repro.pad` and its start card under `PSXPORT_BUG_REPORT_DIR`; replaying the report and filing again at 2400 was byte-identical. Measured before the native renderer was deleted; not re-run since. The windowed B key and form are unrun by an agent. Saved by default under `<user data>/tomba2/bug-reports/`.

### S026 — Tomba runs through psxport's multi-title host and in-window picker

Gap: Both titles are catalogued; headless, both panels reach `demo` and hold it to the end of the run. Tomba! 2's panel plays its opening `MOVIE/OP.STR` and then the in-game attract-demo field (`Demo::stepOpeningMovie` steps the opening, since a blocking `fmv.play` froze the selector for its whole 67 s run — `issues/0041`). Tomba! 2's panel supplies no logo: its title screen draws the TOMBA!2 wordmark inside the one opaque 8bpp title picture on pages (640,256)/(768,256) together with its background art (`scratch/picker4/t2b/title_vram_640_256.png`, the frame's two rects and three polys in `t2b/cmd.log`), so no texture selection separates it; a cut of that picture would carry its background at a hand-placed rectangle. Tomba! 1's panel shows its intro movie; it has no title-owned boot-prefix signal yet, so its publisher card and logo count as its picture until the panel settles. The windowed picker is unrun by an agent. The boot stub `SCUS_944.54` runs under `tools/product_environment.py`'s launch environment; its stdin REPL runs `run N`, but `newgame` never returns (watchdog-free hang, 240 s). `tools/oracle_title.py` and `tools/title_prompts.py` are unverified against the framework REPL.
