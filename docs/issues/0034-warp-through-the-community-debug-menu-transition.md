# 0034 — the debug warp should launch the transition the community debug menu uses

Status: open.

## Reference

The Tomba Club "Debug Menu (Press L3 to toggle)" code list for SCUS-94454 (author unicorngoulash), carried verbatim in
`github.com/mstan/Tomba2Recomp` at `mods/sources/tomba2_debug_menu.cht`, with its README at
`mods/development/tomba2.debug.debug-menu/1.0.0/README.txt`. It writes a MIPS payload to `0x8000C000` and re-points
five call sites in the gameplay overlay at it. Its WARP TOOLS page picks a destination area and entry and launches the
game's own transition. The list's licence is not stated: read it as evidence, never copy the payload into this repo.

## Task

Decode the WARP TOOLS routine from the code list (the 32-bit writes at `0x8000C000+`) and name the guest routines and
state words it uses to start a transition. Compare with `tomba::applyColdWarp` (`game/core/debug/dev_warp.cpp`), which
loads the destination synchronously and writes `sm[0x4c]`. Move the debug warp onto the transition the menu uses, so
it runs through the game's own owners with the entry point the menu selects, and record the routines in
`docs/re-frontier.md`.
