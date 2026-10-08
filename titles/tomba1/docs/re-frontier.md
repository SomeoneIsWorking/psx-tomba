# Tomba! 1 native/dynarec RE frontier

Statuses follow the workspace RE-frontier vocabulary. The guest-source path is already removed; title
execution work starts only after Tomba! 2 representative gameplay is complete. The chain preserves
executable-grounded startup and CD facts while introducing the shared Lightrec product.

## Execution spine

### T1-00 — Select and measure the target executable
- status: re-verified
- deps:
- evidence: C001/I001. `SCUS_942.36` is 559,104 bytes with SHA-1 `81cbc79f0230aeb4252e058039f47ac95a777f5a`; its PS-X header reports entry `0x8006B58C`, text `[0x80010000,0x80098000)`, and initial SP `0x801FFFF0`. The verifier compares 15 facts and detects an altered byte.
- where: `executable.json`; `tools/verify_executable.py`; private user input outside Git
- gap: None for the selected USA executable.

### T1-01 — Provision the selected disc reproducibly
- status: re-verified
- deps: T1-00
- evidence: C002/I002. `tools/provision.py` resolves explicit argument, `PSXPORT_TOMBA1_DISC`, `.env`, then exactly one root CHD; requires exactly one `SYSTEM.CNF` boot declaration naming root `SCUS_942.36`; and publishes only after 15/15 executable agreement. Positive and negative cases distinguish agreement from missing, ambiguous, malformed, wrong-target, and altered input.
- where: `tools/provision.py`; user-supplied disc; untracked runtime input
- gap: None for the selected USA disc.

### T1-02 — Recover CRT0 and startup semantics
- status: re-verified
- deps: T1-01
- evidence: Binary analysis and independent execution establish BSS `[0x8009AFB0,0x800A3348)`, SP `0x801FFFF8`, heap base `0x800A3348`, heap size `0x15C8B0`, gp `0x80097FA8`, `A(39h)` wrapper `0x8006B70C`, and game main `0x800163B0`.
- where: executable-backed startup evidence; title runtime facts in `game/core/`
- gap: None at the semantic boundary.

### T1-03 — Establish the 35-field independent boundary
- status: re-verified
- deps: T1-02
- evidence: C004/I003. Two independent-oracle runs are deterministic and agree on 35/35 target/PC/register fields at the first `A(39h)` call after 42,140 steps. A forced `gp` mismatch and 100-step refusal produce the opposite answer.
- where: recorded C004/I003 evidence
- gap: None for the expected boundary. The Lightrec product leg is T1-05.

### T1-04 — Consume the shared psxport Lightrec executor
- status: todo
- deps: T1-03, T1-08
- evidence: Tomba! 1 has no Lightrec product proof. The shared executor is first qualified by Tomba! 2.
- where: shared `external/psxport/` executor; title composition
- gap: After Tomba! 2 passes representative gameplay, compose the per-`Core` executor with title state, bounded exits, image generations, and invalidation. Prove interpreter absence by product link and selector inspection; any interpreter exists only in a separately built test target, including diagnostics.

### T1-05 — Reproduce the 35-field CRT0 boundary through Lightrec
- status: todo
- deps: T1-04
- evidence: T1-03 supplies the complete expected state and negative controls.
- where: shipping Lightrec product boundary plus separately built independent test oracle
- gap: Run the authenticated executable to `0x8006B70C`, report 35/35 fields and nonzero translated blocks, then prove the forced mismatch and too-short refusal still fire.

### T1-06 — Route title overrides and cross the current CD/movie frontier
- status: todo
- deps: T1-05
- evidence: Pre-migration execution reached visible SCEA and Whoopee Camp frames, loaded `OPTSUB00` at `0x800E7388`, and advanced beyond LBA 58739. Public/internal `CdSync` are `0x800648C8`/`0x80065470`; the current fatal VSync return address is `0x800654A4`. DMA registration `0x80067E84` installs callback `0x80066D80`.
- where: title-native owners in `game/core/`; shared image-aware override/original-call dispatcher and Lightrec executor
- gap: Exercise all six engine-neutral native overrides and scoped original calls through the shipping dispatcher, cross issue 0006 without weakening the VSync trap, preserve the recorded CD/DMA/movie behavior, then reach the title screen and demonstrate input.

### T1-07 — Prove representative interactive gameplay
- status: todo
- deps: T1-06
- evidence: Logos, movie playback, and title-screen input are checkpoints, not representative gameplay.
- where: shipping product, deterministic gameplay scenario, and independent emulator/hardware or separately built test oracle
- gap: Cover player input, guest PC/registers and memory, interrupts/timing, relevant CD/GPU/SPU behavior, native override reachability, overlay invalidation, and declared frame-time/correctness budgets.

### T1-08 — Remove the Tomba! 1 guest-source path
- status: re-verified
- deps: T1-03
- evidence: The emitted guest source, generator, emission-only seeds, offline registry/build inputs, and address-based tests are absent. The title provisioner publishes only authenticated executable bytes.
- where: absent by design; enforced by title composition and launcher boundaries
- gap: None for removal. Fresh native/Lightrec build and launch evidence belongs to T1-04 through T1-07.

## Widescreen spine

### T1-09 — Identify projection, visibility, buffer, and layout owners
- status: todo
- deps: T1-08
- evidence: `SetGeomOffset` `0x80063A34`, `SetGeomScreen` `0x80063A54`, initializer `0x80016AF4`, display construction `0x80016C4C`, and later resident `0x8002D784` ground the current 320x224 centre `(160,112)`, `H=544` projection facts.
- where: binary-backed RE and `game/render/`
- gap: Observe resident and loaded-code publications, distinguish visual from gameplay culling, identify wide draw-buffer placement, and classify left/right/center 2D anchors.

### T1-10 — Implement and verify true widescreen
- status: todo
- deps: T1-09
- evidence: Not started.
- where: title-owned projection/layout modules and shared non-temporal presentation
- gap: Preserve 4:3 and prove additional correctly projected content, visibility, edge coverage, buffer placement, and anchored UI in representative gameplay.

## Explicitly excluded enhancement work

### T1-X1 — Interpolation and temporal history
- status: skip-by-design
- deps:
- evidence: User scope explicitly selects widescreen as Tomba! 1's only rendering enhancement.
- where: no title owner
- gap: None. Do not add interpolation, frame synthesis, or prior/current presentation state.

### T1-X2 — Title-native renderer, producers, depth, or 60fps options
- status: skip-by-design
- deps:
- evidence: User scope and `enhancement_scope.json` exclude these surfaces.
- where: no title owner; shared psxport remains presentation owner
- gap: None. Unsupported modes stay absent rather than hidden or disabled.
