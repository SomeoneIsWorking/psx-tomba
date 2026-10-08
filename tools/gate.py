#!/usr/bin/env python3
"""gate.py — the AGENT's headless gate. Never `./run.sh`.

WHY THIS EXISTS (USER, 2026-08-11: "I don't know why you are running run.sh, that is for me to play,
you should have your own tools"). `run.sh` is the user's end-to-end WINDOWED play launcher: it re-syncs
submodules, re-extracts the boot executable, rebuilds, and launches. An agent invoking it competes with
the user's session for the shared tree, and its submodule re-sync can silently revert in-progress
framework work to the recorded pin — after which every measurement describes a different framework than
the agent thinks (`external/psxport/docs/workspace/PROTOCOL.md` records that incident). So agents drive
the ALREADY-BUILT binary, and that is what this does.

It does NOT build and does NOT extract. Build with:
    cmake --build build --target tomba2_port -j$(nproc)

WHAT A PASS MEANS, STATED SO IT CANNOT BE OVERREAD. A green gate here means: the binary launched
headless, the REPL accepted the script, the run reached the asserted frame, and nothing matching the
failure patterns appeared. It says NOTHING about pixels or about SBS byte-exactness. Every run prints
its own denominator — frames reached, lines scanned, patterns searched — because a gate that prints
only "OK" is indistinguishable from one that never ran the game (CLAUDE.md: "a diagnostic that can
print nothing is lying").

REFUSALS (exit 2, never 0): a missing binary, a missing boot executable, a REPL that produced no
output, or a run whose frame counter never appeared. Each says what it did NOT do rather than
returning a clean empty pass.

USAGE
    python3 tools/gate.py boot [--frames 400] [--expect-frame 440]
    python3 tools/gate.py replay <replays/.../foo.pad> [--frames 900]
    python3 tools/gate.py run --script 'newgame
run 400
stage
quit' [--debug nofx,plumefx]
"""
from __future__ import annotations

import argparse
import os
import re
import subprocess
import sys
import time
from pathlib import Path

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
# The framework's own tools live in the checkout tools/psxport_fetch.py resolved. This insertion
# precedes the import below and uses the same external/psxport/tools resolution as every other
# framework import in this file.
PSXPORT = os.path.join(REPO, 'external', 'psxport')
sys.path.insert(0, os.path.join(PSXPORT, 'tools'))

# This repository's canonical maintainer build directory: the same tree verify_ci.py builds. It is a
# property of THIS repository's layout, so it is owned here.
CANONICAL_VERIFY_BUILD = os.path.join(REPO, 'build', 'ci')

from run import resolve_disc  # noqa: E402

# The product this gate drives is the canonical maintainer build (the same tree verify_ci.py
# builds); the player build under build/player/<toolchain> belongs to run.sh.
BIN = os.path.join(CANONICAL_VERIFY_BUILD, 'bin', 'tomba2_port')
EXE = os.path.join(REPO, 'scratch', 'bin', 'tomba2', 'MAIN.EXE')
LOGDIR = os.path.join(REPO, 'scratch', 'logs')

# The enhancement configuration every agent run of this port is gated under. It is passed to
# psxport's agent_environment rather than left to the product's working-directory discovery, which
# would otherwise configure a gate from the operator's untracked psxport_settings.ini.
SHIPPING_SETTINGS = os.path.join(REPO, 'tools', 'shipping_settings.ini')
sys.path.insert(0, os.path.join(PSXPORT, 'tools', 'oracle'))
from compare import binary_identity  # noqa: E402  (the framework's one binary-identity owner)

