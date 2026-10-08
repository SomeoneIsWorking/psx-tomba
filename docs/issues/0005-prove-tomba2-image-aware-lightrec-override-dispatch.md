---
id: 5
title: Prove Tomba! 2 image-aware Lightrec override and original-call dispatch
status: open
symptom: The planned Lightrec product has no shipping proof that resident and colliding-overlay calls select the correct native owner and can call the original guest body
state_items: S001
tags: tomba2,dynarec,lightrec,overrides,overlays
created: 2026-09-04
updated: 2026-09-12
---

## Required discriminator

After the shared per-`Core` Lightrec executor is available, exercise one reached resident native
override and one address reused by two authenticated overlays. `0x801113B4` is a grounded candidate:
binary evidence identifies different entry shapes at that numeric address in A03 and A0B.

For the resident and both overlay-image cases, prove that normal dispatch reaches the intended native
owner, a scoped original call executes the corresponding guest body through Lightrec and returns, and
a mismatched image cannot reuse either the override decision or a stale translated block. Report
call/block denominators and include a forced-negative case so silence cannot pass.

This issue is the first title discriminator, not the completion gate. The generated/static path is
already absent. Tomba! 2 still must regain its recorded free-roam frontier and pass representative
interactive gameplay without a fallback.

## Current synthetic coverage

`tomba_native_override_catalog` exercises the production declaration/binding
owner and the shipping Lightrec dispatcher: a first-image native call, a stale
generation negative that executes guest code, idempotent rebinding to the final
generation, a different image at the same address that remains on the JIT path,
and a scoped original call with nonzero translated execution and zero interpreter
fallback. Binding requires an explicit resident image token and the canonical
resident text range; only the two resident-load lifecycle points acquire that
token. It cannot adopt another active image merely because an address matches.

This preserves the boot reload contract but uses a synthetic resident image.
Launch-time image authentication is now owned by `tools/tomba2_provision.py` and
`config/tomba2-images.json`, described below. An image-generation token still
establishes residency rather than authenticity at the runtime loading boundary.
The title now owns an image-scoped MODE overlay lifecycle: both faithful and normal area loaders
activate the authenticated A00–A0L image after loading its fixed MODE slot, retire the previous
image's native entries, and bind only declarations for the active image and loaded text range.
The catalog test proves two declarations at one numeric address (A03/A0B) remain isolated across
activation, dispatch, original-call, and wrong-image cases. That catalog test uses synthetic image
bytes; the authenticated but still diagnostic collision probe is recorded below.

## Authenticated A03/A0B collision probe, 2026-09-12

The CTest target `test_authentic_overlay_collision` validates the local A03.BIN and A0B.BIN
against the tracked USA image manifest and runs the collision itself. Its declarations at the real shared entry
`0x801113B4` are **diagnostic fixtures**, not native producers in the shipping title. The test
copies each authenticated body into the title's MODE slot and uses the production
`activateModeOverlay`, normal dispatcher, and scoped Lightrec original-call path. A03's real
wrapper and callee return through the binary's pool-capacity exit; A0B's different real body
decrements a timer and advances node state 1→2. Replacement removes the A03 native entry,
guest byte writes invalidate translations, and a forced unrelated image token at the same address
executes the A0B guest body without selecting either native fixture. Wrong digest and missing
overlay inputs both refuse before execution.

The positive probe reported 2/2 diagnostic native calls, 2/2 original guest returns, 14
translated blocks, 161 translated instructions, 115,561 A0B-load invalidation notifications,
and 0 fallback blocks. Those notifications are 115,560 byte writes plus one descriptor write
through the guest-memory API, not a count of distinct stale blocks. This proves authenticated
collision semantics for these bounded paths. No shipping A03/A0B overlay override is declared,
and no real game route reached `0x801113B4` under either overlay. The required resident native
original-call comparison, reached overlay owner calls, exact runtime overlay-content
authentication, and representative gameplay conformance remain open.
On pinned psxport `4e8dcc0f`, the combined Clang asset-free gate passed 23/23 CTest cases,
the execution-boundary scan, and the build-receipt pin check; the separate local real-byte
probe also passed against that gate's binary.

## Authenticated resident original-call probe, 2026-09-12

The CTest target `test_authentic_resident_override` checks local MAIN.EXE against the tracked USA
manifest, then rechecks that exact buffer and maps it with the shipping
`loadPsxExeImage` path. It registers the actual `tomba::Str::length` native owner at resident
`0x80079528` through the title catalog, calls it through normal image-qualified dispatch, and
calls the same authenticated guest body through scoped `callOriginal`. Both return `v0=v1=4`,
advance `a0` to the terminator, and match all 32 GPRs plus return PC. This exposed a native
wrapper defect: it previously left `a0` at the source pointer despite the guest loop advancing it.
The wrapper now mirrors the observed guest mutation. A neighboring `jr ra` entry at
`0x8007954C` and the exact entry after removing its native registration each run guest code
without selecting the owner. The positive probe reports one native, one original, two guest
negative-control returns, 9 translated blocks, 54 translated instructions, and zero fallback.
The pre-existing A03/A0B authenticated probe still passes with 14 translated blocks, 161
instructions, and zero fallback after sharing the exact-buffer authentication helper.
The combined asset-free Clang gate on psxport `ff3709e7` passed 24/24 CTest cases, the product
execution-boundary scan, and the build-receipt pin check. Both local real-byte probes passed
against that same build, and a deliberately wrong resident digest refused with exit 2 before
mapping any image.

