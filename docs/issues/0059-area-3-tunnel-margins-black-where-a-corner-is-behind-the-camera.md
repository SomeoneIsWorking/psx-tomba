# 0059 — area 3 tunnel margins are black where a ground record has a GTE error

Features: F-WIDE.

Status: fixed (2026-11-11).

## Reproduction

21:9 (`aspect=2`), `fps60=0`, `ires=0`, sink 3840x2160; sync, pause, step to frame 1050, `warp 3/0`, shots at
frames 1250, 1330, 1410 (`scratch/m0059/shots.py`; `frames.txt` in each run shows the frames reached). Frame 1250
(the cart start, "Go!"): the right margin below the tunnel wall and the bottom-left corner are black
(`scratch/m0059/cmp_f200.png`, top).

## Cause

`game/render/subdivided_ground.cpp:emitRecord` (0x80110290) links each GT4 the guest projected. A record with a
corner behind or far beside the camera sets the GTE flag error bit; its projected corners are wrong, and the GPU
skips a triangle whose edge spans 1024 columns or 512 rows. At 4:3 the guest's cull leaves those pieces off
screen, so retail shows nothing wrong; in the margins they are missing.

## Fix

When the draw window is wider than 4:3, `emitRecord` sends a record whose flag has the error bit through
`linkErrored`: its model corners are placed in view space (`game/render/view_clip.cpp:ViewClip::place`, RTPS per
corner), clipped at depth H/2 and to the whole draw window (`ViewClip::visible`), and the clipped polygon is drawn as
a fan with every vertex projected from the exact view point, so the texture runs continuously across the 4:3 edge.
A culled erroring record draws only its margin parts (`linkMargins`), and an erroring leaf of the near subdivision
draws its margin parts as well. Each call has a budget of 256 clipped packets; past it the guest's packet is linked, and the run-end line
`[ground] run-end: N A03 ground record(s) in M call(s) drawn as the guest drew them` counts those records
(`GroundClipTally`, held by the session). A clip keeps a corner whose own projection has no error where the record's
path links it: the split paths on its word, an edge between two such corners bent at the guest's halfway word
(0x8010F968), the averaged path and the subdivided leaves at the GTE's place. A neighbour that projected without error
shares those vertices, so the shared edge is one line.
At 4:3 nothing takes these paths and the output is the guest's.

## Evidence

The reproduction above. `scratch/m0059/cmp_f200.png`: HEAD above, black right margin below the wall and black bottom-left; the
fix below, tunnel wall and floor to both edges. `seams2_f200.png`: no seam at either 4:3 edge. `cmp_f280_f360.png`:
frames 1330 and 1410, HEAD left and fix right, show no black margin or seam in the fix. 4:3 runs `fix43` and `head43` are byte-identical at all three shots.

The crack at guest (315, 196) between a re-clipped record (0x801BDC40, 0x801BC0F4, 0x801BDB10) and an exact
neighbour came from the clip placing shared corners at their exact projection while the neighbour's split put them on
their words. `crack_zoom_f200.png` (run `fix4`, brightened 3x): left the old build, a dark line along the shared edge
and a dashed one under "mph"; right the fix, both closed. `fix4_all.png`: the three shots, margins filled. The run-end
line reads 0 records over the run.
