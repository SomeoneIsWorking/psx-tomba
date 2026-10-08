#!/usr/bin/env python3
"""Tomba! 2's side of the oracle comparison (psxport tools/oracle/compare.py, docs/oracle.md):
checkpoint predicates, declared state, exclusions, the game-frame barrier and the input policy.

Every predicate reads main RAM only, because the console reference cannot read the scratchpad.

WHICH BUTTON A SCREEN WANTS IS NOT HERE. The route's decisions — the button per leg, its duty cycle,
and the guest state that ends the leg — live in tools/title_prompts.py, which tools/live_play.py
asks too, because they are title knowledge rather than comparison knowledge. This module keeps the
transport: how one core is stepped, what a settle pad is, and what the two cores are compared on.
"""

from __future__ import annotations

from compare import (
    Checkpoint,
    CoreSession,
    DeclaredRange,
    Driver,
    Pattern,
    SelftestSeed,
    Settle,
    released,
    step_until_counter_resets,
    tap,
    u32,
)
from compare_cores import CoreError
from title_prompts import (
    GAMEPLAY_ROUTE,
    PLAYER_G,
    PLAYER_POSITION,
    STATE_MACHINE,
    STATE_MACHINE_BYTES,
    TASK0,
    TASK0_ENTRY,
    Screen,
    Step,
)

name = "Tomba! 2 (SCUS_944.54)"

# The addresses the ROUTE reads are title_prompts' (see the module docstring). What is left here is
# what only the comparison needs.
AREA_INDEX = 0x800BF870
DWELL_COUNTER = 0x800E809C       # VBlanks since the main loop last passed its vblank gate (GameConfig)
PAD_CURRENT = 0x800ECF54         # frame fence FUN_800788AC: this frame's pad word (docs/engine_re.md)
PAD_PRESSED = 0x800E7E68         # ... buttons newly pressed this frame
PAD_RELEASED = 0x800F23A4        # ... buttons newly released this frame
TASK_RUNNING = 4                 # task slot state while the scheduler is inside that task's logic

# The resident camera object (game/camera/cutscene_camera.h CAM_OBJ). The guest OVERLOADS this one
# block: the per-frame camera driver 0x8006EC44 hardcodes it as the camera, and the GAME-overlay fade
# sequencer 0x8010957C is handed the same address as its node (game/render/screen_fade.h `sequence`,
# called from Engine::fieldRun's sm[0x4e]==0xb branch). So the camera and the screen fade's ramp share
# a base, which is why one constant serves both groups below.
CAM_OBJ = 0x800E8008

declared = (
    DeclaredRange("task0.state", TASK0, 2, False),  # frame-phase dependent, see excluded
    DeclaredRange("task0.entry", TASK0_ENTRY, 4, True),
    DeclaredRange("state_machine", STATE_MACHINE, 12, True),
    DeclaredRange("area_index", AREA_INDEX, 1, True),
    DeclaredRange("player.position", PLAYER_POSITION, 12, True),     # 16.16 X/Y/Z
    DeclaredRange("player.motion", PLAYER_G + 0x44, 10, True),     # dir/speed pairs
    DeclaredRange("player.G", PLAYER_G, 0x184, False),
    # The pad words are what each core's game actually read; a mismatch there is an input-delivery
    # defect in the harness, not a product divergence, and must fail before anything downstream.
    DeclaredRange("pad.current", PAD_CURRENT, 4, True),
    DeclaredRange("pad.pressed", PAD_PRESSED, 4, True),
    DeclaredRange("pad.released", PAD_RELEASED, 4, True),
    # --- What the PICTURE needs and the RAM comparison does not; see picture_decisive below. ---
    # The camera's mode byte drives an 18-entry dispatch (cam[0x64]&0x3F) and its three angles place
    # the look point (cam[0x6c/0x6e/0x70] -> rcos/rsin), so between them they decide where the scene
    # is seen from. The camera's world position is not here: it is assembled into the scratchpad
    # (0x1F8000D2/D6/DA, docs/areas.md), which the console reference cannot read at all.
    # The whole camera object, on the same pattern as player.G above: a named sub-range gives the
    # precise message, the block catches everything else. Measured 2026-09-19 equal on both cores at
    # free_roam and at 30/60/120/180/240 frames along the scripted route, while the picture
    # differed -- which is what moved the search downstream of it (docs/issues/0019).
    DeclaredRange("camera.object", CAM_OBJ, 0x90, False),
    DeclaredRange("camera.mode", CAM_OBJ + 0x64, 1, False),
    DeclaredRange("camera.angles", CAM_OBJ + 0x6C, 6, False),
    # The fade sequencer's own state: outer state at +2 (0 init, 1 running), running substep at +3,
    # the step-2 delay counter at +104 and the RAMP LEVEL 0..31 at +106. The level is the field that
    # separates two frames of the same scene at different brightness, and it is the reason this group
    # exists (docs/issues/0018).
    DeclaredRange("fade.sequencer", CAM_OBJ + 0x02, 2, False),
    DeclaredRange("fade.level", CAM_OBJ + 0x68, 4, False),
    # The FIELD's own fade ramp, which is a different fade from the sequencer above and is the one
    # that runs on area entry and exit: Engine::fieldRun case 9 arms it to 31 and case 10 counts it
    # down to 0, driving the leaf at 0x8007E9C8 with (level * -8) replicated into R/G/B
    # (game/core/engine/engine.cpp:1721-1738). The guest reaches it through a scratchpad pointer
    # (`sm = mem_r32(0x1F800138)`), which the console reference cannot read; the fixed base is the
    # same 0x801FE000 the state_machine range above is expressed against.
    DeclaredRange("fade.field_ramp", TASK0 + 0x6E, 1, False),
)

