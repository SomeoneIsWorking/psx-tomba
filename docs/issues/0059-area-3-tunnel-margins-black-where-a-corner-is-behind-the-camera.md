# 0059 — area 3 tunnel margins are black where a ground record has a GTE error

Features: F-WIDE.

Status: fixed (2026-11-11). A hairline crack of about half a guest pixel remains where a re-clipped record meets
an exact neighbour (below).

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
draws its margin parts as well. Each call has a budget of 256 clipped packets; past it the guest's packet is linked.
At 4:3 nothing takes these paths and the output is the guest's.

## Evidence

The reproduction above. `scratch/m0059/cmp_f200.png`: HEAD above, black right margin below the wall and black bottom-left; the
fix below, tunnel wall and floor to both edges. `seams2_f200.png`: no seam at either 4:3 edge. `cmp_f280_f360.png`:
frames 1330 and 1410, HEAD left and fix right, show no black margin or seam in the fix. 4:3 runs `fix43` and `head43` are byte-identical at all three shots.

## Remaining

A crack about 0.6 guest pixels wide shows at guest (315, 196) where a re-clipped record (0x801BDC40, 0x801BC0F4,
0x801BDB10) meets an exact neighbour: the neighbour's edge is the guest's integer projection, ours is exact, so the
two edges are not the same line. The guest's own geometry has similar hairlines elsewhere in the tunnel.
