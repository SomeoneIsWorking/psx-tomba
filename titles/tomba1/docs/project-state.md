# Tomba! 1 project state

Baseline: the USA PlayStation release `SCUS_942.36` on original hardware or a trusted emulator, and
this title's recorded pre-migration native/offline-translated hybrid. Title-local inventory: it
inherits no capability from Tomba! 2. Epic intent in `project-goals.md`, work in `issues/`.

Current focus: S004 — the product stops at `0x800E7D5C` with `image identity lookup: claimed by
none`; that is a missing framework seam (streamed-module identity plus `CdRead` invalidation), not
title work.

| ID | Capability | State | Evidence or gap |
|---|---|---|---|
| S001 | Selected executable and disc provenance established | verified | Root `SYSTEM.CNF` names `SCUS_942.36`; 559,104 B, SHA-1 `81cbc79f0230aeb4252e058039f47ac95a777f5a`, entry `0x8006B58C`, text `[0x80010000,0x80098000)`, SP `0x801FFFF0`; altered bytes, malformed/wrong/ambiguous discs refuse publication (`titles/tomba1/tools/verify_executable.py`, 15 facts). |
| S002 | Title identity and engine-isolation evidence exists | verified | All Tomba! 1 owners live under `titles/tomba1/` and the composition imports no root `game/` source; the mechanical isolation checker was deleted with the rest of the reporting tooling, so this is now a placement fact, not an enforced gate. |
| S003 | The 35-field CRT0 boundary independently established | verified | Two independent-oracle runs agree 35/35 on target/PC/register fields at the first `A(39h)` call after 42,140 steps; forced `gp` mutation and a 100-step run give the opposite answer. Startup facts: BSS `[0x8009AFB0,0x800A3348)`, SP `0x801FFFF8`, heap base `0x800A3348`, size `0x15C8B0`, gp `0x80097FA8`, wrapper `0x8006B70C`, game main `0x800163B0`. |
| S004 | Native/Lightrec product reaches representative Tomba! 1 gameplay | blocked | After the bounded multi-field resume the shipping product runs 400 fields, exit 0, 1,713,248 blocks / 11,422,254 guest instructions, 0 fallback — then faults at `0x800E7D5C`, `image identity lookup: claimed by none` (issue 0008). Publishing the streamed module's identity experimentally only exposes the next blocker: guest task 1 retry loop at `0x8001F2FC` calling `0x8001EFE8` with an empty ring. Presented frames are 320x224 and fully black; no gameplay claim is available. |
| S005 | True widescreen works in the running Tomba! 1 product | missing | Grounded facts kept: `SetGeomOffset` `0x80063A34`, `SetGeomScreen` `0x80063A54`, init `0x80016AF4` publishes centre `(160,112)` and `H=544`, display construction `0x80016C4C` uses 320x224 rectangles, resident `0x8002D784` later reasserts `H`. Gap: loaded-code projection contributions, visual-vs-gameplay culling, wide draw-buffer placement and authored 2D anchors — then proof against a controlled 4:3 run. Depends on S004. |
| S006 | Tomba! 1 remains engine-isolated and exposes only widescreen | verified | `titles/tomba1/enhancement_scope.json` is `{"widescreen": true}`; the title surface rejects 60 fps, temporal history, native rendering and lerp options. |
| S007 | The Tomba! 1 offline guest-source product path is removed | verified | No generator, emitted guest source, emission-only seed or generation-only selftest remains; `titles/tomba1/tools/run.py` provisions the authenticated disc and builds the native/Lightrec product only. |

## Structure pass

`titles/tomba1/game/core/` is split by concept into `entry/ boot/ frame/ task/` (`game/app/` folded
into `entry/`); `platformHlePlan()` no longer holds its table in a function-local `static`.
