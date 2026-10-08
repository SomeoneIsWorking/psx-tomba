#!/usr/bin/env python3
"""Compare the PICTURE the Tomba! 2 product presents against psxport's independent Beetle
full-console reference, at the same title-owned states tools/oracle_compare.py compares RAM at.

    uv run --frozen python tools/picture_oracle.py --bios ../SCPH1001.BIN
    uv run --frozen python tools/picture_oracle.py --bios ../SCPH1001.BIN --selftest

tools/oracle_compare.py answers "does the simulation still behave"; it reads guest RAM and cannot
see a rendering defect. This answers "does it still LOOK like the game", which is the question a
player is actually asking. The title policy is shared with the RAM comparison (tools/oracle_tomba2.py)
and the launch environment with every other agent driver (tools/gate.py).
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
import picture  # noqa: E402

REPO = Path(gate.REPO)
OUT_DIR = REPO / "scratch" / "picture"
DEFAULT_BIOS = REPO.parent / "SCPH1001.BIN"


def main() -> int:
    parser = picture.picture_parser(__doc__, DEFAULT_BIOS)
    parser.add_argument("--watchdog", type=int, default=3600, help="product watchdog seconds")
    args = parser.parse_args()
    # Not tools/shipping_settings.ini: the picture question needs 4:3 and no interpolation, and
    # picture.py owns that configuration because it is the comparator's requirement, not Tomba! 2's.
    environment = gate.native_environment(args.watchdog, extra_env=compare.product_env(args),
                                          settings=str(picture.REFERENCE_SETTINGS))
    product = compare.Product(Path(gate.BIN), Path(gate.EXE), environment, REPO,
                              Path(environment["PSXPORT_TOMBA2_DISC"]))
    return picture.run(title, product, args, OUT_DIR)


if __name__ == "__main__":
    raise SystemExit(main())
