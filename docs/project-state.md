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
- Picture: S004. Tomba! 2 defaults to the `record` path, the guest's GP0 output recorded per frame and
  replayed, at 4:3, 320x224; at 1x it is byte-identical to the Beetle `device` path. The display-time
  native renderer is deleted. Against the pre-migration hybrid it has no widescreen yet (S005), and 60
  fps interpolation (S006) covers only primitives a producer keys.
- Startup: Tomba! 2 starts through psxport's multi-title host with the in-window picker (S026).
- Platforms: Linux x86-64 (S019); Windows, macOS and Android are missing (S020-S022).

## Current focus

S001 — issue 0005 must prove one resident and one colliding-overlay override, each scoped back into the
original guest body, before the recorded gameplay frontier is restored.

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
| S026 | Tomba runs through psxport's multi-title host and in-window picker | partial | S010 | — |

### S001 — Tomba! 2 reaches representative gameplay as a native/Lightrec product

Blocker: Headless `newgame` runs 3000 frames through the intro and the seaside field with zero faults and zero Lightrec fallback, past the village-vista packet-pool overrun that cull limits wider than the guest's caused; representative gameplay unverified (issue 0005).

### S002 — Tomba! 2 behaviour compared independently against the original

Gap: The known framing delta is tracked in `issues/0019`; no comparison instrument is retained in this repository.

### S003 — Tomba! 2 game behaviour owned by readable native subsystems

Gap: Native owners exist across `game/`; 42 declared-at-overlay-address overrides that never installed are fixed and refused by abort (issue 0015, closed).

### S004 — Tomba! 2 picture produced completely from game-owned scene state

Gap: The picture is the guest's GP0 output on the GPU device; the display-time native renderer is deleted. Guest-time native ports write the render walk, per-object, billboard, overlay, HUD and UI packets (`codemap.md`, `game/render/`, `game/ui/`); the rest is drawn by guest bodies. Screen fades are the packets of the fade leaf `FUN_8007E9C8`, now native (`ScreenFade::draw`, byte-identical to the retail leaf at 4:3); the intro fades track Beetle frame for frame within one ramp step.

### S005 — Tomba! 2 true widescreen covers world visibility and 2D layout

Gap: The record path draws a [-M, 320 + M) canvas (`wide_window::drawWindow`). Letterbox bars, screen fades and every native X gate cover it, including the A05/A07/A08 model emitters (`issues/0027`, `issues/0028`); 4:3 stays byte-identical to retail. Missing: the A01, SOP, resident, A0B and A0C emitter bodies and the A00 sky cells (`issues/0028`), and the 3-px framing shift (`issues/0019`).

### S006 — Tomba! 2 interpolation covers moving camera, objects and effects

Gap: Every keyed producer now has a state render (`codemap.md`, State producers): the model emitters, the tile grids, the rain and the glyphs move by t from saved state. fps60 on shows composed presents (about 70% of presents in area 1 and area 8), fps60 off stays at 0 mismatched; the in-between shots show Tomba, rain and the lava moving with nothing torn, doubled or lost, but the cameras of both areas are idle so a scrolling grid or moving model list is shown only by the unit tests. Left: keyed blend still runs for Tomba (`issues/0033`), and the unkeyed UI, billboard and fade writers are presented as drawn. Producers (`codemap.md`, Producers) key the drawn object: render command, sub-part or scenery table slot, with each model primitive an element (`GT3/GT4 list, index`), so two commands drawing one shared model no longer share keys. Keyed share 29.8% of intro+field-0, 96.9% of area 1, 94.2% of area 8 primitives; with the SOP tile grid, SOP ground, glyph and A08 rain producers, `newgame`+1026 is 99.0% (637,658 of 644,038) and warp 8 holding right 98.3% (321,966 of 327,629, was 62.9%), 0 duplicate keys; 0 duplicate keys in all 1425 consecutive frames; 180, 168 and 511 blended pairs per frame (`newgame`+1000, warp 1+200, warp 8+200). Paired vertices step at most 39 px in area 8 and over 48 px only on three guest teleports (f216 intro objects 110 px, f274 one vertex 49 px, area 1 warp+3 sway scenery 264 px); cuts come from `FrameCut`. Record 1x = device: `recordcheck` mismatched=0 on 1428/1428 records, three shots byte-identical. In-between frames cannot be captured on the record path (`preseq` stores N twice).

### S007 — Tomba! 2 accepts native player input through representative gameplay

Gap: Held pad input walks and jumps Tomba in the seaside field for 402 frames with the reference agreeing; other areas, menus and combat undriven.

### S008 — Tomba! 1 selected executable and disc provenance established

