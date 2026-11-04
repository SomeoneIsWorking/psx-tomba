# 0053 — area 9's left margin is black

Features: F-WIDE.

Status: closed (2026-11-04), no defect: the black is beyond the rooms' own geometry, the guest's cutaway.

## Reproduction
`warp 9/0` (`Large Mine Underground: ...???`) from free roam, `ires=0`, `PSXPORT_PRESENT_SINK=3840x2160`, 16:9.

## Observed vs expected
The cave room fills the 4:3 middle and the right margin; the left margin beyond the room's left wall is black
(`scratch/tomba2d/walk/reduced/a09.jpg`). Expected: the 16:9 margins show the scene, or the guest's own cutaway if the
room is drawn that way at 4:3 too.

## Cause
None in the port. Each black region is outside the room model, and the 4:3 frame shows the same edge
(`scratch/tomba2e/margins/reduced/pairNN.jpg`, 16:9 above, `aspect=0` below):
- Area 9: the floor and wall end on one slanted line that runs from the left margin into the 4:3 frame, where it is
  the black wedge at the bottom left. A 4:3 cull would cut along the frame edge, not along a line that crosses it.
- Area 12: the room ends on its right; at 4:3 the same right part of the room is black.
- Areas 10 and 15: in this pass both margins show the scene (area 10's dark purple sky, area 15's veil on the left
  and the starfield grid past the pillar on the right); the earlier black was the walk's camera state.
