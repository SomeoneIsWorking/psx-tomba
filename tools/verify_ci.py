#!/usr/bin/env python3
"""Run the complete asset-free Linux native/dynarec repository gate."""

from __future__ import annotations

import argparse
import sys
from collections.abc import Sequence
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
# The framework's own tools live in the checkout tools/psxport_fetch.py resolved. This insertion
# precedes the import below and uses the same external/psxport/tools resolution as the rest of this
# file.
PSXPORT = (ROOT / "external/psxport").resolve()
sys.path.insert(0, str(PSXPORT / "tools"))

from port.consumer_verify import ConsumerVerifyConfig, run_consumer_verification  # noqa: E402

# This repository's canonical maintainer build directory. It is a property of THIS repository's
# layout, so it is owned here.
CANONICAL_VERIFY_BUILD = ROOT / "build" / "ci"
DEFAULT_BUILD = Path(CANONICAL_VERIFY_BUILD)


def main(arguments: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, default=DEFAULT_BUILD)
    args = parser.parse_args(arguments)
    build = args.build.resolve()
    result = run_consumer_verification(
        ConsumerVerifyConfig(
            name="Tomba native/Lightrec products",
            root=ROOT,
            build=build,
            psxport=PSXPORT,
            product=build / "bin/tomba2_port",
            cmake_module=ROOT / "cmake/tomba2_port.cmake",
            test_regex=".",
            cmake_definitions=(
                "-DBUILD_TESTING=ON",
                "-DPSXPORT_BUILD_PORT=ON",
                "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
            ),
            python=Path(sys.executable),
        )
    )
    return result


if __name__ == "__main__":
    raise SystemExit(main())
