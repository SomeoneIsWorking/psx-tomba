# Tomba! 1 project state

Baseline: the USA PlayStation release `SCUS_942.36` on original hardware or a trusted emulator, and
this title's recorded pre-migration native/offline-translated hybrid. Title-local inventory: it
inherits no capability from Tomba! 2. Epic intent in `project-goals.md`, work in `issues/`.

Current focus: S004 — gameplay is reached through the title menu (Start) and `warp`; the hand-off
stall (psxport issue 0159) is the open real-window failure.

| ID | Capability | State | Evidence or gap |
|---|---|---|---|
| S001 | Selected executable and disc provenance established | verified | Root `SYSTEM.CNF` names `SCUS_942.36`; 559,104 B, SHA-1 `81cbc79f0230aeb4252e058039f47ac95a777f5a`, entry `0x8006B58C`, text `[0x80010000,0x80098000)`, SP `0x801FFFF0`; altered bytes, malformed/wrong/ambiguous discs refuse publication (`titles/tomba1/tools/verify_executable.py`, 15 facts). |
| S002 | Title identity and engine-isolation evidence exists | verified | All Tomba! 1 owners live under `titles/tomba1/` and import no root `game/` source. Both titles link into `tomba2_port`; `tomba_title_link_isolation` refuses an external name both trees define from their own sources, and psxport's fixture pair (`title_link_isolation_detects_collision`) proves it reports one. |
| S003 | The 35-field CRT0 boundary independently established | verified | Two independent-oracle runs agree 35/35 on target/PC/register fields at the first `A(39h)` call after 42,140 steps; forced `gp` mutation and a 100-step run give the opposite answer. Startup facts: BSS `[0x8009AFB0,0x800A3348)`, SP `0x801FFFF8`, heap base `0x800A3348`, size `0x15C8B0`, gp `0x80097FA8`, wrapper `0x8006B70C`, game main `0x800163B0`. |
| S004 | Native/Lightrec product reaches representative Tomba! 1 gameplay | partial | Issue 0008 is resolved. Through the picker or `tomba2_port scratch/bin/tomba1/SCUS_942.36` the product shows the publisher card, the Whoopee Camp logo and both opening movies, then replays the intro movie (LBA of `16:34:22`) in a loop; 20,000 unpaced frames exit 0, and a 70 s windowed run holds p50 16.75 ms, p99 19.25 ms, 1 of 4,199 presents over 1.5 vsync. With Start at the title the product reaches the first field (area 0), and the dev `warp 1` enters area 1 through the game's own transition. Real-window play check (2026-10-16, Start then `warp 1`, 75 s): 6,677 presents, 2 over 1.5 vsync, both at the New Game hand-off in a row (psxport issue 0159), none in the 75 s after the warp; 0 audio reprimes or drops. Gaps: the hand-off stall; movie frames present clipped at x≈718 of 960 and some show only the right strip of the picture; no representative-gameplay claim beyond the field and area 1. |
| S005 | True widescreen works in the running Tomba! 1 product | missing | Record path (`tomba1::Runtime::renderCapabilities`): at 16:9 the canvas adds 54-column margins around the retail 320x224 picture; the guest keeps OFX 160 (stock `StockLibgte` SetGeomOffset/SetGeomScreen bodies) and its retail culls, so the margins show only what the guest did not cull. Grounded facts kept: `SetGeomOffset` `0x80063A34`, `SetGeomScreen` `0x80063A54`, init `0x80016AF4` publishes centre `(160,112)` and `H=544`, display construction `0x80016C4C` uses 320x224 rectangles, resident `0x8002D784` later reasserts `H`. Gap: loaded-code projection contributions, visual-vs-gameplay culling, wide draw-buffer placement and authored 2D anchors — then proof against a controlled 4:3 run. Depends on S004. |
| S006 | Tomba! 1 remains engine-isolated and exposes only widescreen | verified | `titles/tomba1/enhancement_scope.json` is `{"widescreen": true}`; the title surface rejects 60 fps, temporal history, native rendering and lerp options. |
| S007 | The Tomba! 1 offline guest-source product path is removed | verified | No generator, emitted guest source, emission-only seed or generation-only selftest remains; the product is the shared `tomba2_port` (`./run.sh`), and the host extracts and authenticates the executable from the disc. |

## Structure pass

`titles/tomba1/game/core/` is split by concept into `entry/ state/ boot/ frame/ task/` (`game/app/` folded
into `entry/`); the libcd/libetc/libgte entries are psxport stock library classes bound by `tomba1::Runtime::registerOverrides`, and `bindTitleLibraryOwners` binds the title's own CdSync/CdReady/CdControl and widening SetGeomOffset.
