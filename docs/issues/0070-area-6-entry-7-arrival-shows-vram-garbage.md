# 0070 — Area 6 entry 7 arrival shows VRAM garbage, then black

Features: S001.

Status: open. Owner: not yet traced.

## Reproduction
`warp 6/7` from any field, unpaced, hidden (`scratch/s023b/warp67`); the boss-clear route from area 10
(`warp 10/0`, `leave 6 7 8`, `scratch/s023b/leave8b`) arrives the same way.

## Observed vs expected
The arrival's first frames show scattered texture-page blocks on black at the left of the screen, then the screen
stays black (`scratch/s023b/a67_sheet.png`, top row). Expected: area 6 at entry 7, as at its other entries.
