# 0045: the area 14 boss arena backdrop stops at the 4:3 edge

Features: F-WIDE.

Status: open; cause not traced.

## Reproduction

Hidden, unpaced, ires 0, fps60 1, sink 1920x1080, aspect 16:9: title route to free roam, then `warp 14`, a `shot`
25 s later (the Evil Water Pig's entrance dialog).

## Observed vs expected

The blue gradient sky behind the arena is drawn only over the 4:3 picture: both margins above the stone ledges and
below the floor are black, while the arches, the floor and the actors draw across the 16:9 width. Expected: the
backdrop fills the canvas as it does in every other area shot (areas 0-3, 6-11, 13).

## Proper fix

Trace the backdrop's draw in the area 14 overlay to its owner and widen it there (a margin producer or the
backdrop's own span), as 0028 did for the A0B backdrop.