# The ranges that must agree before a PIXEL COUNT means anything. Not the same question as the RAM
# set: `decisive` asks "may the simulation differ here", and a camera angle or a fade level can leave
# the game behaving identically while moving every pixel on screen. Without this, compare.py falls
# back to the RAM set -- which is what produced this title's first picture number, free_roam at
# 17664/71680 (24.64%) on 2026-09-19, from two frames of the same scene at different fade brightness.
# That number ranked nothing (docs/issues/0018).
#
# These four are informational for the RAM comparison and decisive only here, which is a deliberately
# narrow claim: nothing has yet measured whether the camera or the fade carries a standing timing
# offset between the cores the way Spyro 1's does, so this does not start failing RAM runs that
# passed before. If a measurement later shows they agree exactly, they can be promoted.
picture_decisive = tuple(r.name for r in declared if r.decisive) + (
    "camera.object", "camera.mode", "camera.angles",
    "fade.sequencer", "fade.level", "fade.field_ramp",
)

excluded = {
    "task0 +0x00 scheduler state (informational only)":
        "the product samples after the buffer swap, when FUN_800506D0 has re-armed the yielded task "
        "to 2; the console samples at the VBlank interrupt while the guest still spins in its "
        "vblank gate before that sweep, so it reads 1 (docs/engine_re.md main loop order)",
    "task0 +0x04..+0x0B (BIOS thread/event handles)":
        "assigned by the thread service; psxport's native HLE threads number them differently "
        "from the SCPH-1001 BIOS (docs/engine_re.md task scheduler)",
    "scratchpad 0x1F800000..0x1F800400": "the console reference exposes main RAM only",
    "PRNG, callback ring, stdio, OT and packet pools":
        "allocation/timing state, not gameplay state (docs/oracle.md retained observations)",
    "VRAM, SPU and CD device state": "separate pixel, audio and hardware questions",
}

# Representative free-roam input after the third checkpoint: held buttons and game-frame counts.
# The first segment repeats the released pad the checkpoint parked with (Playback's precondition).
#
# A recorded .pad replay CANNOT be used here, which is worth stating because it is the obvious idea.
# `runtime/psx/pad_input.h` records that replays are "only valid from boot" — they are one mask per
# pad-service frame from reset — while this schedule starts at a checkpoint the driver reached with
# its own input, and the console reference is stepped by VBlank and realigned to the guest's vblank
# gate every frame. There is no frame mapping between the two that survives a boot loader running
# across gates, and a wrong mapping would feed the two cores different input and report it as a
# product divergence.
#
# So the route is scripted, and its length and variety are the coverage. Tomba walks left away from
# the landing spot and back right (short of the fisherman, whose dialogue would take the pad),
# jumps standing and while walking, crouches, uses the attack and the jump-attack, walks the far
# side of the clearing, and opens and closes the in-game item menu -- which is the only segment that
# leaves free roam, and is included for exactly that reason: a menu is a different task and a
# different leaf machine, and comparing it exercises code the walk cannot reach.
gameplay = (
    (frozenset(), 30),
    (frozenset({"left"}), 60),
    (frozenset(), 30),
    (frozenset({"right"}), 90),
    (frozenset(), 30),
    (frozenset({"cross"}), 6),
    (frozenset(), 60),
    (frozenset({"left", "cross"}), 6),
    (frozenset({"left"}), 60),
    (frozenset(), 30),
    # --- extended 2026-09-19: the 402-frame route above stayed within a few steps of the landing
    # spot and never left free roam, so it could not have seen a menu, an attack, or a crouch.
    (frozenset({"down"}), 30),
    (frozenset(), 20),
    (frozenset({"square"}), 8),
    (frozenset(), 40),
    (frozenset({"right"}), 120),
    (frozenset({"right", "cross"}), 8),
    (frozenset({"right"}), 40),
    (frozenset(), 30),
    (frozenset({"circle"}), 8),
    (frozenset(), 40),
    (frozenset({"left"}), 120),
    (frozenset(), 40),
    (frozenset({"cross"}), 8),
    (frozenset({"square"}), 8),
    (frozenset(), 60),
    # The item menu: open, dwell, close. Triangle is the in-game menu button (replays/bugs/
    # ingame-item-menu.pad).
    (frozenset({"triangle"}), 8),
    (frozenset(), 90),
    (frozenset({"triangle"}), 8),
    (frozenset(), 90),
    (frozenset({"right"}), 60),
    (frozenset(), 30),
)

