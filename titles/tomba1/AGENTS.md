# Tomba! 1 title authority

This subtree owns Tomba! (`SCUS_942.36`, USA). It shares the generic `psxport` framework with
Tomba! 2 but has a separate title engine, runtime policy, addresses, evidence, and enhancement scope.

## Execution contract

The gameplay product is a native/Lightrec hybrid. Title-owned C++ overrides implement deliberately
native behavior; every remaining guest instruction executes through the shared per-`Core` Lightrec
executor from the authenticated resident executable or current overlay. Dispatch keys include image
generation and address. Scoped original calls execute the corresponding guest body through Lightrec.

Lightrec remains the default. The shared runtime may enter a bounded interpreter fallback only for a
translation failure or unavailability, unsafe fetch, or rare unsupported block. Each entry is
reason-coded and counted against an explicit threshold. Forced interpreter mode is diagnostic-only,
and fallback-covered execution proves neither gameplay conformance nor performance. The generator,
emitted guest source, emission-only seeds, static dispatcher, and static product are already removed;
do not recreate them.

Tomba! 2 is first. Do not resume Tomba! 1 implementation until Tomba! 2 has passed representative
gameplay through Lightrec.

## Title boundary

- Tomba! 1 source lives below `titles/tomba1/game/` and must not compile or include root `game/`.
- `game/core/entry/main.cpp` composes owners only. Runtime behavior belongs in `game/core/`; widescreen
  projection and layout belong in `game/render/`.
- Disc images and extracted executables remain untracked. `tools/provision.py` resolves explicit
  argument, `PSXPORT_TOMBA1_DISC`, `.env`, then exactly one repository-root CHD, and publishes only
  after `SYSTEM.CNF` plus all tracked executable-identity facts agree.
- Preserve the recorded 35-field CRT0 contract and current CD/movie/title facts in
  `docs/re-frontier.md`; do not recreate them through address-based inspection.
- Never reuse a Tomba! 2 guest address, overlay assumption, hook table, renderer policy, or
  compatibility adapter.

## Enhancement scope

Tomba! 1's enhancement target is widescreen only. Do not add interpolation, temporal frame history,
a title-native renderer, native graphics producers, native depth, or 60fps/native-rendering options.
`enhancement_scope.json` is the machine-readable positive scope. Widescreen starts only after the
native/Lightrec product passes representative gameplay and executable-grounded RE identifies
projection, visibility culling, wide draw-buffer, and 2D layout owners. Host stretching, cropping,
OT/GP0 reconstruction, and GTE-output-derived pictures are not widescreen.

## Working loop

From the repository root, consult the project authorities before work. Read `docs/migration.md`, root `docs/re-frontier.md`, this title's `docs/re-frontier.md`, and
`docs/codemap.md`. The first Tomba! 1 implementation discriminator is the recorded 35/35 CRT0
boundary through Lightrec with forced-mismatch and too-short negatives. Next cross issue 0006's
internal-`CdSync` boundary, reach the title screen, prove input, and pass representative gameplay.
Only then resume widescreen work.
