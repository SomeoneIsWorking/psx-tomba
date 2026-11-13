# FUN_80050B08 — the retail main loop

Decompiled from MAIN.EXE with `external/psxport/tools/decomp_pipeline.py` (targets 0x80050B08,
0x800506D0, 0x8008179C). `tomba::FrameDriver` (`game/core/frame/frame_driver.*`) runs one pass of the
loop per frame; `enterLoop` is the prologue, called at the end of `tomba::Runtime::bootInit`.

| guest | what | port owner |
|---|---|---|
| `0x1F800135` u8 | buffer parity | read/flipped by `tomba::FrameDriver` |
| `0x800ED8C8` u32 | env pointer = `0x800E80A8 + parity * 0x2070` | `cfg.otBasePtr`, written on every flip |
| `0x800BF544` / `0x800BF4F4` | packet pool cur/last; pass top: last = cur, cur = `(0x800BFE68 + parity * 0x14000) & 0xFFFFFF` | `rotatePacketPool`: cur = `0x800BFE68` every pass, both pools (below) |
| `0x800E809C` u16 | dwell counter, zeroed at pass top | `FrameCadence::beginLogicFrame` |
| `0x1F800235` u8 | vblank quota the pass waits for | `FrameCadence` |
| `0x800788AC` | per-frame state update | `Engine::frameUpdate` |
| `0x80051E60` | task scheduler | `tomba::sched::Scheduler::step` |
| `0x80080F6C(0)` | DrawSync | guest, `cfg.drawSync` |
| `0x800506D0` | task sleep countdown: state 1 slots decrement +2, re-arm to 2 at zero | `tomba::sched::Scheduler::tickSleepCountdown` |
| `0x1F80019C` u8 | present state, read after the gate | `finishPass` |
| state 0 | PutDispEnv(env+0x2000), PutDrawEnv(env+0x2014), DrawOTag(env+0x1FFC), flip, ClearOTagR(new env, 0x800) | `0x8008179C`, `0x800815D0`, `Engine::drawOTag`, `0x80081458` |
| state 1 | next pass, nothing presented | |
| state 2 | PutDispEnv(env+0x2000), state = 1, flip (no clear) | |
| state 3 | ClearOTagR(env, 0x800) | |
| other | next pass | |
| prologue | pool cur = `0x800BFE68 + (1 - parity) * 0x14000`, env from parity, ClearOTagR(env) | `tomba::FrameDriver::enterLoop` (cur = `0x800BFE68`) |

PutDispEnv `0x8008179C` has two callers, both in this loop (0x80050D48, 0x80050D9C).

## Packet pool

The guest splits 0x800BFE68..0x800E7E68 into two 80 KB parity pools so a pass can build while the GPU reads the
other's packets. `finishPass` draws the OT before the next pass top, so no packet outlives its pass, and the port
gives every pass both pools from `0x800BFE68`. The widened 16:9 cull outgrows one pool: in the opening's letterboxed
village shot after A00 binds the second time (logic frame ~5760) passes use 0x133E4..0x14204 bytes, and a parity-1
pass ran past 0x800E7E68 into the pad edge mask and the render-record free-stack cursor `0x800E7E74` (issue 0058).
The same opening at 4:3 never aborts. At 21:9 the opening (new game, OPN and A00, 6,880 passes) peaks at 90,708 of 163,840 bytes, 156 of its passes past one parity pool (`scratch/pool-open/a21`, `tomba-packet-pool` debug channel, one line per pass). Every native writer reserves its packet bytes through `PacketPool::reserve` before writing them and aborts naming the packet; guest code writes past the end are still caught only when the pass that ends past `PacketPool::kEnd` reaches the next pass top.
