# Tomba port agent instructions

This repository ships native title overrides around the shared `psxport` Lightrec dynarec. Remaining
guest instructions execute from the user's authenticated game image at runtime. The gameplay products
must not generate or compile guest C/C++ or expose an interpreter-first gameplay mode. Lightrec is
the default executor. The shared runtime may enter its bounded interpreter fallback only after a
translation failure or unavailability, unsafe fetch, or rare unsupported block; every entry is
reason-coded and counted against an explicit threshold. A forced interpreter mode is diagnostic-only,
and fallback-covered execution is not gameplay-conformance or performance evidence.

Read [`docs/migration.md`](docs/migration.md), [`docs/project-state.md`](docs/project-state.md), and
[`docs/re-frontier.md`](docs/re-frontier.md) before execution work. Tomba! 2 is the active title: first
prove a resident override and a colliding-overlay override, including scoped calls to each original
guest body through the dynarec; then restore the recorded gameplay frontier and pass a representative
interactive scenario. The generated paths are already removed. Tomba! 1 follows with its recorded
35-field CRT0 boundary and current CD/movie/title frontier.

Do not recreate a generator, generated corpus, static dispatcher, or static product. Measured evidence
is retained independently of that deleted machinery. An unbounded fallback, silent fallback, or
user-selected gameplay interpreter is a product defect.

Read [`CLAUDE.md`](CLAUDE.md) for still-valid title behavior, guest addresses, and native subsystem
contracts. Its offline CPU-source execution vocabulary and generated-symbol workflows are superseded
by `docs/migration.md`; do not follow them. When this repository is inside the PSX workspace, also
read [`../AGENTS.md`](../AGENTS.md) and [`external/psxport/AGENTS.md`](external/psxport/AGENTS.md).

## How the product starts

`game/core/entry/main.cpp` composes `psx::host::ProductHost` over `TombaCatalog`. Zero arguments open the
picker; one executable argument runs that title. The catalog entry carries the disc's boot executable
SCUS_944.54; `TombaRuntime::registerOverrides` loads and authenticates `MAIN.EXE` from the disc
(`entry/main_handoff.cpp`) before the host's crt0 audit. The SCEA card and LOGO movie are stepped frames
(`frame/boot_cards.cpp`). The dev `warp`, `items` and `flag` commands arrive through `TombaRuntime::controlCommand`.
Tomba! 1 is not catalogued; its binary under `titles/tomba1/` is unchanged.