This is real MAIN.EXE byte and shipping owner/dispatcher evidence in an isolated fixture. It does
not establish that a retail Tomba! 2 route reached `Str::length`, nor a reached original-call
comparison inside gameplay. The A03/A0B owners remain diagnostic fixtures, and runtime
overlay-content authentication and representative gameplay are still open.

## Stage slot dispatch frontier, 2026-09-12

A Clang product built from `4fe4e20` and the pinned Lightrec framework loaded the authenticated
disc, initialized native systems, and entered the DEMO stage. The 350-frame headless auto-drive
observation reached logged frames 0–3, then `Demo::s2` called `0x8010696C` and the shipping
dispatcher refused it with `ambiguous code-image identity` after zero guest cycles. The root cause
is that the stage loader copied START/DEMO/GAME code into their shared `0x80106228` slot without
publishing an image generation. The earlier MODE activation covered only A00–A0L.

The stage loader now owns its active token per `Engine`, activates the loaded code after the
synchronous CD read, retires the prior stage on replacement, and binds the two DEMO sub-machine
declarations only to the DEMO image. The dead parallel stage-loader body was replaced by this
single live owner. The production catalog and Lightrec dispatcher pass 20 focused checks,
including START→DEMO→GAME dispatch, DEMO scoped original call, wrong-stage rejection, and
invalidation. The task's second context word is the caller's `gp` returned by resident
`FUN_80080930`, not another entry PC; the live loader now preserves this distinct value and
the production catalog checks the resident-stage write with unequal entry/`gp` fixtures.
The authentic rerun passed the former frame-3 fault, entered GAME at frame 25,
and reached SOP's area-load body at frame 27. Its next call to guest `0x8010A8D4` faulted
before any guest cycle because `SOP.BIN` had been loaded into the MODE slot during DEMO s0
without publishing an image generation. The normal DEMO loader now activates SOP immediately
after that synchronous read. The same index-based MODE activation owner reads the guest
descriptor size for SOP (index 2) and A00–A0L (indices 3–24), so an area replacement retires
SOP and its compiled blocks. Synthetic SOP→A03 replacement, scoped original calls, and
invalidation pass. The original A03/A0B
real-image discriminator remains open.

The next authentic rerun bound SOP at DEMO frame 1, passed its former `0x8010A8D4` fault,
completed the opening intro, and bound A00 at GAME frame 113. At frame 115 the first field
tick entered `ActorTomba::frameTick`'s scripted branch and called `0x8018BD30`; dispatch again
refused an unqualified image after zero guest cycles. The extracted OPN.BIN has a real function
prologue at offset `0x1D30`, exactly this address from its `0x8018A000` load base. The
`bf89c==2` transition branch calls `FUN_80045558(0)` to load OPN after the raw area-data read,
but the AREA slot had no executable generation. The title now activates OPN on that completed
load and CRD on the front-end load, and retires AREA identity before texture/raw-data writes
reuse the same slot. Synthetic OPN→CRD dispatch, original-call, invalidation, and raw-data
retirement checks cover the catalog path. The focused test passes 25/25 checks, including
the distinct task entry/`gp` write. A corrected-loader authentic 350-frame headless auto-drive
rerun bound OPN after A00 at frame 113, crossed the former
frame-115 fault, reported free-roam at frame 216, and exited cleanly at the cap. Lightrec
reported 82,741 executor calls, 1,688,706 executed blocks, 19,726,370 executed instructions,
and zero fallback/refused-fallback blocks. This is a real-image load and finite execution
observation, not an independent oracle or representative interactive gameplay. The next exact
title discriminator is the authenticated A03/A0B colliding-address native/original-call case.


## Launch-time image authentication

The provisioner previously accepted any same-size executable or overlay and skipped
extraction when a cache file existed. A valid cache could therefore hide selection
of an unrelated disc, while later streaming still used that disc.

`config/tomba2-images.json` owns the 30 supported runtime-image sizes and SHA-256
fingerprints. Provisioning extracts the selected disc's complete set into private
staging, authenticates every image, then publishes validated files with atomic
per-file replacement. Publication is not an all-files transaction: every previously
valid image must match the same immutable manifest, so partial publication cannot
mix valid revisions. Publication errors still refuse launch, and repair of an
invalid cache can remain incomplete. A missing image, changed byte, or extraction failure refuses
launch before changing any published file. Every invocation reads the selected
disc again; corrupt cache files can be replaced from the authenticated source.
The manifest contains fingerprints only, never game bytes. MAIN.EXE's retained
input has the independent historical C044 MD5 `31f07b2bee1fbb685cbc4c0afecc6f89`.

Verified 2026-09-08: `uv run --frozen python tests/test_tomba2_provision.py` passes
10/10 focused regressions:
correct resident/overlay outputs, wrong size, oversized input with a stale size
report and one bounded read, same-size corruption, wrong selected
disc hidden by cache, late-overlay failure without early publication, corrupt-cache
repair, empty-manifest refusal, and extraction-failure cleanup. Fresh extraction from the user-supplied Tomba! 2 USA disc matched all 30/30
manifest entries (3,993,565 bytes); post-publication validation also matched 30/30.
Qualification used a separate scratch output and the existing `discdump`, leaving
normal runtime inputs unchanged. The 12/12 launcher tests also pass, including
launching the binary from the same toolchain-specific build directory just built.
The former launcher incorrectly executed `build/bin/tomba2_port` instead.

This covers launcher provisioning. Direct executable loading and CD-loaded overlay
activation still need runtime authentication and complete image-aware native binding.
It does not complete this issue's original-call or representative-gameplay proof.
