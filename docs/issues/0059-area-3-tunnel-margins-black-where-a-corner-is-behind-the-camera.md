# 0059 — area 3 tunnel margins are black where a ground record has a GTE error

Features: F-WIDE.

Status: open (2026-11-11). A fix exists as `git stash` "0059 margin pieces (seam at the 4:3 edge)"; it fills the
margins but leaves a seam, so it is not landed.

## Reproduction

21:9 (`aspect=2`), `fps60=0`, `ires=0`, sink 3840x2160; pause at boot, step to frame 1050, `warp 3/0`, shots at
frames 1250, 1330, 1410 (`scratch/a03margin/plan2.py`). Frame 1250 (the cart start, "Go!"): the right margin from
the tunnel wall to the bottom is black (`scratch/a03margin/marg_f0r.png`, left panel). Frame 1330 shows a smaller
black strip at the right margin's inner edge.

## Cause

`game/render/subdivided_ground.cpp:emitRecord` (0x80110290) links each GT4 the guest projected. A record with a
corner behind or far beside the camera sets the GTE flag error bit; its projected corners are wrong, and the GPU
skips a triangle whose edge spans 1024 columns or 512 rows. At 4:3 the guest's cull leaves those pieces off
screen, so retail shows nothing wrong; in the margins they are missing.

## The stashed fix and why it is not landed

For a record whose flag has the error bit it cuts the guest's packets to columns [0, 320) and draws each margin's
part from the record's model corners clipped in view space (near plane H/2, the margin's edges) and projected
again (`marginPieces`); exact records wholly in a margin that the GPU would skip are split on screen until they draw.
At frame 1250 the black right margin becomes tunnel wall (`marg_f0r.png`, right panel), but the left margin shows
a texture seam exactly at the 4:3 edge (`marg_f0l.png`, right panel): the margin pieces map the texture across
different triangles than the guest's packets inside, so the wall does not continue across column 0.

The proper fix draws an erroring record's inner and margin parts from the same clipped polygon, so the texture
mapping is continuous at the edge; that changes the inner 320 columns of those records from the guest's output,
which has to be judged against a 4:3 run.
