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
- evidence: C002/I002. The host's `executable_provisioner` reads `SCUS_942.36` from the `PSXPORT_TOMBA1_DISC` disc and publishes it to `scratch/bin/tomba1/` only when size and SHA-256 match `tomba1::identity()`.
- where: `game/core/entry/identity.cpp`; psxport `runtime/psx/host/executable_provisioner.cpp`; user-supplied disc; untracked runtime input
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
- status: partial
- deps: T1-05
- evidence: Pre-migration execution reached visible SCEA and Whoopee Camp frames, loaded `OPTSUB00` at `0x800E7388`, and advanced beyond LBA 58739. Public/internal `CdSync` are `0x800648C8`/`0x80065470`; the current fatal VSync return address is `0x800654A4`. DMA registration `0x80067E84` installs callback `0x80066D80`. Through Lightrec the product now plays the publisher card, the Whoopee Camp logo and both opening movies (issue 0008 resolution). Load-list walker `0x80021D70`: type `0xF0` lands code at `0x800E7388`, type `0xF8` data at `0x80097FA8`. Movie task `0x8001F1C0`: state 3 calls `StUnSetStream` `0x80066D28` (`CdReadyCallback(0)` `0x80064920`), `0x80066CC8`, `CdControlB(Pause)` `0x80064B9C`, then task exit `0x80017208` (record state 0, `CloseTh`, `ChangeTh(0xFF000000)`). `StCdInterrupt` body `0x800670D0` calls `CdReady(1)` and starts DMA3 through `0x80067A74`, which spins on CD status `0x1F801800` bit 6. libetc `VSync(-1)` returns `0x800974D4`.
- where: title-native owners in `game/core/`; shared image-aware override/original-call dispatcher and Lightrec executor
- gap: With no input the intro movie (`16:34:22`) replays; Start reaches the title menu and New Game the first field (2026-10-16). Movie frames still present clipped (S004).

### T1-07 — Prove representative interactive gameplay
- status: todo
- deps: T1-06
- evidence: Logos, movie playback, and title-screen input are checkpoints, not representative gameplay.
- where: shipping product, deterministic gameplay scenario, and independent emulator/hardware or separately built test oracle
- gap: Cover player input, guest PC/registers and memory, interrupts/timing, relevant CD/GPU/SPU behavior, native override reachability, overlay invalidation, and declared frame-time/correctness budgets.

### T1-07a — Area transition owner for the dev warp
- status: grounded
- deps: T1-06
- evidence: Ghidra on `SCUS_942.36` (names from psx_tomba's symbol map, addresses cross-checked): `gameplayStateDispatcher` `0x8001AC00` switches on the gameplay task record (`*0x1F8001D4`) `+0x4C`, the area machine: 0 area select/load `0x8001B0A4`, 1 field `0x8001B2B4` (state 1 ticks), 2..6 area modes, 7 `areaTransitionHandler` `0x8001CFCC`, 8 game over; a pending pair `0x1F8001DC`/`0x1F8001DE` is applied when `0x1F8001CE` is set. The door at `0x8002CFFC` writes the destination u16s `0x8009C0FC` area, `0x8009C0FE` section, `0x8009C108` entry, calls `stopBgm` `0x80020C00`(1) when the area changes, then sets machine 7 state 0 (`0x8002D498`). State 3 of `0x8001CFCC` copies the destination to the current area `0x8009BCC8`/`0x8009BCCA` and loads it. The debug area select `0x8001AD58` bounds the area by `0x8007B290` and the section by the u16 table `0x8007B294`.
- where: `game/core/debug/dev_warp.cpp`
- gap: Entry numbers per area are not catalogued; the warp accepts 0..255.

### T1-08 — Remove the Tomba! 1 guest-source path
- status: re-verified
- deps: T1-03
- evidence: The emitted guest source, generator, emission-only seeds, offline registry/build inputs, and address-based tests are absent. The host publishes only authenticated executable bytes.
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
