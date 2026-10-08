#!/usr/bin/env python3
"""title_prompts.py — WHICH pad button a given Tomba! 2 screen is asking for, as a pure function.

Two drivers play this title and they must not disagree about the answer:

  * `tools/oracle_tomba2.py` drives the product over the framework REPL, which BLOCKS the game
    between commands, and feeds the same script to an independent console reference.
  * `tools/live_play.py` drives it over the live debug server while it keeps presenting.

The menu sequence is TITLE knowledge, not transport knowledge, so it lives here once and both drivers
ask it. A second copy inside the live driver would be free to answer a different prompt than the REPL
one, and the difference would surface as "the live run could not reach gameplay" with nothing to
compare against — the worst possible shape for a finding.

Each `Step` is the DECISION: which button this screen wants, how wide a press, and the guest state
that says the leg is done. Each driver delivers it in its own transport, and each transport adds only
what its clock needs:

  * REPL/console: `compare.tap(button, period, width)` — a per-GAME-frame input pattern, so the cadence
    is exact by construction.
  * Live: `tap <button> <width>` once per observation, never in the observation right after the screen
    changed. Its clock is a socket round trip, not a frame, so it paces presses by what it can observe
    rather than by a frame grid; `drive_to_gameplay` in tools/live_play.py says why, with the
    measurement.

WHY THE CADENCE IS 6-OF-12 AND NOT "hold Cross until something happens". The title is a TWO-PAGE
menu (docs/tomba2-newgame.md §1, verified there by cursor and screen capture): page 1 offers
NewGame (left) / LoadGame (right) and page 2 offers StartGame (left) / Options (right), and BOTH
pages confirm with Cross. So a single press advances one page, and the route has to press twice
before the GAME stage latches — a cadence of one press every 12 frames delivers both confirms
inside the ~25 frames the product's own REPL `newgame` policy takes (measured: GAME at frame 25,
tools/gate.py `boot`). Holding one press until the stage changed would confirm page 1 and then sit
there. The same cadence, in the same numbers, is what `game/core/debug/auto_drive.cpp` ships in C++ for
`PSXPORT_AUTO_SKIP`/REPL `newgame`; that is the product owner of the policy and this module is the
driver-side statement of it, deliberately kept in step with it.

THE ADDR BELOW ARE NOT INVENTED. Each carries the source that owns it, and
`verify_address_owners` re-reads those sources so a stale constant is reported rather than used.

WHAT IS DELIBERATELY NOT DECIDED HERE: anything that would destroy the operator's data to reach a
screenshot (a save-slot overwrite prompt, a memory-card format prompt). This route only ever
confirms NewGame/StartGame on a cold boot, which writes nothing.
"""

from __future__ import annotations

from dataclasses import dataclass
import re

from collections.abc import Iterator
from pathlib import Path

# ---- guest state, each with the source that owns it ---------------------------------------------
# The cooperative-task table's slot 0. `game/tomba2_types.h` names the base (T2_TASK_TABLE) and the
# slot stride; `game/core/debug/auto_drive.cpp` reads the stage entry at +0x0C out of the same slot.
TASK0 = 0x801FE000
TASK0_ENTRY = TASK0 + 0x0C
# Offsets inside the task-0 slot, as game/core/frame/frame_diagnostics.cpp and game/core/debug/auto_drive.cpp
# address them. They are here rather than in a driver because they are the layout of one guest
# object, and a driver that re-derives them is a driver that can disagree with the C++ that writes
# them. One read of TASK0..TASK0+TASK0_PROBE_BYTES therefore answers "which screen is this" in a
# single round trip, which is what keeps a duty cycle inside a title window a dozen frames wide.
STAGE_ENTRY_OFFSET = 0x0C
STATE_MACHINE_OFFSET = 0x48
CURSOR_OFFSET = 0x68
TASK0_PROBE_BYTES = 0x6C
# The six halfword state machine at task0+0x48, decoded exactly as
# `game/core/frame/frame_diagnostics.cpp` prints it: outer 48/4a/4c, leaf 4e/50/52.
STATE_MACHINE = TASK0 + STATE_MACHINE_OFFSET
STATE_MACHINE_BYTES = 12
# The GAME stage's task entry. game/core/entry/game_config.cpp: `.stageGame = 0x8010637cu`.
STAGE_GAME = 0x8010637C
# Tomba's master G block, owned by `game/player/actor_tomba.h` as `ActorTomba::G_ADDR`, with the
# 16.16 X/Y/Z position at +0x2C per its `velocityIntegrate` contract.
PLAYER_G = 0x800E7E80
PLAYER_POSITION = PLAYER_G + 0x2C
PLAYER_POSITION_WORDS = 3
# The guest's own frame counter (u32). This is the GAME's tick
# clock, distinct from the product's presented-frame counter: with interpolation on, the product
# presents two frames per game frame, so the two deltas are not equal and a report that prints only
# one of them cannot say which clock it measured.
GUEST_FRAME_COUNTER = 0x1F80017C
# The attract-vs-real distinguisher, from docs/tomba2-newgame.md §3: this scratchpad byte is 0 while
# the attract demo's replay driver is running and non-zero in real play. It lives in the scratchpad,
# which the console reference cannot read, so it is evidence the live driver reports and NOT part of
# the decision the two drivers share.
ATTRACT_FLAG = 0x1F800137
# The title menu's cursor byte: 0 = the left-hand entry (NewGame, then StartGame), 1 = the right-hand
# one (LoadGame, then Options). docs/tomba2-newgame.md §1, verified there as 0<->1.
MENU_CURSOR = TASK0 + CURSOR_OFFSET


