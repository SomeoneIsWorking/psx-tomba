---
id: 8
title: The guest streams its own code module, and nothing publishes an image identity for it
status: open
symptom: After boot, guest task 0 faults with "ambiguous code-image identity" at 0x800E7D5C
state_items: S004
tags: tomba1,image-identity,module-load,cd,executor-boundary
created: 2026-09-29
updated: 2026-09-29
framework_owner: psxport — `runtime/psx/cd_override.cpp` (`cd_read_stock_sync`) and the absence of a title-facing module-residency seam
---

## Symptom

With the boot decompress fixed (`0007`), Tomba! 1's product presents and then stops:

    [native-dispatch:error] guest address 0x800E7D5C resolves to zero or multiple active code images;
                               image identity lookup: claimed by none
    [executor:error] host dispatch to 0x800E7D5C FAILED (ambiguous code-image identity);
                      the block that produced this target began at 0x80019844
    [tomba1-frame:error] guest task 0 exited with fault at 0x800E7D5C after 782 of 564480 budget cycles

It is a **fault after boot**, not a hang and not a fault during it. The guest got far enough to
stream its own next stage and to call into it.

## Measured cause

**1. The call target is reached by a direct `jal` from resident code, at two sites.**
`0x80019A7C jal 0x800E7D5C` and `0x80019A94 jal 0x800E7D5C`. Both are immediates in the resident
image, so the target is not computed and there is no indirect-dispatch subtlety.

**2. The target is NOT in the resident executable.** Resident text is `[0x80010000, 0x80098000)`
(`GuestProgramImage::residentText`). `0x800E7D5C` is `0x475C` above its end, so the resident image
cannot claim it and `ImageCatalog::resolve` returns nothing for it.

**3. The guest itself streamed a code module there.** With `PSXPORT_DEBUG=cd`:

    [cd] CdRead 7 sector(s) x 2048 bytes from LBA 103311 -> 0x800E7388 (mode 0x80)

7 x 2048 = **14,336 bytes** into `0x800E7388`, so the loaded range is
`[0x800E7388, 0x800EB188)`. The faulting address sits at offset **`0x9D4`** inside it, and the bytes
there are real code: **14 of 16** words in `[0x800E7D50, 0x800E7D90)` are non-zero.

**4. Nothing in this repository publishes an image identity for it.** Measured over
`external/psxport/runtime`, this repo's `game/`, and `titles/tomba1/`, there are exactly **three**
`imageCatalog().activate(` call sites:

| site | what it publishes |
|---|---|
| `runtime/psx/psx_exe_image.cpp:109` | the main PS-X EXE, once, at boot |
| `game/core/native_override_catalog.cpp:242` | **Tomba! 2's own** overlay load, from its native scene code |
| `titles/tomba1/tests/test_guest_task_budget_resume.cpp:371` | a test fixture |

**Tomba! 1 has zero production call sites.** It has no native scene code, so the Tomba! 2 route
(`game/scene/level_load.cpp` -> `activateOverlay`) does not exist for it, and the title therefore
publishes no identity for a module its own guest loads.

## The read-only-looking half of this is a FRAMEWORK defect, measured

`ImageCatalog::activate` is the only publisher and the only caller is the main-executable loader.
That alone would be a design choice. What makes it a defect is the invalidation half:

    $ grep -n 'notifyExecutableWrite' external/psxport/runtime/psx/cd_override.cpp
    606:  ...        (inside cd_loadfile)
    663:  ...        (inside Cd::loadFile, which calls cd_loadfile)

**2 of 2 call sites are in `cd_loadfile`.** `cd_read_stock_sync` — the route Tomba! 1 actually uses,
and the only one that ran in the measured log — writes guest RAM with `c->mem_w8` and calls
`notifyExecutableWrite` **zero** times. So on this route:

- Lightrec is **not** invalidated for guest-written executable RAM, which the framework's own
  contract requires of every executable write; and
- no image identity is published, so the image-scoped dispatcher refuses to execute what was loaded.

The same read also wrote **4 sectors (8,192 B) into `0x80097FA8`**, which is *inside* resident text
(`0x80097FA8 + 8192` straddles the `0x80098000` end). Whether a translated block survives that
overwrite is a separate, untested consequence; the invalidation gap is what is measured here.

## Sufficiency, measured — and it is NOT the last blocker

To answer "is this the only thing between the title and gameplay", the identity was published
experimentally in a scratch build (temporary edit, measured, **reverted**; not in any commit).
With `[0x000E7388, 0x000EB188)` active the title **advances past the fault** and stops somewhere
else entirely:

    [tomba1-frame:error] guest task 1 spent 33 consecutive display fields without reaching its
      cooperative yield; last resume 0x8001EFE8 after 564480 cycles

So the identity gap is **sufficient to expose the next frontier, and not sufficient to reach
gameplay.** One framework change will not finish this title.

### The attractive wrong lead, recorded because it will be re-derived

The first experiment published the range as the **KSEG** address `{0x800E7388, 0x800EB188}` and
`resolve` *still* answered `claimed by none` with the image plainly active. That reads exactly like
"activation does not work", and it is wrong: `ImageCatalog::resolve` masks the guest address with
`0x1FFFFFFF` before comparing, so an image range must be **physical**. The same publish with
`{0x000E7388, 0x000EB188}` resolved immediately. `loadPsxExeImage` passes `image.physicalText`, and
Tomba! 2's `activateOverlay` callers pass `kSlot`-based physical ranges — neither says so in a way a
new caller would necessarily notice.

