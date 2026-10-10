// dev_areas.cpp — the game's area index for the DEV WARP selector (GameHooks::devAreaCount /
// devAreaName / devWarpAllowed). Game-side on purpose: the framework must #include no game header and
// must not know a Tomba guest address, so the area count, the names and the "is a warp legal right now"
// test all live here and reach the framework only through the hook table.
//
// COUNT: 22 areas, ids 0..21. Established 2026-07-22 (kanban #24) three independent ways — 22 A0*.BIN
// files on the disc, 22 entries in every one of MAIN's six per-area handler tables, and GAME.BIN's
// next-table zeroed at 22/23. Area load is file index `area + 3` into the stride-8 table at 0x800BE118,
// which is [0]=OPN [1]=CRD [2]=SOP [3..24]=A00..A0L [25]=START [26]=DEMO [27]=GAME — so an id >= 22
// loads START.BIN into the mode slot and produces out-of-range CD reads. The selector therefore cannot
// offer one; that is the UI half of kanban #36.
//
// NAMES: an area index is a fact, an area name is a CLAIM and needs a source (the USER, or a name found
// in guest data). Two cards were filed against invented names before this rule existed — #24 against an
// "area 22" that does not exist, and #26 against a "Temple interior" that is the ghost pig boss arena.
// So an unnamed area shows as "Area N" here rather than a guess. See docs/areas.md; add a row there in
// the same commit as a name added below.
#include "core.h"
#include "debug/dev_gate.h"
#include "debug/dev_warp.h"
#include "engine/engine.h"
#include "game.h"

namespace {

constexpr int AREA_COUNT = tomba::DevWarp::kAreaCount; // ids 0..21

// Sourced names only. Blank = not established; the selector renders "Area N".
const char *const kAreaNames[AREA_COUNT] = {
    /*  0 */ "",
    /*  1 */ "",
    /*  2 */ "",
    /*  3 */ "",
    /*  4 */ "",
    /*  5 */ "",
    /*  6 */ "",
    /*  7 */ "",
    /*  8 */ "Water Temple", // USER 2026-07-23
    /*  9 */ "",
    /* 10 */ "",
    /* 11 */ "",
    /* 12 */ "Ghost Pig boss", // USER 2026-07-23
    /* 13 */ "",
    /* 14 */ "",
    /* 15 */ "",
    /* 16 */ "",
    /* 17 */ "",
    /* 18 */ "",
    /* 19 */ "",
    /* 20 */ "",
    /* 21 */ "",
};

} // namespace

int Engine::devAreaCount() {
  return AREA_COUNT;
}

// Display label for the selector row. Static storage: the caller only formats it into a row value.
const char *Engine::devAreaName(int area) {
  if (area < 0 || area >= AREA_COUNT) {
    return "";
  }
  return kAreaNames[area];
}

bool Engine::devWarpAllowed(Core *c) {
  return tomba::DevGate::inGameStage(*c);
}
