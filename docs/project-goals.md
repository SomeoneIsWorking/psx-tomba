# Project goals

This repository hosts two isolated Tomba ports. They share `psxport`, not a game engine. Factual
coverage belongs in `project-state.md`, migration order in `migration.md`, atomic work in `issues/`,
and placement in `codemap.md`.

## G001 — Playable Tomba! 2 native/dynarec PC port

**Outcome.** Tomba! 2 runs from user-supplied game content as one PC product: readable native C++
owners implement deliberately ported behavior and `psxport` Lightrec dynamically executes every
remaining guest instruction.

**Why it matters.** Full gameplay does not need to be manually rewritten before the authenticated executable/overlay evidence
can leave the product. A runtime dynarec provides complete guest coverage while native ownership grows
at verified subsystem boundaries.

**Success conditions.** A fresh `./run.sh` launch reaches ordinary representative gameplay without a
guest-source emission step; resident and colliding-overlay overrides plus scoped original calls work
through image-aware runtime dispatch; Lightrec executes nonzero blocks and invalidates them correctly;
the gameplay product contains no guest instructions at bodies or interpreter-first gameplay selector;
bounded fallback is reason-counted and below its declared conformance threshold; input, audio, saves,
movies, menus, transitions, timing, and relevant device behavior are
verified against independent evidence on each released host.

**Constraints and non-goals.** Restricted assets remain user supplied. Boot, logos, menus, or isolated
leaf calls do not establish gameplay conformance. The retired guest-source path is not preserved as a second
product, fallback, or permanent oracle. Runtime interpretation is permitted only as `psxport`'s bounded
automatic fallback for translation failure or unavailability, unsafe fetch, or rare unsupported blocks.
Forced interpreter mode is diagnostic-only; fallback-covered execution proves neither gameplay
conformance nor performance.

**Contributing state items.** S001, S002, S003, S007, S014, S015, S016, S017, S018.

## G002 — Tomba! 2 native wide and interpolated presentation

**Outcome.** Tomba! 2 renders from game-owned scene state through its native renderer, supports true
widescreen, and presents correctly interpolated frames between simulation ticks.

**Why it matters.** Native scene ownership is the common prerequisite for a wider view, real depth,
and stable interpolation without reconstructing meaning from GTE, ordering-table, or GP0 output.

**Success conditions.** Every visible layer has an attributed game-state producer; widescreen shows
additional correctly projected content with correct culling and anchored 2D layout; interpolation
uses stable previous/current camera, object, and effect state through the same render path as real
frames; representative real-game runs and controlled independent-oracle legs establish the picture
and cadence through the Lightrec product.

**Constraints and non-goals.** Render code does not write guest memory. Guest packets and GTE output
are diagnostic evidence, never picture inputs. Missing producers remain explicit rather than gaining
a family-wide fallback or renderer special case.

**Contributing state items.** S004, S005, S006.

## G003 — Playable, engine-isolated Tomba! 1 native/dynarec port

**Outcome.** Tomba! runs from user-supplied `SCUS_942.36` content through its own title engine and the
shared `psxport` Lightrec executor.

**Why it matters.** Co-location is organizational convenience only. The title retains its grounded
startup and CD behavior while gaining complete runtime guest coverage without a second guest-source product.

**Success conditions.** The Lightrec product reproduces the recorded 35-field CRT0 boundary, crosses
the current CD/movie frontier, reaches the title screen, accepts input, and passes representative
interactive gameplay; its six engine-neutral native overrides and scoped original calls use complete
image identity; no offline-emitted guest source or interpreter-first gameplay mode exists; bounded
fallback is reason-counted below its declared threshold; no Tomba! 2 guest
address, game source, renderer assumption, or compatibility adapter crosses the title boundary.

**Constraints and non-goals.** Tomba! 1 code lives under `titles/tomba1/`. Its guest-source path is
already removed; measured binary behavior remains as evidence. Tomba! 2 must complete
its migration first. Forced interpreter mode is diagnostic-only, and fallback-covered execution is
not gameplay-conformance or performance evidence.

**Contributing state items.** S008, S009, S010, S012, S018.

## G004 — Tomba! 1 true widescreen without unrelated enhancement modes

**Outcome.** Tomba! 1 supports true widescreen through its own projection, visibility, edge-coverage,
and 2D-layout owners.

**Why it matters.** A wider view is the selected presentation enhancement for this title; unrelated
renderer and cadence systems would add false compatibility paths.

**Success conditions.** The running Lightrec product preserves faithful 4:3 presentation and displays
additional correctly projected world content at wide aspect ratios; visual culling and authored UI
anchors are handled explicitly; the title exposes no interpolation, temporal-history,
title-native-renderer, native-producer, native-depth, 60fps, or native-rendering option.

**Constraints and non-goals.** Widescreen is not host stretching, cropping, or reconstruction from
post-projection data. The shared `psxport` renderer remains the platform presentation owner.

**Contributing state items.** S011, S013.

## G005 — Loading removal

Remove storage latency and loading-only waits from every load the game performs, without changing
unrelated scripted timing or faking completion. Loading runs asynchronously and the product goes
straight to the next real presentation.

Success requires each measured load operation to deliver the same payload and terminal state as
retail while omitting its loading-only presentation. Logo screens accept Start/Cross through the
title's recovered cancellation route (or a purpose-built skip establishing the same lifecycle,
resource, and state invariants). Authored transition cutscenes are presentation, not loading, and
remain. Tomba! 2's area transition is an authored cutscene, not a loading screen: it stays, with a five-second minimum and the same Start/Cross cancellation route.

Faster simulation, bypassed lifecycle callbacks, written phase/timer/scene words, and presentation
tricks that hide a wait are not implementations of this goal. Loading removal is suppressed under
oracle comparison.

Contributing state: S023, S024.
