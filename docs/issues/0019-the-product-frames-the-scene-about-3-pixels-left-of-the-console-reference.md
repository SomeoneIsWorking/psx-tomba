---
id: 19
title: The product frames the scene about 3 pixels left of the console reference
status: open
symptom: at a settled free-roam frame where every declared guest range agrees byte for byte, shifting the product by dx=-3 dy=+1 drops the picture difference from 86.98% to 73.81%. The camera ranges agree in RAM, so the offset is introduced downstream of the state
state_items: S004
tags: render,camera,oracle,picture
created: 2026-09-19
---

## Cause location: downstream of the camera OBJECT

The camera object is byte-identical on both cores at free roam and at every sampled frame along the
route (all 144 bytes). So the input state is the same and the two cores compute different pictures.

The composed camera — the view basis and the look position — is assembled into the SCRATCHPAD
(S+0, S+6, S+8; world readout 0x1F8000D2/D6/DA), and the two cores compute it by different code:
the product runs the native `CutsceneCamera`, the reference runs guest execution. Identical inputs,
two implementations, so a differing output is the expected failure. The per-frame offset is erratic
(+11, 0, -3, +1 px at 60/120/180/240 frames), which is what a small arithmetic divergence in a
smoothing/accumulator pipeline looks like rather than a fixed display origin (constant) or camera
drift (monotone).

Consequences:

- The console reference cannot read scratchpad, so no declared RAM range will ever catch this. The
  comparison must be made product-side, against guest execution.
- The offset is not the whole story: after the best shift, about 65% of the picture still differs. The
  remainder is the shading question in issue 0018.
- Every whole-frame percentage in 0018 is contaminated by a per-frame framing difference of up to
  11 px.