# Anything here in the output fails the gate. A guest instruction path MISS aborts the process by design, but it can
# also appear in a line that scrolls past a watchdog kill, so it is matched as text too.
FAIL_PATTERNS = [
    r'\bFATAL\b',
    r'\babort\b',
    r'Aborted',
    r'guest instruction path[- ]MISS',
    r'typed runtime address dispatch miss',
    r'\bASSERT\b',
    r'Segmentation fault',
    r'std::bad_alloc',
    r'VSync:\s*timeout',
    r'GUEST VSYNC VIOLATION',
]
# The cfg subsystem names a knob that matched nothing. Not fatal, but it means a flag the caller was
# relying on did NOTHING, so it is surfaced loudly rather than buried.
UNKNOWN_KNOB = re.compile(r'UNKNOWN knob (\S+)')
FRAME_RE = re.compile(r'frame[= ](\d+)')
# `newgame` pulses an unspecified number of frames to reach the GAME prologue, so the ABSOLUTE frame a
# run ends on is a function of how long that took (measured: 27, while a note from an earlier build says
# 39). Asserting an absolute frame is therefore hardcoding an expected value — a bandaid that fails for
# a reason having nothing to do with the change under test. Assert the ADVANCE past the prologue, and
# the END STATE, which are the actual invariants.
PROLOGUE_RE = re.compile(r'reached GAME prologue at frame (\d+)')
ENDSTATE_RE = re.compile(r'stage=([0-9A-Fa-f]+)\s+sm48=(\d+)')
ENV_AUDIT_RE = re.compile(r'env audit AT EXIT[^\n]*')
# The AUTHORITATIVE unknown-knob verdict. The startup validator runs BEFORE late-initialising
# subsystems register their knobs, so it reports false UNKNOWNs (measured: PSXPORT_VK_HEADLESS,
# PSXPORT_REPL, PSXPORT_PAD_REPLAY). The framework's own exit audit is explicitly labelled
# "everything that was going to be read has been" — that is the number to gate on.
AUDIT_UNKNOWN_RE = re.compile(r'env audit AT EXIT[^\n]*?(\d+) UNKNOWN')


def refuse(msg: str) -> int:
    print(f"GATE REFUSED: {msg}", file=sys.stderr)
    return 2


def native_environment(watchdog: int, debug: str = '', extra_env: dict | None = None,
                       settings: str | None = None) -> dict:
    """The one headless REPL launch environment for the built tomba2_port binary.

    The launch-environment policy (psxport tools/port/launch_environment.py) owns the
    headless/silent/unpaced knobs; the disc and asset directory follow run.py's resolution.
    Every agent driver of the product (this gate, tools/oracle_compare.py) builds its
    environment here so they cannot drift apart.

    `settings` names the tracked .ini the run is gated with, defaulting to the shipping one. The
    picture oracle passes psxport's reference settings instead: it photographs the product against
    a 4:3 console, and a widescreen frame is a different SIZE, which the comparison refuses."""
    sys.path.insert(0, os.path.join(PSXPORT, 'tools'))
    from port.launch_environment import agent_environment
    env = agent_environment(dict(os.environ), settings or SHIPPING_SETTINGS)
    env['PSXPORT_ASSET_DIR'] = env.get('PSXPORT_ASSET_DIR') or PSXPORT
    env['PSXPORT_TOMBA2_DISC'] = resolve_disc(None, Path(REPO), env)
    env['PSXPORT_REPL'] = '1'
    env['PSXPORT_WATCHDOG'] = str(watchdog)
    if debug:
        env['PSXPORT_DEBUG'] = debug
    env.update(extra_env or {})
    return env


