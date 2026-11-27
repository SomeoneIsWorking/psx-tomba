# 0078 — Culling too aggressive at the wide margins

Status: fixed (2026-11-25, 6c0bb6a4), checked 2026-11-27. Owner: `GroundFootprint` (`game/render/ground_footprint.h`).

## Observed
User reports 2026-11-20, 16:9:
- "Culling is still too agressive" ("This is a minor example but there are bigger cases I couldn't capture"),
  A00 village by the net wall, marked x 0–6 %, y 34–56 %: a sea-coloured wedge in the ground at the left margin
  (user, 2026-11-27: "still here").
- "Another example to aggressive culling", A01 cliff path, marked x 90–100 %: the cliff cut off and the far hut
  drawn as a fragment at the right edge.

## Cause
Both are ground cells outside the 4:3 view triangle that each area's grid gather (A00 FUN_8013F4DC and its copies)
was handed. 6c0bb6a4 opens that triangle to the draw window's width.

## Proof
Each report's state.bin, `ires=0`, 3840x2160 sink, fps60 on and off, presents 2-17
(`scratch/cull/before_after.png`). After a state load, presents 2-3 are the saving run's own records drawn again
(`RecordRasterizer::redrawRestored`), so they show the 11-20 build's picture. They have the wedge and the cut
cliff. From present 4, HEAD draws the frame: the village ground is whole at the left margin, and the cliff and
hut draw whole at the right. A shot taken at pad frame 1 after a load shows the saving build's records, not HEAD's
draw.

## Next
The "bigger cases" need a capture: a bug report taken on the frame where the pop shows.
