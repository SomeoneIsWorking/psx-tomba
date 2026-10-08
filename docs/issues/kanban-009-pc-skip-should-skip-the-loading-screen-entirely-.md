---
id: kanban-009
title: Remove pure loading-only screens
status: partial
labels: [loading, enhancement]
---

# Finding (2026-10-03): the loading card and its wait are the SAME code, and the wait is
# already gone from every native route.

## The wait loop, from MAIN.EXE (0x80044BD4)

```
0x80044C40  sb   r16,0x6d(r2)      ; sm[0x6d] = mode   (arg3)
0x80044C44  sb   r0,0x19b(r17)      ; 0x1F80019B = 0
0x80044C48  jal  0x80051F14         ; FUN_80051F14 — arm slot 1 with the loader
...
0x80044C8C  lh   r2,0x198(r0)       ; WAIT LOOP
0x80044C94  addiu r2,r2,1
0x80044C98  jal  0x8007FD54         ; draw the blinking "Loading....." card
0x80044C9C  sh   r2,0x198(r16)      ; (delay slot) bump the counter
0x80044CA0  jal  0x80051F80         ; FUN_80051F80 — YIELD: costs ONE display field
0x80044CA4  addiu r4,r0,1
0x80044CA8  lbu  r2,0x19b(r17)      ; 0x1F80019B — load-done flag
0x80044CB0  beq  r2,r0,0x80044C8C   ; loop
```

`0x80044C98` is the ONLY `jal 0x8007FD54` in the whole executable — verified by scanning every
instruction word of MAIN.EXE and all 30 provisioned overlays for direct `jal` targets. So there is no
separate "hide the card" change: the card is what the wait presents, once per display field. Deleting
the drawer alone would have hidden the symptom and left the wait.

`FUN_80044BD4`'s guest call sites are real and common — 22 `jal` sites. NOTE: an earlier pass at this
count read the `.BIN` overlays at file offset 0x800 as if they were PS-X EXEs, which yields addresses
0x4000 too high; overlays load at 0x80100000 with file offset 0. The corrected list:
DEMO.BIN `0x801001D4`/`0x80100A98`, SOP.BIN `0x80100524`, START.BIN `0x80100578`/`0x8010059C`,
GAME.BIN `0x80101984`/`0x80101B88`/`0x80101C8C`/`0x80101DA0`/`0x80101E58`/`0x8010270C`,
A00 `0x80101DA0`/`0x80103CE4`, A01 `0x80101E4C`, A04 `0x80108C18`/`0x80109934`,
A05 `0x80106244`/`0x8010827C`/`0x801082A8`, A06 `0x8010BA44`, A08 `0x8010A9D0`/`0x8010BC64`.

## What is already synchronous

Every load site the native product reaches was already ported to complete the spawned task inside the
caller's own display field:

| load | native owner | wait | presented during it |
|---|---|---|---|
| boot preloads + START.BIN file table | `StartBinStage` | none (synchronous; logs "completed synchronously") | the logo FMV |
| attract item launch (`Demo` s7 phase0) | `Demo` phase0 native load + `PcScheduler::completeSyncWait(flag=2)` | none | the attract world fading in (f463+) |
| GAME prologue → first area | `Engine::submode1Case0Native` → `Sop::transitionAreaLoad` | none (1 display field, f1426) | the authored transition |
| in-field / door area transition | `FieldTransition::areaLoadBd4` → `Sop::transitionAreaLoad` | none (1 display field) | the authored transition |
| cold warp | `tomba::applyColdWarp` → `Sop::transitionAreaLoad` | none (1 display field) | the destination area |

Measured on the headless route (blank memory card, no input, then `newgame`): boot→logos→title at
f330..f456, attract launch at f460 and f1818, prologue at f445, area loads logged at f~800 and
f1426. Across 5000 frames `FUN_80044BD4`'s loop never ran, and the loading card never appeared in any
captured field — the card box (x 95..225, y 170..190 of 320x240) reads max luma 0 for every frame of
the attract launch window f457..f461, where the screen presents nothing at all.

## The change that landed

`game/ui/loading_text.cpp` / `.h` are deleted, and the caller-less `Engine::submode1Faithful` — the
last writer of the guest multi-tick cadence — is removed. Nothing else: the native owner that WAS
tried for `FUN_80044BD4` itself (`PcScheduler::spawnAndWait` declared as the override) was reverted.

**Why no override.** A/B with it installed and commented out, over two 2000-frame runs and a 10-area
cold-warp run, was byte-identical apart from the declaration counters (`declarations=251 installed=197`
vs `252/198`): a clean no-regression result AND a no-effect result. An override no route reaches is not
landed — least of all this one, whose body carries `SynchronousTaskWait`'s abort when a loader does not
raise `0x1F80019B` synchronously. See the section below for why no route can reach it.

## Still open

The card and its wait are gone for the same reason they were never seen: every load the product
performs is already synchronous at its own native owner, and the one wait that could present a card has
no reachable caller.

## Who the 22 call sites belong to, and why none of them is reachable (2026-10-03, Ghidra/disas)

(This pass predates the manifest fix below: the overlays were not declared and MAIN.EXE's `text_size`
was wrong, so `decomp_pipeline.py` refused both images and the per-image reading came from
`tools/disas.py`. Ghidra now answers for both — see the manifest section.)

