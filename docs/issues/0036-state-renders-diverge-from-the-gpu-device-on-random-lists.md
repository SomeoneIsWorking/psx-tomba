# 0036 — state renders diverge from the GPU device on random lists

Features: F-FPS60.

Status: open.

The emitter tests now take the guest's frame from the GPU device through DMA2 (`tests/state_render_check.h`
`submittedFrame`) and the render through `composeFrame`. The asset-gated tests then fail on inputs the device
normalises and the shared decoder (`psxport/runtime/psx/present/gp0_primitive_decode.cpp`) does not:

- `test_authentic_model_emitters`, MAIN.EXE `0x8007FDB0` case 0: a 15bpp polygon's `clutWord` is the command's
  raw attribute in the render (5115) and the device's last CLUT tag (32767); a texpage mode 3 polygon is mode 3
  in the render and mode 2 on the device.
- `test_authentic_writers`, packed mesh case 5: the render emits a polygon 645 lines tall that the device drops
  (over 511), so `composeFrame` finds no CLUT for it and keeps the guest's packets.

Run: `build/test_authentic_writers scratch/bin/tomba2/MAIN.EXE <sha256>`; the emitter test also takes each
overlay in `scratch/bin/overlays/` with its sha256, in the order of its `names`.

Fix at the owner: the decoder records what the device draws (mode 3 as 2, no `clutWord` without a CLUT, no
oversize polygon), with `psxport/tests/test_gp0_primitive_decode.cpp` cases against the device. Shared with
every title's renders, so it is a framework change.
