# 0053 — area 9's left margin is black

Features: F-WIDE.

Status: closed (2026-11-04). Areas 9 and 12: the rooms' own cutaway, no defect. Areas 10 and 15: A0A's and A0F's
unlit pairs culled at the 4:3 edge, fixed by `UnlitModelEmitter` kA0a/kA0f.

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
- Areas 10 and 15 were a cull, not the camera. A0A's pair 0x8010F97C/0x8010FD74 and A0F's 0x80114A5C/0x80114E70
  keep a polygon only when one of its vertices has an unsigned x below 0x140 (`scratch/tomba2e/decomp/a0f/c/80114A5C.c`),
  so a polygon wholly in a margin was dropped and the surface ended on a jagged line of whole polygons. Both pairs
  are now `unlit_model_emitter.cpp:UnlitModelEmitter` kA0a/kA0f, which cull against
  `horizontal_cull::forDrawWindow` (commit d74ef94). Same arrival camera, before (`scratch/tomba2d/walk`, the guest
  pairs) and after (`scratch/tomba2e/w3`, HEAD): area 10's rock now reaches both edges and area 15's veil fills the
  left margin (`scratch/tomba2e/w3/reduced/before_after_10_15.jpg`). The emit-time window is the widened one in play
  (`[-54, 374)` logged at every guest call in area 13). Two frames after a `state load` of the same
  arrival the margins show the 4:3 cut again (`w3/reduced/walk_vs_load_a15.jpg`, not traced), so loaded states are
  not used for margin judgements.
- Areas still drawn by unported guest emitters (0052: 3, 4, 11, 13) keep that 4:3 cull until they are owned.
