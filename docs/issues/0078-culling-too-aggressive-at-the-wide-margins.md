# 0078 — Culling too aggressive at the wide margins

Status: open, waiting for a capture of the larger cases. Owner: `GroundFootprint` (`game/render/ground_footprint.h`)
and the object cone cull.

## Observed
User reports 2026-11-20, 16:9:
- "Culling is still too agressive" ("This is a minor example but there are bigger cases I couldn't capture"),
  A00 village by the net wall, marked x 0–6 %, y 34–56 %.
- "Another example to aggressive culling", A01 cliff path, marked x 90–100 %: the far hut and its palm cut off at
  the right edge.

## State on HEAD (2026-11-27)
Each report's state.bin, shot at pad frame 1, `ires=0`, 3840x2160 sink:
- The right-edge report draws the far hut and palm whole. It was fixed by 6c0bb6a4, which opens every area's ground
  footprint to the wide draw window.
- The left-edge report's frame shows the same picture as at report time. Its only gap is a flat sea wedge between
  two ground areas. The wedge stays the same with the ground footprint opened 3x and at a 21:9 canvas, so it is the
  scene's own geometry, not a cull. Nothing else in the marked region differs from the wider canvas.

## Next
The "bigger cases" need a capture: a bug report taken on the frame where the pop shows.