def run_gate(script: str, frames_hint: int, debug: str, watchdog: int,
             expect_frame: int, extra_env: dict, label: str,
             expect_stage: str = '', expect_sm48: str = '', settings: str | None = None) -> int:
    if not os.path.isfile(BIN):
        return refuse(f"{BIN} does not exist — NOTHING WAS RUN. Build first: "
                      f"cmake --build build --target tomba2_port -j$(nproc). Do not read this as a pass.")
    if not os.path.isfile(EXE):
        return refuse(f"{EXE} does not exist — NOTHING WAS RUN. The boot executable is extracted from "
                      f"the disc; it is not this tool's job to extract it (that is run.sh's, and run.sh "
                      f"belongs to the user). Extract it once, then re-run this gate.")

    env = native_environment(watchdog, debug, extra_env, settings)

    os.makedirs(LOGDIR, exist_ok=True)
    stamp = time.strftime('%Y%m%d-%H%M%S')
    logpath = os.path.join(LOGDIR, f'gate-{label}-{stamp}.log')

    # Identity before and after: another session can rebuild the shared tree mid-run.
    bin_id = binary_identity(Path(BIN))

    t0 = time.time()
    try:
        p = subprocess.run([BIN, EXE], input=script, capture_output=True, text=True,
                           env=env, cwd=REPO, timeout=watchdog + 120)
    except subprocess.TimeoutExpired:
        return refuse(f"the binary did not exit within {watchdog + 120}s even though PSXPORT_WATCHDOG="
                      f"{watchdog}. The watchdog itself did not fire — treat this as a HANG, not a pass.")
    dt = time.time() - t0
    out = (p.stdout or '') + (p.stderr or '')
    with open(logpath, 'w') as f:
        f.write(out)

    after = binary_identity(Path(BIN))
    swapped = after['md5'] != bin_id['md5']
    print(f"[gate:{label}] binary that ran: md5 {bin_id['md5'][:12]} mtime {bin_id['mtime']}"
          + (f"  <-- REPLACED MID-RUN (now md5 {after['md5'][:12]} mtime {after['mtime']}); another "
             f"session rebuilt the shared tree, so this run's observations belong to NO identifiable "
             f"build and must not be used as evidence" if swapped else ""))

    lines = out.count('\n')
    if lines == 0:
        return refuse(f"the binary produced ZERO output lines in {dt:.1f}s (exit {p.returncode}). "
                      f"Nothing was observed, so nothing is proven. Log: {logpath}")

    frames = [int(m.group(1)) for m in FRAME_RE.finditer(out)]
    maxframe = max(frames) if frames else -1
    mp = PROLOGUE_RE.search(out)
    prologue = int(mp.group(1)) if mp else -1
    me = ENDSTATE_RE.search(out)
    end_stage = me.group(1).upper() if me else ''
    end_sm48 = me.group(2) if me else ''
    audit = ENV_AUDIT_RE.search(out)

    hits = []
    for pat in FAIL_PATTERNS:
        for m in re.finditer(pat, out, re.IGNORECASE):
            ctx = out[max(0, m.start() - 100):m.end() + 140].replace('\n', ' | ')
            hits.append((pat, ctx))
            break

    # PSXPORT_REPL is read via the legacy cfg_on() path (native_boot.cpp:299) but is not registered in
    # config_vars.h, so the startup validator reports it UNKNOWN even though it works. Filtered so a
    # REAL unknown knob — a flag the caller is relying on that did nothing — is not lost in noise.
    # Verified rather than assumed: the REPL demonstrably drives the run (a `run N` advances the frame
    # counter by N past the newgame prologue).
    KNOWN_FALSE_UNKNOWN = {'PSXPORT_REPL'}
    unknown = sorted(set(UNKNOWN_KNOB.findall(out)) - KNOWN_FALSE_UNKNOWN)

    # ---- the report, always with its denominator ------------------------------------------------
    print(f"[gate:{label}] exit={p.returncode} in {dt:.1f}s · {lines} output line(s) · "
          f"max frame counter seen = {maxframe if maxframe >= 0 else 'NONE'}")
    print(f"[gate:{label}] scanned {lines} line(s) against {len(FAIL_PATTERNS)} failure pattern(s): "
          f"{', '.join(FAIL_PATTERNS)}")
    if prologue >= 0:
        print(f"[gate:{label}] newgame prologue at frame {prologue}; advanced "
              f"{maxframe - prologue} frame(s) past it"
              + (f" (asked for {frames_hint})" if frames_hint else ""))
    if end_stage:
        print(f"[gate:{label}] end state: stage={end_stage} sm48={end_sm48}")
    if audit:
        print(f"[gate:{label}] {audit.group(0).strip()}")
    print(f"[gate:{label}] log: {logpath}")
    bad = 0
    # Gate on the EXIT audit, not the startup warnings. A knob this gate passed that was truly never read
    # is a silent degradation — a replay applying no input still advances frames and still reads green —
    # so it must fail. But the STARTUP "UNKNOWN knob" lines are premature for any subsystem that
    # initialises after the validator, and treating those as failures produced a false FAIL on
    # PSXPORT_PAD_REPLAY while the exit audit reported 0 UNKNOWN for the same run.
    ma = AUDIT_UNKNOWN_RE.search(out)
    if ma is not None:
        n_unknown = int(ma.group(1))
        if n_unknown:
            print(f"[gate:{label}] FAIL — the exit env audit reports {n_unknown} UNKNOWN knob(s): a flag "
                  f"passed to this run was never read, so the run did not do what was asked. Startup "
                  f"UNKNOWN lines seen: {', '.join(unknown) if unknown else '(none)'}")
            bad = 1
    elif unknown:
        # No exit audit (the run died early), so the startup list is all there is — report, do not judge.
        print(f"[gate:{label}] note: startup reported {len(unknown)} unknown knob(s) and there is no exit "
              f"audit to confirm against (run ended early?): {', '.join(unknown)}")

    if hits:
        print(f"[gate:{label}] FAIL — {len(hits)} failure pattern(s) matched:")
        for pat, ctx in hits:
            print(f"    /{pat}/  …{ctx}…")
        bad = 1
    if maxframe < 0:
        print(f"[gate:{label}] FAIL — no frame counter ever appeared in the output, so the run cannot "
              f"be said to have advanced. This is not a pass.")
        bad = 1
    elif expect_frame and maxframe < expect_frame:
        print(f"[gate:{label}] FAIL — reached frame {maxframe}, expected at least {expect_frame}.")
        bad = 1
    # The real assertion: did the run actually ADVANCE the number of frames it was told to, past the
    # prologue? This is what an absolute frame floor was standing in for, without the magic constant.
    if frames_hint and prologue >= 0 and (maxframe - prologue) < frames_hint:
        print(f"[gate:{label}] FAIL — asked to advance {frames_hint} frame(s) past the prologue "
              f"(frame {prologue}) but only reached {maxframe}, i.e. {maxframe - prologue}.")
        bad = 1
    if expect_stage and end_stage != expect_stage.upper():
        print(f"[gate:{label}] FAIL — end stage {end_stage or 'NONE REPORTED'}, expected "
              f"{expect_stage.upper()}.")
        bad = 1
    if expect_sm48 and end_sm48 != expect_sm48:
        print(f"[gate:{label}] FAIL — end sm48 {end_sm48 or 'NONE REPORTED'}, expected {expect_sm48}.")
        bad = 1
    if p.returncode != 0 and not hits:
        print(f"[gate:{label}] FAIL — non-zero exit {p.returncode} with no failure pattern matched; "
              f"read the log, the pattern list is incomplete for this failure.")
        bad = 1

    if bad:
        return 1
    print(f"[gate:{label}] PASS — launched headless, REPL accepted the script, advanced "
          f"{maxframe - prologue if prologue >= 0 else maxframe} frame(s)"
          + (f" to stage={end_stage} sm48={end_sm48}" if end_stage else "")
          + f", no failure pattern matched. This says NOTHING about pixels or SBS parity.")
    return 0


