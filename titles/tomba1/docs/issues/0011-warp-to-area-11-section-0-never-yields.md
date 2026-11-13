# 0011: a warp to area 11 section 0 never finishes a gameplay frame

Status: open (2026-11-13). Owner: `titles/tomba1/game/core/debug/debug_menu.cpp:tomba1::DebugMenu` (it offers the
section), with the arrival itself in `dev_warp.cpp:DevWarp::applyArmed`.

## Reproduction
`scratch/t1hang/hang2.py` (`TARGET=11/0`, run through `scratch/s005t1/run.py` with
`DRIVE_EXE=scratch/bin/tomba1/SCUS_942.36`): pauses right after the warp and steps one field at a time, dumping RAM
at each overrun field. The old `scratch/t1warp/hang.py` dumped before the overrun because it polled the log
without pausing; the task's 33 fields pass in about 130 ms unpaced and the process exits on the fault.

## Found (2026-11-13)
- Arrival is correct: Tomba is placed at (20, -52, 0) from `0x8007BC70` (store watch on `0x800A53A8..B0`).
- The machine the transition picks, `0x80077084[area + 0x8009EBA0][section]`, is 5 (`FUN_8001C434`) for 11/0 and 2
  for 11/1. Machine 5's frame `FUN_8001C618` calls the code module at `0x800ECEEC` and `0x800EC588`.
- After the warp the module at `0x800E7388` is the field module from area 0; `0x800ECEEC` there is the tail of an
  entity loop, not an entry. A warp to 5/0 (also machine 5) loads a different module whose `0x800ECEEC` is an
  entry, and that warp runs normally.
- `FUN_80021CC8` loads the area list and the section list `PTR_80079150[area][section + 1]`. 11/0's list
  `0x800980B0` is empty (so are 11/3 and 11/4); 11/1 and 11/2 end with the 0xF0 module entry. So 11/0 brings no
  module of its own: the game only enters it with the module already loaded by where it comes from. No exit
  record in the resident exit tables `0x8007C54C` names 11/0 or 16/0 as a destination.
- The hang: the frame body re-runs about 38 times per field (`0x1F8001F8` increments, `0x8009BCD0` does not),
  every frame above `FUN_800FFAE8` is unchanged between fields, and Tomba's x/y take garbage floor values.
  The stray call into the module tail is the re-entry; which epilogue returns into `0x8001C65C` is not traced.

Machine-5 sections: 5/0, 5/2, 8/0, 8/2, 11/0, 15/0, 16/0, 17/0, 18/0, 19/2.

## Next
Find what loads machine 5's module before 11/0 in play (the area or event that enters it; candidates are the
other machine-5 section 0s of areas 15 to 18), then either warp through that path or refuse the section in the
catalogue. Do not refuse by "machine 5" or "empty list" alone: 5/0 is machine 5 and works, and area 16's
`PTR_80079150[16]` table reads all zero from area 11, so its lists are not resolvable from there. Area 16 and
the 10 to 11 fault are still untried with the stepping script.
