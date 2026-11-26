# 0077 — Frame drops

Status: open (2026-11-26), CPU cost reduced; not reproduced as a missed present on this host. Owner: psxport
presentation cost (`psxport/docs/presentation.md` Cost and cadence).

## Observed
User report 2026-11-21 ("frame drops?"), bug report `20261121-104641-frame-drops`, present frame 4352.

## Measured
From the report's state, unpaced, `ires=0`, sink 3840x2160, 300 logic frames:

- Cadence is 0.50 tick per vblank throughout. The guest never overruns its two fields.
- A hidden paced run on this host under load 12 missed no present.
- Before psxport 4f0ccf40: 93.3 Minsn per logic frame (game-logic 18.1, present 29.9, replay 14.3, other 29.9).
  Device rasterization was 22% of instructions. The logging GPU's CLUT-cache fill settled the palette rows the
  game cycles each frame (1008, 228/250/251), and each settle replayed every pending draw.
- After: 76.5 Minsn per logic frame (other 11.0, present 31.5); device rasterization is 2%. The pictures are
  byte-identical to `PSXPORT_GPU_EAGER=1` over two runs.

## Left
- The record planner (`RecordPlanner::Builder::addTriangle`, `WrittenMap::mark`) is about 20% of instructions.
- The drop has not been seen as a missed present here. A weaker host (the Ally) may still miss presents at
  76 Minsn; check pacing there when the user asks for a pacing run.