def cmd_boot(args) -> int:
    script = f"newgame\nrun {args.frames}\nstage\nquit\n"
    return run_gate(script, args.frames, args.debug, args.watchdog, args.expect_frame, {}, 'boot',
                    args.expect_stage, args.expect_sm48)


def replay_environment(pad_arg: str) -> dict | None:
    """The pad-replay knob for a recorded pad path (repo-relative or absolute); None when it is missing.

    A replay whose knob name is wrong degrades to a PLAIN RUN — same frame count, same green, no input.
    The gate's unknown-knob line is what caught exactly that (PSXPORT_SBS_PAD_REPLAY did not exist), so
    an unaccepted knob is a hard failure rather than a pass over no input.
    """
    pad = pad_arg if os.path.isabs(pad_arg) else os.path.join(REPO, pad_arg)
    if not os.path.isfile(pad):
        refuse(f"replay {pad} does not exist — NOTHING WAS RUN.")
        return None
    return {'PSXPORT_PAD_REPLAY': pad}


def cmd_replay(args) -> int:
    env = replay_environment(args.pad)
    if env is None:
        return 2
    script = f"newgame\nrun {args.frames}\nquit\n"
    return run_gate(script, args.frames, args.debug, args.watchdog, args.expect_frame, env, 'replay')


def cmd_run(args) -> int:
    if not args.script:
        return refuse("--script is empty — NOTHING WAS RUN.")
    env = replay_environment(args.replay) if args.replay else {}
    if env is None:
        return 2
    return run_gate(args.script if args.script.endswith('\n') else args.script + '\n',
                    0, args.debug, args.watchdog, args.expect_frame, env, 'run')


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0],
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--debug', default='', help='PSXPORT_DEBUG channels, e.g. nofx,plumefx')
    ap.add_argument('--watchdog', type=int, default=120, help='PSXPORT_WATCHDOG seconds (default 120)')
    ap.add_argument('--expect-frame', type=int, default=0,
                    help='fail unless the ABSOLUTE frame counter reaches this. Prefer the defaults: the '
                         'gate already asserts the advance past the prologue, which is the same '
                         'invariant without a magic constant')
    ap.add_argument('--expect-stage', default='',
                    help='fail unless the run ends on this task-0 stage entry, e.g. 8010637C')
    ap.add_argument('--expect-sm48', default='',
                    help='fail unless the run ends with this stage state-machine value, e.g. 2')
    sub = ap.add_subparsers(dest='cmd', required=True)
    b = sub.add_parser('boot', help='newgame + run N frames headless')
    b.add_argument('--frames', type=int, default=400)
    b.set_defaults(fn=cmd_boot)
    r = sub.add_parser('replay', help='run a recorded pad replay headless')
    r.add_argument('pad')
    r.add_argument('--frames', type=int, default=900)
    r.set_defaults(fn=cmd_replay)
    x = sub.add_parser('run', help='drive an arbitrary REPL script')
    x.add_argument('--script', required=True)
    x.add_argument('--replay', default='', help='drive the script under this recorded pad replay')
    x.set_defaults(fn=cmd_run)
    args = ap.parse_args()
    return args.fn(args)


if __name__ == '__main__':
    sys.exit(main())
