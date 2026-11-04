# 0053 — area 9's left margin is black

Features: F-WIDE.

Status: open (2026-11-04). Owner: not traced.

## Reproduction
`warp 9/0` (`Large Mine Underground: ...???`) from free roam, `ires=0`, `PSXPORT_PRESENT_SINK=3840x2160`, 16:9.

## Observed vs expected
The cave room fills the 4:3 middle and the right margin; the left margin beyond the room's left wall is black
(`scratch/tomba2d/walk/reduced/a09.jpg`). Expected: the 16:9 margins show the scene, or the guest's own cutaway if the
room is drawn that way at 4:3 too.

## Cause
Not traced. Area 9's overlay A09 is a single cave room; whether its model ends at the left wall (a cutaway, like the
area 0 tower room in issue 0048) or a 4:3 cull drops the wall's far side is not yet shown. Next: a 4:3 capture of the
same frame (`aspect=0`) and `census` of the left margin's writers.
