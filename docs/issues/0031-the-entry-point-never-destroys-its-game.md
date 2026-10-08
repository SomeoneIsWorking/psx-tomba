# 0031: the entry point never destroys its Game

Status: open

## Observed
`printf 'newgame\nrun 1026\ncensus\nquit\n' | PSXPORT_REPL=1 build/bin/tomba2_port` exits without the
framework's run-end lines, among them `producer census at shutdown`, which `Game::~Game` logs in every other
title.

## Cause
`game/core/entry/main.cpp:main` allocates `new Game()` and returns without deleting it, and it composes the
boot by hand instead of through `Machine`, so neither `Game::~Game` nor `Machine::reportRunEnd` runs. It logs
Lightrec telemetry itself (`tomba2-exit`) as a second run-end path.

## Proper fix
Own the `Game` for the process lifetime with RAII and end the run through the framework's run-end owner, then
delete the hand-written telemetry line.
