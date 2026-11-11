# 0058 — the opening aborts on a corrupt render-record free stack

Features: F-FPS60.

Status: open (2026-11-11).

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
Not traced. The pool reads the guest's own free stack, so either a guest writer stores a bad cursor in this
transition, or `0x800E7E74` is not the free-stack cursor in the scene the opening switches into.
