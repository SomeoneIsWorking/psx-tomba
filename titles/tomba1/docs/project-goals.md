# Tomba! 1 project goals

These goals refine the repository goals for Tomba! 1. Capability coverage belongs in
`project-state.md`, execution order in the root `docs/migration.md`, atomic work in `issues/`,
and placement in `codemap.md`.

## G001 — Faithful, independently verified native/dynarec PC port

**Outcome.** Tomba! 1 runs from the USA `SCUS_942.36` executable and disc content as one product:
readable title-native overrides own selected behavior and the shared `psxport` Lightrec executor
runs every remaining guest instruction.

**Why it matters.** The recorded startup and CD evidence must become a playable product without
recreating the removed authenticated executable/overlay evidence or manually rewriting all gameplay first.

**Success conditions.** After Tomba! 2 completes, the Lightrec product reproduces the recorded
35-field CRT0 boundary, crosses the current CD/movie frontier, reaches the title screen, accepts
input, and passes representative interactive gameplay. Its six engine-neutral overrides and scoped
original calls dispatch by complete image identity. Runtime invalidation has positive and negative
coverage. Product inspection proves no guest instructions at source or interpreter-first gameplay selector
exists; bounded fallback is reason-counted below its declared conformance threshold.

**Constraints and non-goals.** The implementation remains independent from Tomba! 2. Restricted
assets remain user supplied. Boot, a logo, compilation, framework smoke, or an isolated boundary is
not representative gameplay conformance. The old guest-source path is removed, not a product, fallback, or
oracle to extend. Runtime interpretation is permitted only as the shared bounded automatic fallback
for translation failure or unavailability, unsafe fetch, or rare unsupported blocks. Forced
interpreter mode is diagnostic-only; fallback-covered execution proves neither gameplay conformance
nor performance.

**Contributing state items.** S001, S002, S003, S004, S006, S007.

## G002 — True widescreen

**Outcome.** Extend Tomba! 1's projection, visibility, edge coverage, and 2D layout to wide aspect
ratios while preserving faithful 4:3 presentation.

**Why it matters.** Widescreen is the selected presentation enhancement and must expose more
correctly projected game content rather than stretch or crop a 4:3 picture.

**Success conditions.** After the Lightrec gameplay gate, the running product demonstrates additional
world coverage, correct visual culling, a correctly placed wide draw buffer, and authored
left/right/center UI anchoring at representative wide ratios with a controlled 4:3 comparison.

**Constraints and non-goals.** Interpolation, temporal frame history, a title-native renderer, native
graphics producers, native depth, and 60fps/native-rendering options are outside the title surface.
Unsupported modes are absent rather than retained disabled. Shared `psxport` remains the platform
presentation owner.

**Contributing state items.** S005, S006.