def screen_from_task0(block: bytes) -> tuple[Screen, int]:
    """One read of the task-0 slot -> (the screen, the title menu's cursor byte). Both live in the
    same 0x6C bytes, so a driver that can only afford one command per decision still sees the whole
    title state — which cursor entry is highlighted is what says whether the next Cross confirms
    NewGame or LoadGame, and a driver that cannot see it is guessing."""
    if len(block) < TASK0_PROBE_BYTES:
        raise ValueError(f"task-0 probe needs {TASK0_PROBE_BYTES} bytes, got {len(block)}")
    return Screen.from_task0_block(block), block[CURSOR_OFFSET]

# The literal each address above is written as in this repository. Deliberately NOT a source PATH:
# a path couples this check to where an owner lives, so moving a file between concept directories
# would report a disagreement about a constant that never moved. verify_address_owners() resolves the
# literal across the first-party tree instead and reports which sources declare it.
ADDRESS_OWNERS: dict[str, str] = {
    "TASK0": "0x801FE000",
    "STAGE_GAME": "0x8010637cu",
    "PLAYER_G": "0x800E7E80",
    "GUEST_FRAME_COUNTER": "0x1F80017C",
    "ATTRACT_FLAG": "0x1f800137",
}


@dataclass(frozen=True)
class Screen:
    """The guest state a navigation decision may read: the task-0 stage entry and the six halfwords
    of the state machine. Nothing else, because anything else would be a decision one driver can
    make and the other cannot."""

    stage_entry: int
    sm: dict[str, int]

    @classmethod
    def from_bytes(cls, stage_entry: int, raw: bytes) -> Screen:
        names = ("48", "4a", "4c", "4e", "50", "52")
        return cls(stage_entry,
                   {name: int.from_bytes(raw[i * 2:i * 2 + 2], "little") for i, name in enumerate(names)})

    @classmethod
    def from_task0_block(cls, block: bytes) -> Screen:
        """Decode one read of the WHOLE task-0 slot, for a transport that can afford only one command
        per decision. `from_bytes` stays the two-address form, because the console reference reads
        the two words separately and there is no reason to make it read a block it cannot."""
        return cls.from_bytes(
            int.from_bytes(block[STAGE_ENTRY_OFFSET:STAGE_ENTRY_OFFSET + 4], "little"),
            block[STATE_MACHINE_OFFSET:STATE_MACHINE_OFFSET + STATE_MACHINE_BYTES])

    @property
    def in_game_stage(self) -> bool:
        return self.stage_entry == STAGE_GAME

    @property
    def left_intro(self) -> bool:
        """The opening has handed the outer machine off: sm[0x4a] left 0. Start is the game's own
        skip for it (game/core/debug/auto_drive.cpp SkipCutscene), so the route presses Start until this
        is true rather than guessing how long the intro runs."""
        return self.sm["4a"] != 0

    @property
    def in_field(self) -> bool:
        """The field runs: the outer machine is at sm[0x4a]==1 with the area machine at sm[0x4c]==2."""
        return self.in_game_stage and self.sm["4a"] == 1 and self.sm["4c"] == 2

    @property
    def in_free_roam(self) -> bool:
        """The player has the pad: the field's leaf machine sm[0x4e] is 1. The opening cutscene (the
        landing and the fisherman's dialogue) runs it at 9 and ignores the pad."""
        return self.in_field and self.sm["4e"] == 1

    def describe(self) -> str:
        return (f"stage=0x{self.stage_entry:08X} sm[48={self.sm['48']} 4a={self.sm['4a']} "
                f"4c={self.sm['4c']} 4e={self.sm['4e']} 50={self.sm['50']} 52={self.sm['52']}]")