Evidence: `SCUS_942.36`, 559,104 B, SHA-1 `81cbc79f0230aeb4252e058039f47ac95a777f5a`, entry `0x8006B58C`, text `[0x80010000,0x80098000)`, `SYSTEM.CNF` agreement before publication.

### S009 — Tomba! 1 identity, isolation and independent startup evidence exist

Evidence: Title-local identity + isolation checks and a 35-field CRT0 boundary (`titles/tomba1/tools/verify_executable.py`).

### S010 — Tomba! 1 reaches representative gameplay as a native/Lightrec product

Blocker: Pre-migration run reached SCEA/Whoopee Camp presentation; next boundary is the guest stream task budget (issue 0008 open).

### S011 — Tomba! 1 true widescreen works in the actual product

Missing capability: `titles/tomba1/game/render/widescreen_projection.cpp` exists; no product run yet (depends on S010).

### S012 — Tomba! 1 and Tomba! 2 game-engine implementations are isolated

Evidence: Tomba! 1 owners live only under `titles/tomba1/` and import no root `game/` source.

### S013 — Tomba! 1 exposes widescreen only, no unrelated enhancement modes

Evidence: `titles/tomba1/enhancement_scope.json` is `{"widescreen": true}`; the title surface rejects 60 fps, native rendering and lerp.

### S014 — Tomba! 2 sound effects and music work throughout the game

Gap: Native audio and CD/XA owners exist; the per-vblank sequencer tick runs again on the slots its own trampoline reads, but not yet observed through representative Lightrec gameplay.

### S015 — Tomba! 2 saves, reloads and survives a full restart

Gap: Save and menu owners exist; no product run proves save → restart → reload.

### S016 — Tomba! 2 movies play correctly

Gap: FMV/CD owners exist with pre-migration coverage; unobserved on the Lightrec product.

### S017 — Tomba! 2 area and scene transitions work throughout the game

Gap: Title-to-gameplay transition recorded; representative transitions unverified (card `kanban-108`).

### S018 — Both titles removed their offline guest-source product paths

Evidence: No emitted guest source, generator, static dispatch or generation-only selftest remains; both CMake graphs build native/Lightrec products.

### S019 — Linux x86-64 asset-free native/Lightrec CI builds and tests the repository

Evidence: Hosted `Linux x86-64 native/Lightrec contract` job runs `tools/verify_ci.py` (build + product tests + clang-format/tidy/cpp policy).

### S020 — Windows x86-64 native/Lightrec CI builds and tests the repository

Missing capability: No Windows product composition or hosted build; CMake graph and psxport presentation unproven there.

### S021 — macOS x86-64 and arm64 native/Lightrec CI builds and tests the repository

Missing capability: No hosted macOS build; executable-memory publication, icache coherence, ABI and SDL presentation unqualified on arm64.

### S022 — Android arm64-v8a native/Lightrec CI assembles and tests the repository

Missing capability: No application, Gradle/NDK composition, `android-port` consumption, touch layer or SAF setup here.

### S023 — Tomba! 2 loads complete without loading-only waits or presentation

Gap: Every reachable load is synchronous and presents no card: boot preloads, attract item launch, GAME prologue first area, the in-field area transition and cold warps all complete inside the caller's own display field, and the "Loading....." card and `Engine::submode1Faithful` are deleted. The card's only caller was `FUN_80044BD4`'s wait loop, and all 22 of its guest call sites are guest images of load paths already owned natively, so an override there is unreachable and was not landed. The area-transition 5 s minimum/cancel is open: no writer of `FUN_80127798`'s `node[5]=3` is reachable.

### S024 — Tomba! 1 loads complete without loading-only waits or presentation

Missing capability: No load operation classified for Tomba! 1.

### S025 — Tomba! 2 files in-app bug reports (B key) with a replayable reproduction

Gap: Headless REPL `bugreport` at pad frame 2400 saved the presented shot, the PSX render of the same ordering table (`Engine::captureBugReportReference`), `repro.pad` and its start card under `PSXPORT_BUG_REPORT_DIR`; replaying the report and filing again at 2400 was byte-identical. Measured before the native renderer was deleted; not re-run since. The windowed B key and form are unrun by an agent. Saved by default under `<user data>/tomba2/bug-reports/`.

### S026 — Tomba runs through psxport's multi-title host and in-window picker

Gap: Tomba! 2 is catalogued and boots, warps and returns to the picker headlessly; its panel holds the in-game attract-demo field after the 900-step pre-roll, not `OP.FMV` (the opening movie is Demo's blocking `fmv.play`). Tomba! 1 is not catalogued: its product aborts on the guest task fault at 0x800E7D5C (S010, issue 0008). The REPL-driven tools `tools/gate.py`, `tools/oracle_tomba2.py` and `tools/title_prompts.py` still assume the removed stdin REPL and are stale.
