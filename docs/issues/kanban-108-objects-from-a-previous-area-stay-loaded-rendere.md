---
id: kanban-108
title: Objects from a previous area stay loaded/rendered over the ocean after its terrain unloads
status: open
labels: [render, gameplay, bug]
---


USER 2026-08-20, live windowed run. Standing at the seaside cliff edge, a dozen objects from the STARTING area — treasure chests, eggs, a door, a signpost, a bird, branches — hang in mid-air over the ocean. The area's own terrain is unloaded and not rendered; only its OBJECTS remain. Evidence: scratch/screenshots/live/stale-objects-over-ocean.png.

NOT COSMETIC. USER: 'this actually causes real gameplay issues'. The objects are not merely drawn — they are still SIMULATING, so they are still collidable/interactable at a position the player can reach.

USER'S READ, and the code agrees with it: 'the object load/unload is overriden to adapt to widescreen which is good but I think it is overdone, maybe this is also one of the sources of the lag'.

THE NAMED LEVER, unverified as the cause but it is exactly the shape described — game/render/cull.cpp:

    #define CULL_FAR_MULT 4   // x4 the stock per-state far limits (4097..7169 -> ~16388..28676)

It multiplies EVERY per-state far-distance limit in cull_decide() by four. The file's own comment says what that buys and what it costs: 'Because cull_decide() drives the visible flag (obj+1) AND the render queues that the per-frame object walk consults, widening these limits keeps far objects SIMULATING, not just drawing' — which is precisely the reported gameplay symptom — and it carries a standing RISK note about the ~52-node active pool (free count @0x800E7E7C) overflowing if the kept working set grows too far.

WHY 'OVERDONE' IS THE RIGHT WORD. Widescreen needs a wider HORIZONTAL FOV. It does not need FOUR TIMES the draw/simulate DISTANCE. The same file already lands the actual widescreen fix separately — the FOV-cone re-include, which exists because the stock ±34 degree cone is narrower than even the 4:3 frustum. The ×4 distance multiplier came in for GitHub issue #22 ('culling too aggressive, geometry vanishes too near the camera'), a DIFFERENT complaint, and the two got solved with one over-broad knob.

THE EXPERIMENT THAT SETTLES IT, and it needs no code change — the A/B knob already exists:
    PSXPORT_CULL_FAR_MULT=1   (stock far limits, FOV re-include untouched)
Run the same spot with 1 vs 4. If the floating objects vanish at 1 and return at 4, CULL_FAR_MULT is the cause and the fix is to separate the two concerns: keep the FOV-cone widening (that IS the widescreen fix) and either drop the distance multiplier or scale it far less. FALSIFIER: if the objects are still there at MULT=1, the cull far limit is NOT the cause and the leak is in the area load/unload path instead — look at what tears down an area's object list on transition, because the terrain clearly went away while the objects did not.

ALSO WORTH MEASURING WHILE THERE: the user's lag suspicion. A ×4 far limit means a much larger kept set every frame, and the kept set drives both the render queues and the per-frame simulate walk. Count the kept objects per frame at MULT=1 vs MULT=4 — with a denominator, i.e. kept of inspected — before crediting or clearing it as a perf cause.
