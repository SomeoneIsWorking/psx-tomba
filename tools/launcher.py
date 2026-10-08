"""Select a title product before any host or asset discovery."""

from __future__ import annotations

import sys
from collections.abc import Sequence

HELP_ARGUMENTS = {"-h", "--help"}
TITLE_SELECTORS = {"tomba1", "tomba2"}


def print_usage(title: str | None = None) -> None:
    if title == "tomba1":
        print(
            "Usage: ./run.sh tomba1 [disc.chd]\n"
            "Build and launch the verified Tomba! USA product.\n\n"
            "Options:\n"
            "  -h, --help  Show this help and exit"
        )
        return
    if title == "tomba2":
        print(
            "Usage: ./run.sh tomba2 [--resume [recording.pad]] [disc.chd]\n"
            "Build and launch the Tomba! 2 native PC product.\n\n"
            "Options:\n"
            "  -h, --help  Show this help and exit"
        )
        return
    print(
        "Usage: ./run.sh [tomba1|tomba2] [product options]\n"
        "       ./run.sh [disc.chd]\n\n"
        "Products:\n"
        "  tomba1  Tomba! USA (separate title engine)\n"
        "  tomba2  Tomba! 2 native PC port (default)\n\n"
        "Options:\n"
        "  -h, --help  Show this help and exit"
    )


def select_product(arguments: Sequence[str]) -> tuple[str, list[str]]:
    remaining = list(arguments)
    if remaining and remaining[0] in TITLE_SELECTORS:
        return remaining.pop(0), remaining
    return "tomba2", remaining


def main(arguments: Sequence[str] | None = None) -> int:
    args = list(sys.argv[1:] if arguments is None else arguments)
    if args and args[0] in HELP_ARGUMENTS:
        print_usage()
        return 0

    title, product_args = select_product(args)
    if product_args and product_args[0] in HELP_ARGUMENTS:
        print_usage(title)
        return 0

    if title == "tomba1":
        from titles.tomba1.tools.run import main as run_tomba1

        return run_tomba1(product_args)

    from tools.run import main as run_tomba2

    return run_tomba2(product_args)


if __name__ == "__main__":
    raise SystemExit(main())
