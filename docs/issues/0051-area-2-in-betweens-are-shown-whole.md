# 0051 — area 2 in-betweens are shown whole

Features: F-FPS60.

Status: fixed (2026-11-04). Owner: `game/render/unlit_model_emitter.cpp` (`UnlitModelEmitter`), reached from
`game/render/perobj_dispatch.cpp` (`Render::perModeDispatch`).

## Reproduction
`warp 2/0` from free roam, unpaced, `ires=0`, `fps60=1`, `PSXPORT_PRESENT_SINK=3840x2160`, debug `fps60dump`.

## Observed vs expected
Every in-between after the arrival repeats the later real present (shares 1.00 1.00 ... over 7 in-betweens,
`scratch/tomba2d/walk/reduced/shares.txt`). Expected: in-betweens near halfway, as areas 0, 8 and 12 show.
`owners`: `IntervalHeld/no-state producer=0x8003CDD8 ot=0:480-637 packet=0x800C1248 cmd=0x3C screen='warp 2/0'` on
every interval (`scratch/tomba2d/own/owners/*.jsonl`).

## Cause
Area 2's mode table sends `Render::perModeDispatch` to case 0x8003F708, whose target in A02 is the drawer
0x8012555C: `FUN_80124DB8(list + 0x10, ot, count & 0xFFFF)` (GT3) then `FUN_80125100(end, ot, count >> 16)` (GT4).
Both are guest code, so their GT3/GT4 packets carry only `cmdListDispatch`'s key, which has no state; the composer
refuses the interval (psxport `FrameComposer`, `HeldCause::NoState`). The store watch on 0x800C1248 shows all 64 stores
from that call (`ra=0x8003F710`, `scratch/tomba2d/watch/run.log`).

The pair is a copy of the unlit body none of `UnlitModelEmitter`'s variants describe
(`scratch/tomba2d/decomp/a02/c/80124DB8.c`, `80125100.c`):
- 0x24-byte GT3 records, the resident stage words 0x1F800080/84;
- colour words split into `c & 0xFFF0F0F0` and `(c & 0x0FFF0F0F) << 4`;
- depth from flag bits 0-1: 0 average, 1 farthest, 2 and 3 nearest;
- a compressed bucket `(z >> (z >> 10)) + (z >> 10) * 0x200`, dropped outside 4..0x7FF;
- a U scroll by flag: bit 4 adds `u16[0x8012C92C] >> 4`, else bit 3 `u16[0x8012C92A] >> 4`, else bit 2
  `u16[0x8012C928] >> 4`;
- a screen cull at x < 0x140, y < 0xF0 that does not go through `horizontal_visibility_cull.h`.

## Fix
`UnlitModelEmitter` declares the pair on A02 as `kA02`: the resident projection (corner 3 first, every colour code
byte kept), depth 0 average, 1 farthest, 2 and 3 nearest read straight from the SZ registers (`Staging::Registers`,
`ModelPacket::depthFromRegisters`, so nothing is staged on the stack the guest never touches), and the U scroll as a
table of flag bit and word, first set bit wins. The calls are saved as `ListJob`s under the command's object and the
cull goes through the draw window like the other variants.

## Shown
`warp 2/0`, 200 frames, 20 dumped frames, unpaced, `ires=0`, `fps60=1`, sink 3840x2160
(`scratch/tomba2e/a2/reduced/`): every presentseq row composed at `t=0.5`; shares 0.44-0.61 on every interval
where geometry moves, Tomba's idle steps through each in-between (`tomba_seq.png`). The four 1.00 intervals change
only a 14x14 HUD cell at (756, 487) in 960x540, which blinks. The owner journal's `IntervalHeld/no-state` rows for
0x8003CDD8 (260 intervals before) are gone. The margins hold the room's walls at this camera before and after; no
record here lay wholly outside the 4:3 window, so the widened cull is not separately visible.