selftest = SelftestSeed(PLAYER_POSITION, "player.position", 0)



def lookahead(core: CoreSession) -> int:
    """The console's park point is one VBlank after the guest's vblank gate; the pad polled at
    that VBlank is what the NEXT frame's pad fence reads, so a hold committed there arrives one
    frame late. The product's frame driver composes the pad at the start of the frame it steps."""
    return 1 if core.reference else 0


def observe(core: CoreSession) -> Screen:
    return Screen.from_bytes(u32(core, TASK0_ENTRY), core.read(STATE_MACHINE, STATE_MACHINE_BYTES))


def summary(core: CoreSession) -> dict:
    return {"sm": observe(core).sm}


def advance(core: CoreSession, frames: int, strict: bool = False) -> None:
    """Step `frames` Tomba! 2 game frames. The product's driver runs one game frame per step.

    The VBlank-stepped console runs a game frame when the guest main loop passes its vblank gate
    (the dwell counter resets; the field gates on 2 VBlanks). The barrier is pinned one VBlank
    after that gate: task 0's logic has finished and the guest spins in the gate. `strict` refuses
    a frame whose logic overran a second gate (boot loaders legitimately run across frames;
    scheduled gameplay must not)."""
    if not core.reference:
        core.step(frames)
        return
    for _ in range(frames):
        step_until_counter_resets(core, DWELL_COUNTER)
        dwell = u32(core, DWELL_COUNTER)
        stepped_past_gate = 0
        while stepped_past_gate == 0 or int.from_bytes(core.read(TASK0, 2), "little") == TASK_RUNNING:
            core.step(1)
            stepped_past_gate += 1
            previous, dwell = dwell, u32(core, DWELL_COUNTER)
            if strict and dwell < previous:
                raise CoreError(f"{core.name}: the guest passed its vblank gate again after "
                                f"{stepped_past_gate} VBlank(s) with task 0 still running; a lag or "
                                f"1-VBlank frame cannot be aligned with the product's frame count")


def _pattern(step: Step) -> Pattern:
    """One title_prompts step as a per-game-frame input pattern for the two-core transport. A step
    with no button becomes `released`, which is an instruction and not an absence of one."""
    return released if step.button is None else tap(step.button, step.period, step.width)


def reach_game(driver: Driver, core: CoreSession, budget: int, settle: Settle) -> tuple[int, frozenset[str]]:
    """Confirm the title's two menu pages (6 of every 12 frames) until task 0 runs the GAME stage —
    title_prompts' first leg, which is the product REPL's own `newgame` policy expressed once so
    both cores receive the same input."""
    step = GAMEPLAY_ROUTE[0]
    return driver.drive(core, budget, step.reached, _pattern(step), step.name, settle)


def reach_field(driver: Driver, core: CoreSession, budget: int, settle: Settle) -> tuple[int, frozenset[str]]:
    """Skip the intro with Start until the outer machine leaves it (sm[0x4a]!=0), then wait, without
    input, for the field to run. The settle pad belongs to the Start phase; the waiting phase always
    settles on a released pad."""
    intro, field = GAMEPLAY_ROUTE[1], GAMEPLAY_ROUTE[2]
    used, settle = driver.drive(core, budget, intro.reached, _pattern(intro), intro.name, settle)
    waited, _ = driver.drive(core, budget - used, field.reached, _pattern(field), field.name, frozenset())
    return used + waited, settle


def reach_free_roam(driver: Driver, core: CoreSession, budget: int, settle: Settle) -> tuple[int, frozenset[str]]:
    """Skip the field's opening cutscene with Start (the product's own auto-skip cadence) until the
    leaf machine hands the pad to the player."""
    step = GAMEPLAY_ROUTE[3]
    return driver.drive(core, budget, step.reached, _pattern(step), step.name, settle)


checkpoints = (
    Checkpoint("game_stage", reach_game),
    Checkpoint("field", reach_field),
    Checkpoint("free_roam", reach_free_roam),
)
