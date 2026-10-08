"""Refuse the deferred Tomba! product until its typed Lightrec scheduler exists."""

from __future__ import annotations

import sys
from collections.abc import Sequence
from pathlib import Path

from tools import run as common

ROOT = Path(__file__).resolve().parents[3]
HELP_ARGUMENTS = {"-h", "--help"}


def print_usage() -> None:
    print(
        "Usage: ./run.sh tomba1 [disc.chd]\n"
        "Build and launch the Tomba! USA native/Lightrec product.\n\n"
        "Options:\n"
        "  -h, --help  Show this help and exit"
    )


def main(arguments: Sequence[str] | None = None, *, root: Path = ROOT) -> int:
    del root
    args = list(sys.argv[1:] if arguments is None else arguments)
    if args and args[0] in HELP_ARGUMENTS:
        print_usage()
        return 0
    if len(args) > 1 or (args and args[0].startswith("-")):
        print_usage()
        return 2

    print(
        f"{common.RED}[run] error:{common.RESET} Tomba! product unavailable: "
        "the generated executor was removed and the typed Lightrec task scheduler is not yet "
        "complete; use titles/tomba1/tools/provision.py only for authenticated asset provisioning",
        file=sys.stderr,
    )
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
