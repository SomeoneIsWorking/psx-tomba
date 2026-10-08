#!/usr/bin/env python3
"""Compare the Tomba! 2 Lightrec product against psxport's independent Beetle full-console
reference at title-owned state checkpoints (psxport docs/oracle.md, tools/oracle/compare.py).

    uv run --frozen python tools/oracle_compare.py --bios ../SCPH1001.BIN
    uv run --frozen python tools/oracle_compare.py --bios ../SCPH1001.BIN --selftest

The title policy lives in tools/oracle_tomba2.py — which takes the route's DECISIONS (which button a
screen wants, at what cadence) from tools/title_prompts.py, the module tools/live_play.py asks too — and
the product launch environment comes from tools/gate.py so every agent driver of the product builds it in
one place.
"""

from __future__ import annotations

import os
import sys
from pathlib import Path

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import gate  # noqa: E402  (tools/gate.py owns the product launch environment)

sys.path.insert(0, os.path.join(gate.PSXPORT, "tools", "oracle"))

import compare  # noqa: E402
import oracle_tomba2 as title  # noqa: E402

REPO = Path(gate.REPO)
OUT_DIR = REPO / "scratch" / "oracle"
DEFAULT_BIOS = REPO.parent / "SCPH1001.BIN"


def main() -> int:
    parser = compare.build_parser(__doc__, DEFAULT_BIOS)
    parser.add_argument("--watchdog", type=int, default=3600, help="product watchdog seconds")
    args = parser.parse_args()
    environment = gate.native_environment(args.watchdog, extra_env=compare.product_env(args))
    product = compare.Product(Path(gate.BIN), Path(gate.EXE), environment, REPO,
                              Path(environment["PSXPORT_TOMBA2_DISC"]))
    return compare.run(title, product, args, OUT_DIR)


if __name__ == "__main__":
    raise SystemExit(main())
