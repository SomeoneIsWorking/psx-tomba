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
| S011 | Tomba! 1 true widescreen works in the actual product | partial | — | G004 |
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
| S027 | Tomba! 2 pit void-out mod: death-floor falls void out like water (setting, default off) | verified | — | — |

### S001 — Tomba! 2 reaches representative gameplay as a native/Lightrec product

Blocker: Headless `newgame` runs 3000 frames through the intro and the seaside field with zero faults and zero Lightrec fallback, past the village-vista packet-pool overrun that cull limits wider than the guest's caused; representative gameplay unverified.

### S002 — Tomba! 2 behaviour compared independently against the original

Gap: The free-roam picture matches the console reference at zero shift (`tools/picture_oracle.py`, 2026-11-12); behaviour past free roam is not compared.

### S003 — Tomba! 2 game behaviour owned by readable native subsystems

Gap: Native owners exist across `game/`; 42 declared-at-overlay-address overrides that never installed are fixed and refused by abort (issue 0015, closed).

### S004 — Tomba! 2 picture produced completely from game-owned scene state

Gap: The picture is the guest's GP0 output; guest-time native ports write the render walk, per-object, billboard, overlay, HUD and UI packets (`codemap.md`, `game/render/`, `game/ui/`, `draw-emitters.md`), and guest bodies write the rest. Screen fades are the native fade leaf `FUN_8007E9C8` (`ScreenFade::draw`). At free roam the picture matches the Beetle console reference at zero shift, 5.8% of pixels a different colour, spread over the frame (`tools/picture_oracle.py`, 4:3, 2026-11-12); later screens are not compared.

### S005 — Tomba! 2 true widescreen covers world visibility and 2D layout

Gap: The record path draws a [-M, 320 + M) canvas (`wide_window::drawWindow`); letterbox bars, screen fades, the sky tile grids and every native X gate cover it (`Visibility`), and 4:3 keeps the guest's [0, 320). Native model emitters (`draw-emitters.md`) cull over the draw window. The per-object cone cull (`Cull::wideConeThreshold`, FUN_8007712C and FUN_8002B278) widens with the canvas, so margin objects are kept, drawn and simulated; 4:3 is byte-identical (2026-11-12, `scratch/cull1/ba_sheet.png`: the village's Koma pig and the waterfall path's enemy appear in the 16:9 and 21:9 right margins). A0K's two pairs are native (`kA0kCue`, `kA0kDistance`): at `warp 20/4` (the Mouse Village minigame) the 16:9 floor reaches both margins where its end records were dropped, the near-black share of the margins over the floor rows falls from 0.49 to 0.28 (the rest is the floor's own distance cue), 4:3 is byte-identical in two frames, and the override differential matches memory and GTE state on every sampled call (9086 and 55248 calls; only the dead v1 differs) (2026-11-14, `scratch/s005e/a0k_margin_sheet.png`). Still culling at the guest's `SX < 320`: A04's pair 0x801175FC/0x801178A0 (drawn only while area event bit 4 of 0x800BFE56 is set; neither body ran over area 4 entries 0-3, 2026-11-14 `scratch/s005e/obs/run.log`) and A05 0x8013484C/0x80134B74 (drawn by the scenery walker while 0x800E7EAA >= 12 or bit 0x20 of its record's +0x5E6 is set); no warp reaches either story state. SOP's effect 0x8010BF54 runs through the narration and draws through `PackedMesh`, which has no screen cull, and A01's drawer 0x80132DC0 has no screen test of its own. A04 0x8013D0F8/0x8013D568, A07 0x8012DBB4/0x8012DE60 and A0G 0x8010C3A4/0x8010C664 are never called (`draw-emitters.md`). 2D layout at 16:9 and 21:9 (2026-11-12, `scratch/s005/`): the HUD and the Start page sit at their 4:3 places over a widened field; the items page's parchment is centred over black, as at 4:3; the options page's gradient backdrop (`OptionsPage::pushBackdrop`, FUN_8007FC24) spans the draw window. The Event, Status and Help tabs and the card Load page sit at their 4:3 size and place over black, their ink within 4 sink px of the 4:3 frame's (2026-11-14, `scratch/i0070/tb_sheet.png`, `pg_sheet.png`); at 16:9 the opening's dialog box and the Start page sit at their 4:3 place, and the options page with its Messages, Sound, Screen adjust and Controls pages keeps its ink within 4 sink px of the 4:3 frame's over a backdrop spanning the draw window; Screen adjust's field fills both margins (2026-11-14, `scratch/s005e/dialog_options_sheet.png`, `ui4_sheet_1.png`). Crop widescreen (`PSXPORT_WIDESCREEN=1`) shows the packets of the `uiDraw` natives at their 4:3 size and place over the cropped field (`CropLayers`): the narration crawl, the dialog boxes and speech bubbles, the HUD life gauge and item strip, and the pause menu are whole and steady across in-betweens, and guest RAM equals a 4:3 run at play and pause (2026-11-13, `scratch/t2crop/`). The other pages under crop are not checked. Area 16 (one entry) draws both 16:9 margins with no wedge at `warp 16/0` (2026-11-14, `scratch/i0070/g16_sheet.png`). Area 5's wedges were A05's fogged pair 0x8013AC90/0x8013AF0C, now `FogModelEmitter::kA05`: at `warp 5/0` the near-black share of the 16:9 margins falls from 0.36 to 0.05 with the wall and counter drawn through them, and 4:3 is byte-identical (2026-11-14, `scratch/s023b/a05_wedge_sheet.png`).

