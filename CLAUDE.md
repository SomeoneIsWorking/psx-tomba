# Tomba Engine repository authority

## Product direction

This repository contains two isolated native/dynarec ports. Title-owned C++ subsystems and overrides
implement deliberately native behavior. Every remaining guest instruction executes on demand from the
user's authenticated executable or active overlay through the shared `psxport` Lightrec dynarec.

The gameplay products contain no offline-emitted guest C/C++, object corpus, or precompiled title
substrate. They do not link or select an interpreter and contain no fallback to one. An interpreter
may exist only in a separately built test target, including diagnostics, for deterministic diagnosis
against the same canonical CPU, memory, exception, and device boundary. Product proof requires link
and selector inspection, not merely observing zero interpreter entries in one run.

`docs/migration.md` is the execution-plan authority. Both static products and their generation
machinery are already removed; do not recreate them. Retain measured behavioral and binary evidence,
but no compatibility mode or static oracle.

## Title order and isolation

Tomba! 2 (`SCUS_944.54`) is first. Root `game/` is its title engine. It must prove one resident
native override and a colliding-overlay override plus scoped original calls through Lightrec, restore
the recorded gameplay frontier, and pass representative gameplay.

Tomba! 1 (`SCUS_942.36`) follows and lives entirely below `titles/tomba1/`. It reuses only the
generic `psxport` executor and services. Never reuse a Tomba! 2 address, overlay assumption, game
owner, renderer policy, or compatibility adapter. Its first dynarec boundary is the recorded 35-field
CRT0 comparison; it then crosses the current CD/movie boundary, reaches the title screen, demonstrates
input, and passes representative gameplay.

## Shared and title ownership

- `external/psxport/` owns Lightrec integration, per-`Core` CPU synchronization, bounded exits,
  runtime image generations, executable-memory invalidation, generic native-override/original-call
  dispatch, PSX hardware/HLE services, host devices, and independent test infrastructure.
- Each title owns executable/disc identity, native game behavior, title policy, frame composition,
  rendering policy, and enhancement semantics.
- Lightrec owns its translated-code memory and cache. Do not wrap or duplicate those owners locally.
- Native override keys include complete resident/overlay image generation plus guest address. Address
  alone is invalid where overlays reuse a slot.
- A normal call observes the active native override. A scoped original call suppresses only that
  override for the duration of one call and executes the matching guest body through Lightrec.
- Loading or replacing executable bytes and changing an override invalidates every translated path
  that captured the prior bytes or dispatch decision.
- Host work, VSync/frame suspension, interrupts, exceptions, and thread completion use explicit
  bounded executor exits. Do not unwind C++ exceptions through translated frames.

## Preserved execution facts

Tomba! 2's `game/core/frame_driver.*` is the sole per-frame transaction owner.
`TombaRuntime::bootInit` is finite. One `TombaFrameDriver::stepFrame` composes title-aware input,
timing/event delivery, game tasks, native rendering, diagnostics, and exactly one presentation fence.
Guest VSync at `0x80085900` is fatal; a successful guest wait/query or timeout is never pacing.

Recorded pre-migration runs reached GAME at frame 25, free-roam at frame 216, and 620/620 presentation
fences. These are the frontier to regain, not Lightrec evidence. Binary findings identify
`0x801113B4` in both A03 and A0B with different entry shapes, providing a grounded image-collision
case for runtime dispatch and invalidation.

Tomba! 1's preserved facts are title-local. Its CRT0 contract includes BSS
`[0x8009AFB0,0x800A3348)`, SP `0x801FFFF8`, heap `0x800A3348+0x15C8B0`, gp
`0x80097FA8`, first `A(39h)` wrapper `0x8006B70C`, and game main `0x800163B0`.
The deterministic boundary agrees on 35/35 target/PC/register fields and has forced-mismatch plus
too-short negatives. The current CD facts include public/internal `CdSync` at `0x800648C8` and
`0x80065470`, fatal guest-VSync return address `0x800654A4`, DMA registration `0x80067E84`,
callback `0x80066D80`, and recorded movie progress beyond LBA 58739.

## Native engine rules

- Native ownership is a deliberate title boundary, not a workaround for missing CPU semantics.
  Fix MIPS decode/lowering/state defects in shared Lightrec integration, never at one guest address.
- RE before implementation. Use Ghidra headless and the executable or authenticated overlay to
  understand a behavior. Generated output may corroborate a historical finding but is not the
  implementation recipe or a source of new executable bodies.
- Native code reads as game code: cohesive classes, typed views over guest-visible state, explicit
  invariants, named states/constants, narrow interfaces, and no opaque address soup.
- Use the shared guest ABI boundary for arguments, results, and guest-visible state. Preserve required
  guest stack/register behavior when native and guest code interoperate, but do not transcribe an
  instruction stream into C++.
- One behavior has one owner. Do not add environment-gated duplicate implementations, raw fallback
  tables, title-local CPU/cache wrappers, or special cases for one failing input.
- Fail fast on missing execution, invalid identity, unknown service boundaries, and impossible state.
  Do not swallow, retry-until-pass, fast-forward, or patch phase/timer/scene state.
- Title launchers and automation are Python through the locked `uv` environment. `run.sh` remains
  the only shell entry point and never runs tests.

## Rendering

`external/psxport/docs/presentation.md` is the authority. The picture is the guest's GP0 output on
the Beetle GPU device. Native producers are overrides of the guest functions that draw; they write
the same packets, keyed by producer, object and emission index, and only keyed primitives are
interpolated. Anything without a producer is presented as the guest drew it. Rendering and
interpolation write no guest memory beyond the packets the replaced function writes.

Tomba! 2 gets widescreen (projection widening plus margin producers) and 60 fps interpolation of
keyed primitives. Tomba! 1 gets widescreen only.

## Evidence and workflow

Consult the appropriate authority before work:

- `docs/project-goals.md`: durable outcomes and success conditions.
- `docs/project-state.md`: verified/partial/blocked/missing capability inventory and current focus.
- `docs/migration.md`: title order and migration/deletion gates.
- `docs/re-frontier.md` and `titles/tomba1/docs/re-frontier.md`: ordered RE dependencies.
- `docs/codemap.md`: subsystem placement; its last section is the Tomba! 2 guest address index.
- `docs/issues/` and title-local issues: atomic open work.

A diagnostic must report how much it scanned and prove both positive and negative answers. Absence of
a symptom is not evidence without reachability. Validate tools against a case that must differ.

Boot, logos, menus, attract loops, FMV, a leaf override, clean internal traces, and still screenshots
are checkpoints, not representative gameplay. The title completion gate covers player input, guest
PC/register and memory state, interrupts/timing, relevant CD/GPU/SPU behavior, native override
reachability, overlay invalidation, correctness, and frame-time budgets on each released host.
Independent evidence comes from a trusted emulator, hardware, binary analysis, or separately built
test interpreter in a separately built test target, including diagnostics—not the retired static
product. Link and selector inspection prove that interpreter is absent from gameplay products.

Preserve useful behavioral and address findings when replacing stale plans. Remove generated-symbol
workflows and static-process instructions rather than appending a contradictory dynamic section.
