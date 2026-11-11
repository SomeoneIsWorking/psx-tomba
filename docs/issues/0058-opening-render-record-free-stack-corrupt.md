# 0058 — the opening aborts on a corrupt render-record free stack

Features: F-FPS60.

Status: fixed (2026-11-11).

## Reproduction
New game from the title with `PSXPORT_AUTO_SKIP=0`, no input after the start taps, `ires=0`, `fps60=1`, unpaced,
`PSXPORT_NATIVE_FRAMES=6500` (`scratch/narration/run.py` with `sop_plan.py`; runs `h1` on the e9dcfb8 tree and `n7`
on the working tree, both against the live psxport). The opening runs SOP, OPN and A00; about logic frame 5760, after
A00 is bound a second time, the run aborts.

## Observed
`[records:error] free stack cursor 090E7064 count 130 does not end at 800ED8C0`, then `abort` in
`world::RenderRecordPool::sample` (`game/world/render_record_pool.cpp`) from `MotionSet::sampleTrack`, under
`DrawReplay::beforeDraw` in `Engine::fieldFrame`. The free-stack cursor word `0x800E7E74` holds `0x090E7064`, not a
guest address. Runs that stop at 5600 frames (`b2`, `b3`, `b4`) pass the opening letterbox without it.

## Cause
`game/core/frame/frame_driver.cpp:rotatePacketPool` gave each pass only its parity's 80 KB pool, as the guest loop
does for its asynchronous GPU, and the 16:9 margin draws in this shot fill it: passes use 0x133E4..0x14204 bytes
(`scratch/rr0058/hw1/pool.txt`), and a parity-1 pass runs past the pool end 0x800E7E68 into the pad edge mask and the
free-stack cursor `0x800E7E74`. A store watch on the cursor (`scratch/rr0058/w1`, `PSXPORT_CW`) shows the first bad
store at f5251 from `FxSpriteSwarm::emitPerParticle` writing a packet's colour word `2E838383` there; the other runs'
cursors `090E7064` and `090E2A44` are packet tags. The same opening at 4:3 never aborts (`scratch/rr0058/a0`).

## Fix
`finishPass` draws the OT before the next pass top, so no packet outlives its pass: every pass now starts at
`PacketPool::kBase` with both pools (160 KB), and a pass that ends past `PacketPool::kEnd` aborts at the next pass top
(`docs/re/frame-loop.md`, Packet pool). Shown in `scratch/rr0058/fix1` (16:9, fps60, the opening to logic frame
7500): passes reach 0x14204 bytes, no free-stack error, the run ends at its budget; the shot itself at f5246 is
whole (`scratch/rr0058/shot2/f5246_q.png`).
