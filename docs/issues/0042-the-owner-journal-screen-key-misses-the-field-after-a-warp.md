# 0042: the owner journal's screen key misses the field after a warp

Status: open. Owner: `game/core/debug/debug_menu.cpp` (`screen`).

## Observed
Tomba 2 through the title route and `warp 1` (2026-10-27): the journal's rows after the warp carry a screen key with
the field bit 0 and an empty route, although the field is running and area 1 composes.

## Next
Read `DevWarp::phase` at the frame the journal observes the screen; the key may be sampled while the warp's own
transition still reports `Unavailable`, and a key that never changes again keeps that label.