## The next frontier, recovered as readable C++

At `0x8001F2FC` the guest runs a retry loop whose structure is unambiguous:

```c
// 0x8001F2FC — measured, not guessed
TaskRecord *task = (TaskRecord *)[0x1F8001D4];
do {
    task->retry += 1;                       // 0x8001F2EC: lhu/addiu/sh at task + 0x48
    v0 = CallGuest_0x8001EFE8(0x800A0B10);  // 0x8001F304
} while (v0 == 0);                          // 0x8001F30C: beqz -> 0x8001F2FC
```

and `0x8001EFE8` dequeues from a ring whose base and index the guest keeps:

```c
int CallGuest_0x8001EFE8(void *arg) {       // 0x8001EFE8
    s1 = arg;
    if (LibDeque(&sp[0x10], &sp[0x14]) != 0)   // jal 0x80066FEC
        return 1;
    // ... indexes a table at 0x80070000 + 2 * [0x1F8001CD] + 0x7728
}
```

with `0x80066FEC` computing `slot = [0x800A326C] + [0x800A188C] * 32`.

**Measured at the refusal, which is what makes this a wait and not a spin:**

| quantity | value |
|---|---|
| retry counter at `task + 0x48` | **1**, after 33 fields |
| `[0x800A326C]` ring base | `0x800D7188` |
| `[0x800A188C]` ring index | `0` |
| first halfword of that slot | `0` |
| 32-byte argument at `0x800A0B10` | 8 of 8 words zero |

A retry counter of **1** after 33 fields is the load-bearing number: the loop body has **not**
iterated, so the guest is spending its whole time inside the *first* call to `0x8001EFE8`, waiting
on an **empty ring that nothing is filling**.

## Live evidence (and an explicit non-claim)

One run of the shipping product over its loopback control channel:

    [live] presented frames at first sample: real=15 in-between=0 total=15
    [live] FIRST screenshot scratch/live/tomba1/boot.ppm (215055 bytes)
    [live] pad slot0 before tap: FFFF4100 0000
    [live] tap Start across 6 presented frames -> tap start 6
    [live] product state at report time: the product exited with code 139: ...

**The presented-frame count is `real=1`, `6` and `15` across runs, and that spread is the
measurement, not noise to smooth over.** The driver connects as soon as the product's endpoint
answers, and the endpoint is attached before the boot decompress finishes, so the count is simply
"how many frames this connection got to see". It bounds the window rather than measuring a rate, and
nothing here claims a frame rate.

The screenshot was **opened and looked at**: 320x224 P6, **0 of 71,680 pixels non-black**, a single
colour `000000` across the whole frame — and the same 0.00% in every run. So the product presents
real frames and every one of them is black. That is evidence that the loop works and the title does
not reach a picture; it is not evidence of a picture, and it is certainly not gameplay.

**A diagnostic defect this run exposed, in the driver, recorded because it is the workspace's
signature failure in miniature.** The driver's first version reported "the product is still running"
while the product was in fact dying of `SIGSEGV`, because `Popen.poll()` returning `None` means "not
reaped yet" and not "running". That is a confident answer about the one thing the reader most needs to
trust. The wording now states which of the two was observed and names the dropped connection, and it
reaps before describing.

## Falsifiers for the two claims above

1. **"The `CdRead` route publishes no image identity and does not invalidate."** Refuted by any
   reachable `imageCatalog().activate` or `notifyExecutableWrite` inside the `CdRead` call chain
   (`cd_read_stock_sync`, `cd_readsync_stock_sync`, or whatever they are renamed). Falsified today by
   the grep above, whose denominator is the whole file: 2 of 2 sites in `cd_loadfile`.
2. **"Publishing the identity does not reach gameplay."** Refuted by a run that publishes the module
   identity and then presents a non-black frame with the guest accepting input. The scratch
   experiment did the publish and got a different fault, so the claim stands for what was measured —
   but it is a claim about *this* frontier, not about the framework change being insufficient in
   general.

## Not established

- **Whether the invalidation gap has bitten yet.** The measured reads do not prove a stale
  translation was executed. Proving it needs a differential run against the Beetle reference over
  the frames after boot; that leg does not exist for Tomba! 1 yet.
- **What fills the ring at `0x800D7188`.** The producer is NOT identified. The retry loop is a
  *consumer*; naming the consumer is not naming the producer, and the obvious next move — "something
  should enqueue here" — is a guess until the writer is found in the image. This needs Ghidra or a
  store-observer census over the resident image, and the guest-reached module's own code, neither of
  which is done.
- **Whether `0x800E7388`'s module has its own nested modules.** Only one guest module load was
  observed before the fault; how many the title reaches in total is unknown.
- **Whether the residency owner belongs in the framework or in the title.** I believe the framework,
  because "a CD read that lands in executable RAM" is a framework-level fact and because a
  title-side copy would be a second owner of the invalidation rule. That is a judgement, not a
  measurement, and the framework owner may reasonably decide otherwise.

## Next step, precisely

1. **Framework (`psxport`):** make the `CdRead` route report an executable write, and give titles a
   way to publish an image identity for a guest-streamed module. The question a title must be able
   to answer is "this load is code", and the honest discriminator available today is *the guest
   executed inside the range* — which cannot be asked before the range is published, so the seam
   needs a title-visible residency notification, not a rule the framework guesses.
2. **This title, once that exists:** recover the guest's own module table so the code/data decision
   comes from the image rather than from an address list.
3. **Then:** the `0x8001EFE8` ring producer, by store-observer census over the module that is
   resident at that moment.
