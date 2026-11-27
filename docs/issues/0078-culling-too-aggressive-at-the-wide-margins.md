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
- The left-edge report still shows a sea-coloured wedge in the ground at the left margin (user, 2026-11-27: "still
  here"). Traced on its state at pad frame 1. The ground there is A00's grid, gathered by FUN_8013F4DC and drawn by the
  entity loop FUN_801401B8. The wedge's quad, record 0x801AA40C in cell (8,10), is dropped by the leaf's backface gate.
  Both of its triangles wind backwards, so retail drops it too. Cell (8,11) next to it is empty in the ground map
  (0x800ECF78), between cells (4..7,11) and (9..16,11). None of these changes the wedge: the view triangle opened 3x,
  the per-cell visibility test FUN_8013FAE0 forced to pass, or the object cone cull disabled. No GTE-flag or
  screen-edge drop covers it either.

## Next
Find what would cover cell (8,10)'s screen area from in front. On a heightfield a back-facing cell is hidden by a
front face nearer the camera, so the cell in front is missing, or the hole at (8,11) is retail's own and only the margin
shows it. The "bigger cases" need a capture: a bug report taken on the frame where the pop shows.
