# Tomba! (`SCUS_942.36`)

Tomba! 1 is the second title in this repository's native/dynarec migration. It owns a separate engine
under `titles/tomba1/game/`; no Tomba! 2 game code, addresses, overlay assumptions, rendering policy,
or temporal systems cross that boundary.

The intended product combines Tomba! 1 native C++ overrides with the shared `psxport` Lightrec
executor for all remaining guest instructions. The gameplay binary contains no offline-emitted guest
source. Lightrec is the default; shared bounded interpretation is allowed only for translation failure
or unavailability, unsafe fetch, or a rare unsupported block. Every fallback is reason-counted against
an explicit threshold. Forced interpreter mode is diagnostic-only, and fallback-covered execution is
not gameplay-conformance or performance evidence.

Implementation is deliberately deferred until Tomba! 2 passes representative gameplay through
Lightrec. Tomba! 1's guest-source product and emission tooling are already removed; do not recreate
them.

## Preserved frontier

The selected-disc provisioner verifies `SYSTEM.CNF` and the measured 559,104-byte
`SCUS_942.36` before publishing untracked bytes. Recorded startup evidence establishes all 35
target/PC/register fields at the first `A(39h)` call, including forced-mismatch and too-short
negative cases.

Pre-migration product evidence reaches visible SCEA and Whoopee Camp presentation and advances the
movie stream beyond LBA 58739. The next boundary is issue 0006: public `CdSync` `0x800648C8`
forwards to internal `0x80065470`, whose timeout reaches fatal guest VSync with return address
`0x800654A4`. DMA callback registration at `0x80067E84` installs callback `0x80066D80`.
These are behavioral/address facts to preserve, not instructions to rebuild the generated path.

The offline-emitted guest-source path is already removed and is not a fallback. When this title resumes,
Lightrec first reproduces the 35-field CRT0 boundary, then routes the six engine-neutral native
overrides and scoped original calls by complete image identity, crosses the recorded CD/movie
boundary, reaches the title screen, demonstrates input, and passes representative gameplay.

## Enhancement scope

Tomba! 1 targets true widescreen only. It exposes no interpolation, temporal-history,
title-native-renderer, native-producer, native-depth, 60fps, or native-rendering option. Widescreen
must show additional correctly projected content through title-owned projection, visibility,
wide-buffer, and 2D-layout policy; stretching or cropping a 4:3 result does not qualify.

See `docs/project-goals.md`, `docs/project-state.md`, `docs/re-frontier.md`, and
`docs/codemap.md` for the title authorities.
