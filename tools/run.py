#!/usr/bin/env python3
"""Build and launch the Tomba! and Tomba! 2 native PC product (in-window title picker).

Each title's disc comes from PSXPORT_TOMBA1_DISC / PSXPORT_TOMBA2_DISC (environment or .env).
"""

from __future__ import annotations

import os
import shutil
from collections.abc import Sequence
from pathlib import Path

from automation.process import ToolError
from port import launcher
from port.launch_environment import player_environment

ROOT = Path(__file__).resolve().parents[1]
SESSION_PAD = Path("scratch/bin/pad_session.pad")
RESUME_PAD = Path("scratch/bin/pad_resume.pad")


def resume_recording(requested: str) -> Path:
    """The pad recording to fast-forward through; empty means the last windowed session."""
    if not requested:
        if not (ROOT / SESSION_PAD).is_file():
            raise ToolError(f"--resume: no recording at {SESSION_PAD} yet; play a windowed session first, "
                            "or pass a .pad")
        (ROOT / RESUME_PAD).parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(ROOT / SESSION_PAD, ROOT / RESUME_PAD)
        requested = str(RESUME_PAD)
    recording = Path(requested)
    if not (recording if recording.is_absolute() else ROOT / recording).is_file():
        raise ToolError(f"--resume: no such recording: {recording}")
    return recording


def launch(resume: str | None, prepare_only: bool) -> None:
    launcher.preflight()
    build = launcher.build_product(ROOT, "tomba2_port", os.environ)
    if prepare_only:
        launcher.say("Tomba is built and ready.")
        return
    env = player_environment(os.environ, product="tomba2")
    env.setdefault("PSXPORT_DEBUG_SERVER", "1")
    env.setdefault("PSXPORT_NO_TERRAIN", "0")
    if resume is not None:
        recording = resume_recording(resume)
        launcher.say(f"resume: {recording}; fast-forwarding, then it is yours")
        env["PSXPORT_PAD_RESUME"] = str(recording)
    launcher.exec_product(build / "bin" / "tomba2_port", [], env, ROOT)


def main(argv: Sequence[str] | None = None) -> int:
    parser = launcher.argument_parser(__doc__.splitlines()[0], disc=False)
    parser.add_argument("--resume", nargs="?", const="", default=None, metavar="RECORDING.pad",
                        help="fast-forward through a pad recording (default: the last session), then play")
    args = parser.parse_args(argv)
    return launcher.main(lambda: launch(args.resume, args.prepare_only))


if __name__ == "__main__":
    raise SystemExit(main())