@dataclass(frozen=True)
class Step:
    """One leg of the route: the button this screen wants, the duty cycle it wants it at, and the
    property of `Screen` that says the leg is done. `button=None` means hold nothing and just wait,
    which is a real instruction: the field only arms after the intro hands over, and pressing
    through that handoff is a different input than the one the oracle comparison fed its cores."""

    name: str
    goal: str
    button: str | None = None
    period: int = 0
    width: int = 0

    def reached(self, screen: Screen) -> bool:
        return bool(getattr(screen, self.goal))

    @property
    def idle_frames(self) -> int:
        """Presented frames to wait between two presses. A step with no button waits for nothing
        because it presses nothing; its frames are spent observing."""
        return max(0, self.period - self.width)


# Cold boot -> the GAME stage -> the field -> the player holding the pad. The order is the order the
# title runs in, and each leg's goal implies the previous one, so a leg cannot be skipped.
GAMEPLAY_ROUTE: tuple[Step, ...] = (
    Step("GAME stage", "in_game_stage", "cross", 12, 6),
    Step("intro skipped", "left_intro", "start", 24, 6),
    Step("field", "in_field"),
    Step("free roam", "in_free_roam", "start", 40, 6),
)


# Directories that hold vendored, generated, or built code rather than this repository's own C++.
_NOT_OWNED = ("build", "scratch", "external", ".git")


def first_party_sources(root: Path) -> Iterator[Path]:
    """Every first-party C++ source under the repository, laid out however the owners choose."""
    for path in sorted(root.rglob("*")):
        if not path.is_file() or path.suffix not in (".cpp", ".h", ".hpp", ".c"):
            continue
        if any(part in _NOT_OWNED for part in path.relative_to(root).parts):
            continue
        yield path


# A declaration of the literal, not any mention of it: the name being bound, then `=`, then the
# address. A comment or a string that happens to contain the digits is not the owner declaring it.
# A NAME bound to the literal: `constexpr uint32_t kTickWord = 0x1F80017Cu;`,
# `const uint32_t S0 = 0x800E7E80u;`, or a designated initialiser `.taskTableBase = 0x801fe000u,`.
# Comment lines are excluded, so prose that mentions an address is not an owner declaring it.
_DECLARES_ADDRESS = re.compile(
    r"([A-Za-z_][A-Za-z0-9_]*)\s*=\s*[^;]*?" + r"LITERAL")


def declaring_symbols(root: Path, literal: str) -> list[str]:
    """Every `NAME = <address>` binding of `literal` in the first-party sources, as `path:NAME`.

    Resolution is by VALUE, so an owner may live in any directory; what must hold is that the address
    is still BOUND to a name in this repository. Requiring a binding rather than any occurrence is
    what keeps a renamed constant, a moved file, and an incidental mention three different things.
    """
    pattern = re.compile(_DECLARES_ADDRESS.pattern.replace("LITERAL", re.escape(literal)), re.IGNORECASE)
    found: list[str] = []
    for path in first_party_sources(root):
        relative = path.relative_to(root)
        try:
            text = path.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        for line in text.splitlines():
            stripped = line.strip()
            if literal.lower() not in stripped.lower() or stripped.startswith("//"):
                continue
            match = pattern.search(stripped)
            if match:
                found.append(f"{relative}:{match.group(1)}")
    return found


def verify_address_owners(root: Path) -> tuple[int, int, list[str], dict[str, list[str]]]:
    """Re-read the title sources and report how many still bind each address this tool reads.

    A tool that reads guest memory at a hard-coded address is one edit away from reading the wrong
    place, and a wrong address reads plausible zeros — the failure mode that looks like a game that
    is merely not responding. So the constants are checked against the tree, with a denominator:
    (matched, scanned, disagreements).

    Resolution is by VALUE across every first-party source, not by a recorded file path: the owner
    of an address is allowed to move between concept directories, and a move is not a disagreement
    about the address. A disagreement means the address is bound to no name in this repository any
    more, which is the only thing worth refusing a run over. It reads text, so it cannot prove the
    address means what it says; that is what the run's own evidence is for.
    """
    matched = 0
    disagreements: list[str] = []
    owners_by_name: dict[str, list[str]] = {}
    for name, literal in ADDRESS_OWNERS.items():
        owners = declaring_symbols(root, literal)
        owners_by_name[name] = owners
        if owners:
            matched += 1
        else:
            disagreements.append(f"{name}: no first-party source binds {literal} to a name any more")
    return matched, len(ADDRESS_OWNERS), disagreements, owners_by_name