### S006 — Tomba! 2 interpolation covers moving camera, objects and effects

Gap: Every screen that draws replays its guest draw section at blended inputs (`DrawReplay`, `DrawSection`; `plans/replay-interpolation.md`): the field and room draws with the pause menu, menu pages, A03's cart ride, a door's fade frames, SOP's narration and draw tail, the title menu and the attract demo. The update block's letterbox, dialog panel and `UiSprite` draws are rebuilt at t (`UpdateBlockDraws`), and palette cycles cross-fade (`PaletteCycles`). Not replayed: memory-card pages (their BIOS calls refuse an in-between, so they show whole), the stepped boot cards, frames that draw nothing (loads) and declared cuts. Paced `warp 0/0` running right: 698 of 701 simulated 60.00 Hz vblanks advance half a tick, 3 repeat (2026-11-11). Tomba's run bob (his animation's root offset) blends with his pose, and his part angles blend across keyframe and animation switches (issue 0068, 2026-11-14: running in-betweens within 2.1 sink px of halfway, RAM identical with in-betweens on and off over 1638 pad frames). Tomba's pan +0x14E (the idle sequence's turn to face the camera) blends the short way from 0 when the earlier tick did not pan (2026-11-14, standing in `warp 0/0`: before, each in-between of the turn sat on the newer present, 6.7 to 7.1 sink px off the midpoint of its neighbours, `scratch/s006p/pan_sheet_before.png`; after, 0.1 to 2.1 px except the turn's first tick at 6.8 px, `pan_sheet_after.png`; RAM and scratchpad identical with fps60 off and on at 870 pad frames over the turn and two stomps, `ram_identity.txt`). The camera shake's in-betweens draw the later tick's shake over the blended unshaken camera (`ShakenView`, issue 0071, 2026-11-14: over the idle stomp at `warp 0/0` every in-between within 0.8 sink px of the later present, `scratch/t2shake/shake_sheet_after.png`). The section replayed at t=1 matches the real draw byte for byte in 5375 of 5375 draws of a boot, `warp 0/0` and the idle stomp (temporary probe, not committed, `scratch/t2shake/a2`); the one miss before was the opening's fade-out step, whose trailing fade the replays linked a frame late (`DrawReplay::noteTrailingFade`, issue 0057).

### S007 — Tomba! 2 accepts native player input through representative gameplay

Gap: Held pad input walks and jumps Tomba in the seaside field for 402 frames with the reference agreeing; other areas, menus and combat undriven.

### S008 — Tomba! 1 selected executable and disc provenance established

Evidence: `SCUS_942.36`, 559,104 B, SHA-1 `81cbc79f0230aeb4252e058039f47ac95a777f5a`, entry `0x8006B58C`, text `[0x80010000,0x80098000)`, `SYSTEM.CNF` agreement before publication.

### S009 — Tomba! 1 identity, isolation and independent startup evidence exist

Evidence: a 35-field CRT0 boundary.

### S010 — Tomba! 1 reaches representative gameplay as a native/Lightrec product

Gap: Start at the title reaches the first field and the dev `warp` enters other areas through the game's own transition (`titles/tomba1/docs/project-state.md` S004). The real-window play check (2026-10-16) misses two vsyncs in a row at the New Game hand-off (psxport issue 0159) and none in 75 s after the warp. Representative gameplay beyond area 0 and 1 is unshown.

### S011 — Tomba! 1 true widescreen works in the actual product

Owned by `titles/tomba1/docs/project-state.md` S005, which holds its evidence and gaps.

### S012 — Tomba! 1 and Tomba! 2 game-engine implementations are isolated

Evidence: Tomba! 1 owners live only under `titles/tomba1/` and import no root `game/` source; both link into `tomba2_port`, and `tomba_title_link_isolation` finds no external name both trees define from their own sources (psxport's `title_link_isolation` fixture pair must report one).

### S013 — Tomba! 1 exposes widescreen only, no unrelated enhancement modes

Evidence: `titles/tomba1/enhancement_scope.json` is `{"widescreen": true}`; the title surface rejects 60 fps, native rendering and lerp.

### S014 — Tomba! 2 sound effects and music work throughout the game

Gap: Native audio and CD/XA owners exist; the per-vblank sequencer tick runs again on the slots its own trampoline reads, but not yet observed through representative Lightrec gameplay. The 3 s of silence between the opening narration and the first field after New Game is the game's own (2026-11-14, `scratch/s014/`): the narration clip (XA file 1 channel 7, LBA 66515..75523, requested whole) is silent on the disc from its sector 628 on (`narration_tail_levels.txt`, played off the disc by `xadump`), and the host output falls silent at sector 629 (field 2995) while the guest keeps streaming it. The SOP scene object FUN_800527C8 then counts out its own waits and sets 0x800BF839 (field 3108), the field mode fades out over 31 ticks, stops the clip (field 3172), loads area 0 and starts its music, channel 4 LBA 84515..97979 looping (field 3178); output is non-zero from field 3182 (`u1/run.log`, unpaced, the same fields as the paced `scratch/rt1114/t2`). Host output follows each guest XA request within 4 fields; the port's loads take no time, so the hardware's gap is longer.

### S015 — Tomba! 2 saves, reloads and survives a full restart

Gap: Save and menu owners exist; no product run proves save → restart → reload.

### S016 — Tomba! 2 movies play correctly

Gap: FMV/CD owners exist with pre-migration coverage; unobserved on the Lightrec product.

### S017 — Tomba! 2 area and scene transitions work throughout the game

Gap: The dev `warp`, `door` and `leave` verbs arrive through the game's own field transition (`DevWarp`, `FieldTransition::step`; `door` through its fade-out and fade-in, `leave` through any transition type 1-8), and a scene change is a declared cut (`FrameCut`). Title to gameplay runs from boot and through `PSXPORT_AUTO_SKIP`. Transitions are not walked across the whole game.

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

Gap: CD reads complete at once (psxport's instant disc), so every load finishes inside the caller's field. The "Loading....." card's only caller was `FUN_80044BD4`'s wait loop; `Scheduler::spawnAndWait` overrides it and runs the task to its end in the caller's field (A00's room init FUN_8010AC20 on area 0 entry 7), so it presents nothing. Measured from the request to the first field-running frame (sm 2/1/2/1), 2026-11-12, `scratch/s023/`:
- area change without a door (`warp 1/0`): 4 frames, the field machine's own steps (request, load, area select, area init), then the area fades in from black under player control;
- door (`door 0 9`, `FieldTransition::main`): 69 frames, of which the load wait (sm[0x4e] = 2) is 1; the rest is the guest's door corridor (black, the torch-lit walk fading in, then out to white, 31 + 31 frames) and its one-frame steps. Start skips the corridor through case 3's own last step (`FieldTransition::skipCorridor`): 14 frames with Start at frame 12, then the guest's arrival fade from white, Tomba placed at the same entry.
Measured 2026-11-14, `scratch/s023b/` (the RE is the decomp's `docs/notes/scene-transitions.md`):
- new game (New Game press to `fieldRun` running area 0 in load mode 4): SOP's narration ignored Start through its 31-frame fade-in and then took a 31-frame fade-out; the scripted opening's Start took case 10's 31-frame fade; the field ran at +113 at the earliest (+187 with Start at +40 and +150, `ngskip_before`). `Sop::skipNarration` takes Start in the fade-in, crawl and fade-out alike and goes to the fade-out's last step, and `fieldRun` case 9's Start seeds case 10 with its last step: Start at +5 ends the narration at +6, Start at +30 in the opening runs case 10 once, and the field runs at +37 (`ngskip_after_sheet.png`). The opening itself is a dialog cutscene the player advances;
- transition types 5, 6 and 7 (`leave 1 0 5|6|7` from area 0): three one-frame variant steps holding the last picture, the field running at +7, then the destination's arrival fade under control (`leave_sheet.png`). Nothing to remove;
- type 8, the boss-clear card (`warp 10/0`, `leave 6 7 8`): `FUN_8003EBE0` shows the arena's card for 420 frames with no pad test; the field ran at +431 (`leave8b_sheet.png`). Start ends it through its own last frame (`FieldTransition::skipClearCard` sets its count to 0x1A4): Start at +20 ends the card at +20, field at +25 (`leave8skip`). Case 0 now loads the card picture through `Asset::preloadTexgroup` with sm[0x6E] seeded (`cardPictureLoadBd4`); calling `FUN_80044F58` inline without the seed aborted on its cycle budget;
- memory-card pages (game over through `spot 1 0 24 8576 -2600 14400`, then Save? Yes or Load data): from the press, 3 frames hold the menu while CRD loads, 2 are black, then "Checking MEMORY CARD..." for 13 frames before the slot browser takes input (save: press +160, browser +179, `card5_sheet.png`; load: press +190, browser +209, `card6_sheet.png`). The check is CRD's own per-port detect (`0x8018A1E8`) over psxport's instant `_card_info`; issue 0069;
- `FUN_80127798`'s node[5] = 3 path is an in-area event (a jingle, Tomba's action 0x71 and a camera fly-to), not a load: it writes neither the pending byte nor the transition type. Out of scope.
### S024 — Tomba! 1 loads complete without loading-only waits or presentation

Missing capability: No load operation classified for Tomba! 1.

### S025 — Tomba! 2 files in-app bug reports (B key) with a replayable reproduction

Gap: Headless REPL `bugreport` at pad frame 2400 saved the presented shot, the PSX render of the same ordering table (`Engine::captureBugReportReference`), `repro.pad` and its start card under `PSXPORT_BUG_REPORT_DIR`; replaying the report and filing again at 2400 was byte-identical. Measured before the native renderer was deleted; not re-run since. The windowed B key and form are unrun by an agent. Saved by default under `<user data>/tomba2/bug-reports/`.

### S026 — Tomba runs through psxport's multi-title host and in-window picker

Gap: Both titles are catalogued; headless, both panels reach `demo` and hold it to the end of the run. Tomba! 2's panel is a preview (`Game::session.preview()`): the SCEA card ends as on Start and `LOGO.STR` and `OP.STR` end as under `PSXPORT_NO_FMV` (`Session::playsMovies`), each through its own teardown, so the panel's first picture is the in-game attract demo, which loops through its fields and the title screen (`scratch/picker5/h2_sheet.png`); before, it was black for LOGO.STR's 8.5 s (the boot cards precede `pastBootPrefix`) and then showed OP.STR's 67 s. Tomba! 2's panel supplies no logo: its title screen draws the TOMBA!2 wordmark inside the one opaque 8bpp title picture on pages (640,256)/(768,256) together with its background art (`scratch/picker4/t2b/title_vram_640_256.png`, the frame's two rects and three polys in `t2b/cmd.log`), so no texture selection separates it; a cut of that picture would carry its background at a hand-placed rectangle. Tomba! 1's panel shows its intro movie; it has no title-owned boot-prefix signal yet, so its publisher card and logo count as its picture until the panel settles. The windowed picker is unrun by an agent. The boot stub `SCUS_944.54` runs under `tools/product_environment.py`'s launch environment; its stdin REPL runs `run N`, but `newgame` never returns (watchdog-free hang, 240 s). `tools/oracle_title.py` and `tools/title_prompts.py` are unverified against the framework REPL.

### S027 — Tomba! 2 pit void-out mod: death-floor falls void out like water (setting, default off)

`PSXPORT_TOMBA2_PIT_VOID_OUT` (Settings row "Pits void out like water", `tomba::ModSettings`): `HazardFall`
(`game/player/hazard_fall.cpp`) owns the death-floor fall `0x800678E8`; on, it enters the water fall
`0x8006762C` from the same frame (the RE is in the decomp's `docs/notes/hazard-falls.md`). Measured over area 1's
death floor through `spot`: on, vitality 4 → 3, the water fall's flounder, fade and respawn at area 1 entry 1
with 3 vitality (`scratch/m0062/pit_sheet.png`); at 1 vitality it reaches 0 and takes the retail death floor's
game-over route, which shows the game over screen (issue 0066, fixed). Off, vitality 4 → 0 and the game-over
route, unchanged. Area 4 (`spot 4 1 2 5440 -6080 12672`, sector 2's death floor): off, substate 5 and vitality 0;
on, substate 4, vitality 4 → 3 and respawn at area 4 entry 1.
Areas 11, 12 and 15 (the Evil Ice, Ghost and Last Evil Pig battles), through `spot` over each death floor with
the boss intro advanced by circle: off, substate 5 and vitality 0; on, substate 4, vitality 4 → 3 and respawn in
the same area.
