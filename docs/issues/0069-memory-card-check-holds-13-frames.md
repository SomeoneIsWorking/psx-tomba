# 0069 — The memory-card pages hold "Checking MEMORY CARD..." for 13 frames

Features: S023.

Status: open, 13 frames down to 4. Owner: CRD's card browser (`FUN_8007BE18` → CRD `0x8018FBCC`), its check page
`game/scene/card_check.cpp:CardCheck::step` (CRD `0x8018DC2C`), over psxport's card HLE.

## Reproduction
Game over through `spot 1 0 24 8576 -2600 14400`, Cross to end GAME OVER, then Save? Yes (or No, then Load data),
unpaced, hidden (`scratch/s023b/plans/card.py`, runs `card5`, `card6`).

## Observed vs expected
From the press the menu holds 3 frames while CRD loads (card machine `0x800BF84A` case 0), 2 frames are black, then
"Checking MEMORY CARD..." shows for 13 frames before the slot browser takes input (`card5_sheet.png`,
`card6_sheet.png`). Card reads complete at once, so the check presents a wait with nothing to wait for. Expected:
the browser on the first frame after CRD's load.

## Known
CRD `0x8018FBF0` calls `0x8018A1E8` per port record: phase 0 calls `_card_info` (A0:0xAB) for the port, phase 1
polls four card events (`0x800BF498` + 0x1C..0x28) through `FUN_80080840`, counting `+0x43` up to 0xFF before a
timeout. psxport completes `_card_info` and delivers the I/O-end event at once, and the guest keeps calling it
every few frames, alternating ports 0 and 0x10, for as long as the browser is up.

## Cause
The check page CRD `0x8018DC2C` counts its byte +0x50 to 11 before it reads the two port results (+0x4A, +0x4B);
with an instant card the results are in long before (tomba2-decomp `docs/notes/scene-transitions.md`).

## Fix
`CardCheck::step` owns the page and goes from step 1 to the wait on the results at once; the pad answers and the
step to the slot browser are the retail page's.

## Shown
Save page, unpaced, hidden, 16:9 (`scratch/wsgap/cardafter`, `plans/card.py`, presses 100:cross, 140:up,
160:cross): menu +160..+163, black +164..+165, "Checking MEMORY CARD..." +166..+169, the slot browser from +170
(browser state 1 at +167), against the browser at +179 before (`scratch/s023b/card5`)
(`scratch/wsgap/card_after_sheet.png`).

## Left
4 frames of "Checking MEMORY CARD...": `0x8018A1E8` checks one port per call, `_card_info` then an event poll on
the next frame, so both ports settle on the fourth frame.
