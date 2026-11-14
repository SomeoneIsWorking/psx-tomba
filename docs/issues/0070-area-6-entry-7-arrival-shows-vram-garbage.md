# 0070 — Area 6 entry 7 arrival shows VRAM garbage, then black

Features: S001.

Status: closed. Not a product defect: the reproduction's story state is one the game never reaches.

## Reproduction
`warp 6/7` from any field, unpaced, hidden (`scratch/i0070/sheet1.png`); the boss-clear route from area 10
(`warp 10/0`, `leave 6 7 8`) arrives the same way.

## Observed vs expected
The arrival's first frames show scattered texture-page blocks on black at the left of the screen, then the screen
stays black. Expected: area 6 at entry 7, as at its other entries.

## Cause
Area 6 has two variants, picked by bit 6 of `0x800BFE56`, which only area 12's clear sets
(tomba2-decomp `docs/notes/scene-transitions.md`, Area variants). Entry 7 is area 10's clear destination; its
arrival event 9 draws class 12 item 0x63, which only the bit-6 variant's class-12 pack holds. The warp keeps the
boot state's `0x800BFE56` = 0, so the plain variant loads, the event actor reads past its 0x61-item pack and the
unlit emitter (`0x8013D1E4`) faithfully draws the offset table as GT4s, whose code words decode as VRAM copies.

## Check
`w16 800BFE56 40` before `warp 6/7` loads the variant (class 12 = `0x801C9368`) and the arrival draws area 6
with Tomba and no garbage (`scratch/i0070/b6/sheet.png`, frames +10 to +200).
