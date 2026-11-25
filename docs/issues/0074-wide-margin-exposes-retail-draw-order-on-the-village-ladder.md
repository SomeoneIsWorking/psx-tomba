# 0074 — The wide margin shows retail's draw order hiding the village ladder

Status: fixed (2026-11-25). Owner: `game/render/overlay_ground_gt3gt4.cpp` (`OverlayGroundGt3Gt4::markDepth`, A00 FUN_801401B8).

## Observed
User report 2026-11-20 ("Ladder got cropped", x 81-97%, y 48-87%): the ladder leaning on the hut right of the
fire tower in area 0 loses the middle of its right rail behind the hut's front wall; its feet show again in front.

## Cause
Ladder and hut are both A00 ground cells drawn by `OverlayGroundGt3Gt4`. The override differential on 0x801401B8 at
4:3 (`PSXPORT_OVERRIDE_DIFF=0x801401B8`, the report's state) matches the guest body in every RAM and scratchpad
byte: the buckets are retail's. The wall's quads sort nearer than the rail's middle quads, a painter's-sort error
retail keeps off screen; the 16:9 margin shows that part of the ladder.

## Fix
The A00 ground cells test depth per pixel (`DepthOrder::Polygon`, bin 8 SZ: one sort slot at the village's depths).
A smaller bin lets near-coplanar cells fight (a notch where the fire tower's post meets the hut canopy).
Shown on the report state at 16:9 with `depth` on: the rail draws whole over the wall; the cull and pig report
states differ from slot order only in edge pixels.
