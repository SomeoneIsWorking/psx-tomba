# Tomba port

Two isolated native/dynarec ports, Tomba! 2 (`SCUS_944.54`) and Tomba! 1 (`SCUS_942.36`), over the
shared `psxport` Lightrec runtime. Execution policy: the `dynarec-port`, `dynarec-runtime` and
`dynarec-overrides` skills. When this repository is inside the PSX workspace, also read
[`../AGENTS.md`](../AGENTS.md) and [`external/psxport/AGENTS.md`](external/psxport/AGENTS.md).

## Docs

- [`docs/migration.md`](docs/migration.md): title order and migration gates. The static products and
  their generator are removed.
- [`docs/re-frontier.md`](docs/re-frontier.md) and
  [`titles/tomba1/docs/re-frontier.md`](titles/tomba1/docs/re-frontier.md): RE chains per title.
- [`docs/features.md`](docs/features.md): feature hops; [`docs/guest-addresses.md`](docs/guest-addresses.md):
  the Tomba! 2 guest address index; [`docs/producers.md`](docs/producers.md): the 60 fps producers.
- Rendering: [`external/psxport/docs/presentation.md`](external/psxport/docs/presentation.md).

## Titles and isolation

Tomba! 2 is the active title; root `game/` is its engine. Its next steps: one resident override and
one colliding-overlay override with scoped original calls, then the recorded gameplay frontier and a
representative interactive scenario.

Tomba! 1 lives entirely under `titles/tomba1/` as the `tomba1_runtime` library, linked into
`tomba2_port`. The two trees share no code; `tomba_title_link_isolation` refuses an external name
both define. Tomba! 1 reuses only generic `psxport` services, never a Tomba! 2 address, overlay
assumption, game owner or renderer policy.

Deltas: Tomba! 2 gets widescreen (projection widening plus margin producers) and 60 fps
interpolation of keyed primitives; Tomba! 1 gets widescreen only.

## Ownership

- `external/psxport/` owns Lightrec integration, per-`Core` CPU sync, bounded exits, image
  generations, executable-memory invalidation, native-override and original-call dispatch, PSX
  hardware/HLE services, host devices and test infrastructure.
- Each title owns executable/disc identity, native game behavior, title policy, frame composition,
  rendering policy and enhancements.

## Native code and evidence

- Host work, VSync/frame suspension, interrupts, exceptions and thread completion use explicit
  bounded executor exits. Do not unwind C++ exceptions through translated frames.
- Native code reads as game code: typed views over guest-visible state, named states and constants,
  no opaque address soup. Do not transcribe an instruction stream into C++.
- A diagnostic reports how much it scanned and proves both positive and negative answers. Absence
  of a symptom is not evidence without reachability. Validate tools against a case that must differ.
- Boot, logos, menus, attract loops, FMV, a leaf override, clean traces and still screenshots are
  checkpoints, not representative gameplay. The title completion gate covers player input, guest
  PC/register and memory state, interrupts/timing, relevant CD/GPU/SPU behavior, native override
  reachability, overlay invalidation, correctness and frame-time budgets on each released host.

## How the product starts

The entry is the framework main (`psxport_add_product`), running `psx::host::ProductHost` over
`tomba::Catalog`: Tomba! 1 (index 0, `tomba1::Runtime`) and Tomba! 2 (index 1, `tomba::Runtime`).
Zero arguments open the in-window picker; one executable argument runs that title directly. The host
extracts each executable from its disc (`PSXPORT_TOMBA1_DISC`, `PSXPORT_TOMBA2_DISC`) into
`scratch/bin/<slug>/`; `run.sh` takes no disc and provisions nothing. `tools/provision.py` extracts
only the maintainer evidence set (`MAIN.EXE`, the area overlays) read by the authentic tests and RE.

The Tomba! 2 entry carries the boot executable `SCUS_944.54`; `tomba::Runtime::registerOverrides`
loads and authenticates `MAIN.EXE` from the disc (`entry/main_handoff.cpp`) before the host's crt0
audit. The SCEA card and LOGO movie are stepped frames (`frame/boot_cards.cpp`). The dev `warp`,
`items` and `flag` commands arrive through `tomba::Runtime::controlCommand`.

## Tomba! 2 facts

- `game/core/frame/frame_driver.*` is the sole per-frame owner. `tomba::Runtime::bootInit` is
  finite. One `tomba::FrameDriver::stepFrame` composes input, timing/events, game tasks, native
  rendering, diagnostics and exactly one presentation fence.
- Guest VSync at `0x80085900` is fatal; a guest wait/query or timeout is never pacing.
- Pre-migration runs reached GAME at frame 25, free-roam at frame 216 and 620/620 presentation
  fences: the frontier to regain, not Lightrec evidence.
- `0x801113B4` exists in both A03 and A0B with different entry shapes: the grounded image-collision
  case for runtime dispatch and invalidation.

## Tomba! 1 facts

- CRT0: BSS `[0x8009AFB0,0x800A3348)`, SP `0x801FFFF8`, heap `0x800A3348+0x15C8B0`, gp `0x80097FA8`,
  first `A(39h)` wrapper `0x8006B70C`, game main `0x800163B0`. The boundary agrees on 35/35
  target/PC/register fields, with forced-mismatch and too-short negatives.
- CD: public/internal `CdSync` at `0x800648C8` and `0x80065470`, fatal guest-VSync return address
  `0x800654A4`, DMA registration `0x80067E84`, callback `0x80066D80`; recorded movie progress beyond
  LBA 58739. Next: the CD/movie boundary, title screen, input, representative gameplay.
