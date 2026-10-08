# FUN_80050B08 — the retail main loop

Decompiled from MAIN.EXE with `external/psxport/tools/decomp_pipeline.py` (targets 0x80050B08,
0x800506D0, 0x8008179C). `TombaFrameDriver` (`game/core/frame/frame_driver.*`) runs one pass of the
loop per frame; `enterLoop` is the prologue, called at the end of `TombaRuntime::bootInit`.

| guest | what | port owner |
|---|---|---|
| `0x1F800135` u8 | buffer parity | read/flipped by `TombaFrameDriver` |
| `0x800ED8C8` u32 | env pointer = `0x800E80A8 + parity * 0x2070` | `cfg.otBasePtr`, written on every flip |
| `0x800BF544` / `0x800BF4F4` | packet pool cur/last; pass top: last = cur, cur = `(0x800BFE68 + parity * 0x14000) & 0xFFFFFF` | `rotatePacketPool` |
| `0x800E809C` u16 | dwell counter, zeroed at pass top | `FrameCadence::beginLogicFrame` |
| `0x1F800235` u8 | vblank quota the pass waits for | `FrameCadence` |
| `0x800788AC` | per-frame state update | `Engine::frameUpdate` |
| `0x80051E60` | task scheduler | `PcScheduler::step` |
| `0x80080F6C(0)` | DrawSync | guest, `cfg.drawSync` |
| `0x800506D0` | task sleep countdown: state 1 slots decrement +2, re-arm to 2 at zero | `PcScheduler::tickSleepCountdown` |
| `0x1F80019C` u8 | present state, read after the gate | `finishPass` |
| state 0 | PutDispEnv(env+0x2000), PutDrawEnv(env+0x2014), DrawOTag(env+0x1FFC), flip, ClearOTagR(new env, 0x800) | `0x8008179C`, `0x800815D0`, `Engine::drawOTag`, `0x80081458` |
| state 1 | next pass, nothing presented | |
| state 2 | PutDispEnv(env+0x2000), state = 1, flip (no clear) | |
| state 3 | ClearOTagR(env, 0x800) | |
| other | next pass | |
| prologue | pool cur = `0x800BFE68 + (1 - parity) * 0x14000`, env from parity, ClearOTagR(env) | `TombaFrameDriver::enterLoop` |

PutDispEnv `0x8008179C` has two callers, both in this loop (0x80050D48, 0x80050D9C).