Two families:

**Area and transition loads — the ones a loading screen could belong to.**

* `GAME.BIN 0x801026B8` is a jump-table dispatcher on `sm[0x4c]` (guard `sltiu v0,v1,7`, table at
  0x80106334). Its case 0 is `jal 0x8005245C` then
  `FUN_80044BD4(a0=0x800452C0, a1=0x800BF870, a2=0, a3=2)`. That is the guest image of
  `Engine::submode1` case 0 — the walkable-field area machine. The port runs
  `Engine::submode1Case0Native`, whose `Sop::transitionAreaLoad` does the same work synchronously.
* `A08.BIN 0x8010BC64` is the transition bonus-area load: it computes the same bracket
  (`a1` = 34/38/40/36 selected by `sm` value, the same ladder as `Sop::transitionAreaLoad`) and
  stores `0x800BFE60` before calling with flag 3. The port's `FieldTransition::areaLoadBd4` owns the
  caller.

**Per-actor behaviour loads.** `A00 0x80101DA0` → `FUN_80044BD4(0x8011C740, 28, 0, 3)`,
`A01 0x80101E4C` → `(0x8011BAA4, 24, 0, 3)`, and A04/A05/A06 the same shape with flag 3 and a small
integer argument. `0x8011C740` is present byte-identically in every MODE overlay and reads a per-node
record (`lw v0,0xc4(v1)`) before calling `0x80083E80` — a per-object streamer, not a load screen. It
streams one actor, and the guest never put a screen up for it.

**Reachability: not reached.** Every one of the 22 sites is the guest image of code the port has
already replaced with a synchronous native owner. They execute only when `Engine::frame()` hands the
frame back to the cooperative guest loop, which is exactly the unowned condition it reports:
`sm[0x4a] ∉ {0,1}` (plus the SOP-not-loaded case at `s4a==0`). Nothing driven reaches it — boot,
the attract cycle, `newgame`, the prologue, the first area, the in-field transition and a cold warp
all keep `s4a` at 0 or 1 (`applyColdWarp` writes `s4a=1` explicitly). This is a native-ownership
boundary, not an input problem, so a cold warp or a phase-keyed pad replay cannot reach it either.

Consequence for the change: the override is correct by construction and harmless (A/B byte-identical
over 5000 frames and 10 cold warps), but it cannot be shown taking a call, so it stays unlanded
pending a decision.

## Ghidra now works for Tomba! 2 (2026-10-03)

`external/psxport/tools/decomp/manifest.json`:

* the `tomba2` resident entry's `text_size` was `0x28800` against MAIN.EXE's real `0xAE800`, so
  `cross_check_resident` refused the image outright;
* the overlays are now declared as `module` entries — **28 of them**, not 15: A00 through A0L (22 MODE
  images) plus CRD, DEMO, GAME, OPN, SOP and START. Each carries `load_base` 0x80100000, `code_first`
  measured with the framework's own `is_entry_prologue`, `code_last` as the file's last byte, a sha1
  gate, and a note saying where the numbers came from. The words before `code_first` in each overlay
  are a function-pointer table (A00 offset 0 holds `0x0000000A` then `0x8010A3AC`, `0x8010A418`, …),
  which is why seeding at offset 0 would reach nothing.

Both image kinds then answer:

```
--image-name tomba2      --refs 0x800499E8 --function-at 0x800499E8
  refs 0x800499E8: 3 reference(s) from 2 function(s)
    0x80045130 DATA   800450BC FUN_800450bc  _sw v0,0x0(s1)
    0x80050BF4 PARAM  80050B08 FUN_80050b08  _addiu a1,a1,-0x6618
    0x800A3ED8 DATA   ?       -             <no instruction>
  function_at 0x800499E8: entry FUN_800499e8, body 0x800499E8..0x80049A5F, 30 instruction(s)

--image-name tomba2_game --target 0x801026B0 --function-at 0x801026B0
  0x801026B0  FUN_801026b0  found=True decompiled=True body=True insns=20  [ret=2 prologue=1 jal=7]
  AUDIT OK
```

and the decompiled body independently confirms the hand reading in the section above:

```c
void FUN_801026b0(void) {
  if (*(ushort *)(_DAT_1f800138 + 0x4c) < 7) {          // sm[0x4c], cases 0..6
    (**(code **)((uint)*(ushort *)(_DAT_1f800138 + 0x4c) * 4 + -0x7FEF9CCC))();  // table at 0x80106334
    return;
  }
  return;
}
```

The address the table base resolves to is the same 0x80106334 the instruction read gives, and case 0 of
that table is the `FUN_80044BD4(0x800452C0, area, 0, 2)` site.

One measured limitation, stated rather than glossed: Ghidra's cross-reference scope here is what
auto-analysis reaches from the seed. For a resident the seed is the header entry (0x800896E0), so
`FUN_80044BD4` at 0x80044BD4 — reachable only through the cooperative dispatch, not statically from
crt0 — has no function defined and 0 references; `--function-at` on it says so explicitly rather than
guessing. That is the analyzer's scope, and it is exactly why the reachability conclusion above rests on
the native-ownership argument and not on a reference count.
