# 0075 — The village ground shows the sea through slivers in its mesh

Status: fixed (2026-11-26). Owner: `game/render/overlay_ground_gt3gt4.cpp` (`OverlayGroundGt3Gt4::drawSliver`,
A00 FUN_8013FB88/FUN_8013FE58).

## Observed
User report 2026-11-20 ("Tiny seams"): thin cyan lines cross the grass of area 0 above native scale, along the
crests. The report's state at 8x shows 2162 such pixels over presents 2-59; at 1x it shows none.

## Cause
At a crest the ground mesh has thin triangles seen nearly edge-on, such as one between the cell above the crest
and the T-junction of the cells below it. The leaves' backface gate is NCLIP on whole-pixel SXY. For such a
triangle it can read the wrong sign (whole pixels -24, places +24.4 at the report state), so the guest drops it.
At 1x the neighbours' whole pixels cover the gap. Above native scale they sit at their places, and the dropped
triangle's area shows the backdrop sprites behind the ground.

## Fix
A record that the whole-pixel NCLIP culls but whose places face the camera is rerun through the same leaf body
over host memory. Its backface gate then reads the places. The packet is drawn first in its OT slot as a host
packet, so guest RAM and the GTE keep what the guest's cull left. At 1x the packet draws its whole-pixel triangle
over pixels its neighbours already cover.

Report state, ires 8x, fps60 on, presents 2-59: 2162 cyan pixels before, 291 after. The rest are the pond rim's
cyan wedge and the fish bowl's glass, unchanged by this fix. Cost on the same state is +2.1 Minsn per logic frame
(176.2 to 178.3, mostly the OT walk's host-packet lookup) and +4 draws per present.
