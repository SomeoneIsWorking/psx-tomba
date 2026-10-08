# Native/dynarec migration

This is the repository's execution migration authority. It replaces the former offline-translated/native port
plan rather than preserving that methodology as an alternative. Epic outcomes live in
`project-goals.md`, factual coverage in `project-state.md`, ordered RE dependencies in
`re-frontier.md`, and ownership in `codemap.md`.

## Product contract

Each title is one native/dynarec hybrid:

1. Title-owned C++ overrides implement deliberately native behavior and subsystems.
2. A per-game `psxport` executor uses its pinned Lightrec integration to translate every remaining
   guest instruction on demand from the authenticated executable or active overlay.

The product contains no offline-emitted guest bodies. Lightrec is the default gameplay executor. The
shared runtime may use a bounded interpreter only after a translation failure or unavailability,
unsafe fetch, or rare unsupported block. Each fallback entry carries a reason and increments telemetry
with an explicit threshold; a forced interpreter mode is diagnostic-only. Fallback-covered execution
does not establish gameplay conformance or performance. Lightrec owns translated code and its cache;
`psxport` owns the fallback policy, per-`Core` synchronization, bounded exits, hardware/HLE callbacks,
image-generation identity, override dispatch, scoped original calls, and invalidation.

An override key is the complete guest identity, including the resident executable or current overlay
generation and the address. A normal guest call observes the active override. A native override's
scoped original call disables only that one override for the duration of the call and executes the
original guest body through Lightrec. Loading or replacing executable bytes and changing an override
must invalidate every translated path that captured the old decision.

WebAssembly is part of the migration release contract: each migrated title must also provide a
browser-capable runtime path sharing the same dynarec-first execution boundary and bounded fallback
accounting. A web host remains blocked until representative gameplay and telemetry gates include it.

## Break-first boundary

Both titles' generators, generated corpora, emission-only seeds, offline dispatchers and registries,
offline-translation build rules, and generated-symbol tests are absent from the migrated product.
The final provenance audit also removed generated drafts previously retained inside native-source files.
None is a frozen fallback or an oracle and none may be recreated. Measured binary and runtime evidence remains;
new comparisons use an independent emulator or hardware, a separately built test-only interpreter,
and the Lightrec product itself.

## Tomba! 2 first

Tomba! 2 is the active conformance target. Preserve its existing native engine, finite frame owner,
fatal guest-VSync boundary at `0x80085900`, renderer, widescreen, interpolation, and recorded
boot-to-free-roam evidence while changing only the remaining guest executor.

1. Keep the break-first boundary mechanically enforced: the build requires `psxport`,
   `dynarec_capabilities.h`, and the typed runtime address API, and no generator or generated source
   input exists.
2. Consume the shared per-`Core` Lightrec executor from `psxport` and authenticate the resident image
   and each loaded overlay before execution.
3. Prove one reached resident override in both modes: normal calls execute the native owner and a
   scoped original call executes the resident guest body through Lightrec before returning normally.
4. Prove image-aware dispatch with two overlays that reuse one numeric address. The existing binary
   evidence includes `0x801113B4` in both A03 and A0B with different entry shapes. Exercise the chosen
   collision with positive cases for both images, a wrong-image negative, and scoped original calls.
5. Reach the currently recorded boot-to-free-roam frontier with all existing native owners active,
   nonzero Lightrec block execution, correct executable-memory invalidation, and fallback telemetry
   below the declared conformance threshold. A forced interpreter mode remains diagnostic-only.
6. Run one bounded representative interactive gameplay scenario covering guest PC/register state,
   memory, interrupts/timing, relevant CD/GPU/SPU behavior, native override reachability, and declared
   frame-time/correctness budgets against independent evidence.
Boot, logos, menus, an attract loop, or a leaf override alone do not satisfy steps 5 or 6.

## Tomba! 1 follows

Do not begin Tomba! 1 executor migration until Tomba! 2's representative-gameplay gate and offline-path
deletion are complete. Preserve Tomba! 1's separate engine, title-local addresses, widescreen-only
enhancement scope, and current unlanded work; nothing from the Tomba! 2 game layer is reusable.

1. Keep the already-completed break-first boundary mechanically enforced.
2. Reuse the proven shared `psxport` Lightrec executor without a title-local CPU or cache wrapper.
3. Reproduce the recorded CRT0 boundary: 35/35 target/PC/register fields at the first `A(39h)` call,
   including the forced-mismatch and too-short negative cases, using Lightrec as the product leg and
   an independent separately built test oracle.
4. Route the title's six engine-neutral native overrides through image-aware runtime dispatch and
   prove scoped original calls through Lightrec.
5. Continue from the verified startup facts through the current CD/movie frontier: preserve public
   `CdSync` `0x800648C8`, internal `CdSync` `0x80065470`, fatal guest-VSync evidence at return address
   `0x800654A4`, DMA callback registration `0x80067E84`/`0x80066D80`, and the recorded progress beyond
   LBA 58739. Then reach the title screen and demonstrate input.
6. Pass a bounded representative interactive gameplay scenario with the same state, device,
   reachability, negative-control, and host-performance requirements as Tomba! 2.
Widescreen work resumes after step 6 from the recorded `(160,112)`, `H=544`, and 320x224 projection
facts.

## Landing gate for each title

A title migration lands only when all of the following are true together:

- its authenticated runtime image reaches at least the recorded pre-migration gameplay frontier;
- Lightrec executes nonzero translated blocks and invalidation has positive and controlled-negative
  coverage for relevant overlay replacement;
- resident and image-colliding native overrides plus scoped original calls use the shipping dispatcher;
- product inspection proves no generated guest corpus or interpreter-first gameplay selector exists;
- fallback telemetry names every reason, stays below the declared threshold, and is excluded from
  gameplay-conformance and performance evidence;
- representative interactive gameplay passes against independent evidence on every released host;
- the zero-argument/default launcher provisions and launches without any offline translation step.
