# 0025 — the sound driver's cluster base is `32784 << 16`, and the scratchpad's is NOT the same arithmetic

Found 2026-09-28 during the readability pass over `game/audio/sequencer.cpp`. **Not fixed**: it is a
naming collision, and fixing a naming collision by picking a winner is exactly the judgement a
behaviour-preserving pass must not make on its own.

## The collision

Two guest bases in this title are written as a shifted constant in the port, and only one of the two
shifted constants is the same arithmetic as the other:

| base | written as | evaluates to |
|---|---|---|
| the libsnd cluster | `32784u << 16` | `0x80100000` — main RAM, 0x8010_0000 |
| the PSX scratchpad | `0x1F800000u` | `0x1F800000` — the 1 KB scratch block |

The libsnd cluster base is genuinely a shifted constant in the guest's own instruction stream (its
`lui` immediate is 32784), and `game/audio/libsnd_globals.h` now says so and shows the arithmetic. The
scratchpad is NOT: it is a plain address in the guest's stream, and the reason it looked like a
cluster is that this title has both bases and they sit in the same ports.

## How this was actually found

The readability pass wrote the scratchpad base in `game/scene/script_globals.h` as
`32768u << 16` — copying the libsnd idiom — which is `0x80000000`, not `0x1F800000`. It would have
pointed every scratchpad read and write at the bottom of main RAM.

`tests/test_script_interp_vocabulary.cpp::test_the_scratchpad_globals_are_where_the_guest_puts_them`
caught it on the first run, before the file had ever executed. That test now also checks that every
scratchpad slot the interpreter names is inside the 1 KB block, which is the check that would have
caught it without a table of expected values.

The remaining half of the issue is real and is NOT fixed: with two shifted-looking constants in one
title, the next reader has no way to tell from the expression alone which kind of base it is looking
at. `libsnd_globals.h` states the rule in its own banner, and `script_globals.h` says the scratchpad
is "NOT a `n << 16` cluster base the way the libsnd globals at 0x80100000 are" — but the rule lives in
two headers, and the only reason the collision is visible at all is that both were written in the same
session.

## What would close it

- A project-level note in `docs/engine_re.md` stating which guest bases are written as shifted
  immediates and which are plain addresses, listing every one of them once. That is the same
  "one fact, one home" the readability pass applied within each file, applied across them.
- A check that refuses a `<< 16` base whose value is not one of the recorded guest `lui` immediates.
  That is a policy check rather than a test, and it belongs beside the other architecture checks in
  `psxport/tools/check_cpp_style.py` — which is framework territory this pass did not touch.
- Until then: the two headers each carry the rule, and the scratchpad one names the collision
  explicitly, which is weaker than one home for it and stronger than leaving it implicit.
