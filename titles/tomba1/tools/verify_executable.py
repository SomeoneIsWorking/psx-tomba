"""Verify the selected Tomba! USA executable against its tracked identity facts.

Exit codes: 0 agreement, 1 disagreement, 2 refusal because no assertion was possible.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import pathlib
import struct
import tempfile
from collections.abc import Mapping
from dataclasses import dataclass

TITLE_ROOT = pathlib.Path(__file__).resolve().parents[1]
REPOSITORY_ROOT = TITLE_ROOT.parents[1]
MANIFEST_PATH = TITLE_ROOT / "executable.json"
PS_EXE_HEADER_SIZE = 0x800
PS_EXE_MAGIC = b"PS-X EXE"
REGION_OFFSET = 0x4C
REGION_SIZE = 0x4C

HEADER_FIELDS = {
    "entry": 0x10,
    "global_pointer": 0x14,
    "text_address": 0x18,
    "text_size": 0x1C,
    "data_address": 0x20,
    "data_size": 0x24,
    "bss_address": 0x28,
    "bss_size": 0x2C,
    "stack_address": 0x30,
    "stack_size": 0x34,
}


class Refused(RuntimeError):
    """The input cannot support an executable-identity assertion."""


@dataclass(frozen=True)
class Verification:
    checks: int
    failures: tuple[str, ...]


def integer(value: object, label: str) -> int:
    if isinstance(value, int):
        return value
    if isinstance(value, str):
        try:
            return int(value, 0)
        except ValueError as exc:
            raise Refused(f"manifest {label} is not an integer: {value!r}") from exc
    raise Refused(f"manifest {label} is not an integer")


def load_manifest(path: pathlib.Path = MANIFEST_PATH) -> dict[str, object]:
    try:
        manifest = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise Refused(f"cannot read executable manifest {path}: {exc}") from exc
    if not isinstance(manifest, dict):
        raise Refused("executable manifest root is not an object")
    return manifest


def header_word(image: bytes, offset: int) -> int:
    return struct.unpack_from("<I", image, offset)[0]


def verify_bytes(
    image: bytes,
    filename: str,
    manifest: Mapping[str, object],
) -> Verification:
    if len(image) < PS_EXE_HEADER_SIZE:
        raise Refused(
            f"{filename} is {len(image)} byte(s), shorter than the 0x800-byte PS-X EXE header"
        )

    failures: list[str] = []
    checks = 0

    def compare(label: str, actual: object, expected: object) -> None:
        nonlocal checks
        checks += 1
        if actual != expected:
            failures.append(f"{label}: expected {expected!r}, got {actual!r}")

    expected_name = manifest.get("output_name")
    if not isinstance(expected_name, str) or not expected_name:
        raise Refused("manifest output_name is absent")
    compare("filename", filename, expected_name)
    compare("PS-X EXE magic", image[:8], PS_EXE_MAGIC)
    compare("file size", len(image), integer(manifest.get("file_size"), "file_size"))

    expected_sha1 = manifest.get("sha1")
    if not isinstance(expected_sha1, str) or len(expected_sha1) != 40:
        raise Refused("manifest sha1 is absent or malformed")
    compare("SHA-1", hashlib.sha1(image).hexdigest(), expected_sha1.lower())

    fields = manifest.get("ps_exe")
    if not isinstance(fields, dict):
        raise Refused("manifest ps_exe group is absent")
    for name, offset in HEADER_FIELDS.items():
        compare(
            name,
            header_word(image, offset),
            integer(fields.get(name), f"ps_exe.{name}"),
        )

    expected_region = manifest.get("region_marker")
    if not isinstance(expected_region, str) or not expected_region:
        raise Refused("manifest region_marker is absent")
    region = image[REGION_OFFSET : REGION_OFFSET + REGION_SIZE].split(b"\0", 1)[0]
    compare("region marker", region.decode("ascii", errors="replace"), expected_region)

    return Verification(checks=checks, failures=tuple(failures))


def verify_path(path: pathlib.Path, manifest: Mapping[str, object]) -> Verification:
    try:
        image = path.read_bytes()
    except OSError as exc:
        raise Refused(f"cannot read executable {path}: {exc}") from exc
    return verify_bytes(image, path.name, manifest)


def fixture_image() -> bytes:
    image = bytearray(PS_EXE_HEADER_SIZE + 0x100)
    image[:8] = PS_EXE_MAGIC
    values = {
        "entry": 0x80010100,
        "global_pointer": 0,
        "text_address": 0x80010000,
        "text_size": 0x100,
        "data_address": 0,
        "data_size": 0,
        "bss_address": 0,
        "bss_size": 0,
        "stack_address": 0x801FFFF0,
        "stack_size": 0,
    }
    for name, offset in HEADER_FIELDS.items():
        struct.pack_into("<I", image, offset, values[name])
    region = b"Synthetic North America fixture"
    image[REGION_OFFSET : REGION_OFFSET + len(region)] = region
    image[PS_EXE_HEADER_SIZE:] = bytes(range(256))
    return bytes(image)


def fixture_manifest(image: bytes) -> dict[str, object]:
    return {
        "output_name": "SCUS_TEST.00",
        "file_size": len(image),
        "sha1": hashlib.sha1(image).hexdigest(),
        "ps_exe": {
            name: f"0x{header_word(image, offset):08X}"
            for name, offset in HEADER_FIELDS.items()
        },
        "region_marker": "Synthetic North America fixture",
    }


def run_selftest(real_executable: pathlib.Path | None) -> int:
    checks = 0
    failed = 0

    def expect(label: str, condition: bool) -> None:
        nonlocal checks, failed
        checks += 1
        if condition:
            print(f"PASS: {label}")
        else:
            failed += 1
            print(f"FAIL: {label}")

    image = fixture_image()
    manifest = fixture_manifest(image)
    expect(
        "shipping verifier accepts an agreeing fixture",
        not verify_bytes(image, "SCUS_TEST.00", manifest).failures,
    )

    changed_payload = bytearray(image)
    changed_payload[-1] ^= 0xFF
    result = verify_bytes(bytes(changed_payload), "SCUS_TEST.00", manifest)
    expect(
        "payload mutation produces the opposite SHA-1 answer",
        any("SHA-1" in item for item in result.failures),
    )

    changed_entry = bytearray(image)
    struct.pack_into("<I", changed_entry, HEADER_FIELDS["entry"], 0x80010200)
    result = verify_bytes(bytes(changed_entry), "SCUS_TEST.00", manifest)
    expect(
        "header mutation produces the opposite entry answer",
        any("entry" in item for item in result.failures),
    )

    result = verify_bytes(image, "SLUS_WRONG.00", manifest)
    expect(
        "renamed input produces the opposite filename answer",
        any("filename" in item for item in result.failures),
    )

    try:
        verify_bytes(image[:0x40], "SCUS_TEST.00", manifest)
    except Refused:
        expect("short input refuses instead of claiming mismatch-free", True)
    else:
        expect("short input refuses instead of claiming mismatch-free", False)

    if real_executable is not None:
        real_manifest = load_manifest()
        try:
            real_bytes = real_executable.read_bytes()
        except OSError as exc:
            print(f"REFUSED: cannot read real selftest executable: {exc}")
            return 2
        real_result = verify_bytes(real_bytes, real_executable.name, real_manifest)
        expect(
            f"measured {real_executable.name} agrees on all {real_result.checks} facts",
            not real_result.failures,
        )

        scratch_root = pathlib.Path(
            os.environ.get("TOMBA1_SCRATCH", REPOSITORY_ROOT / "scratch" / "tomba1")
        )
        scratch_root.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(
            prefix="identity-selftest-", dir=scratch_root
        ) as temporary:
            altered = bytearray(real_bytes)
            altered[-1] ^= 0x01
            altered_path = pathlib.Path(temporary) / real_executable.name
            altered_path.write_bytes(altered)
            altered_result = verify_path(altered_path, real_manifest)
            expect(
                "one changed byte in the measured executable produces the opposite SHA-1 answer",
                any("SHA-1" in item for item in altered_result.failures),
            )

    print(f"SELFTEST: {checks - failed}/{checks} checks passed")
    return 1 if failed else 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--check", metavar="EXECUTABLE", type=pathlib.Path)
    mode.add_argument("--selftest", action="store_true")
    parser.add_argument(
        "--executable",
        type=pathlib.Path,
        help="real SCUS_942.36 to add positive and altered-byte cases to --selftest",
    )
    args = parser.parse_args()

    if args.selftest:
        return run_selftest(args.executable)
    if args.executable is not None:
        parser.error("--executable is valid only with --selftest")

    try:
        manifest = load_manifest()
        result = verify_path(args.check, manifest)
    except Refused as exc:
        print(f"REFUSED: {exc}")
        return 2

    if result.failures:
        print(
            f"MISMATCH: {len(result.failures)} of {result.checks} identity fact(s) disagree"
        )
        for failure in result.failures:
            print(f"  - {failure}")
        return 1
    print(f"MATCH: {result.checks}/{result.checks} executable identity facts agree")
    print(
        "blind spot: executable identity does not prove SYSTEM.CNF selection, boot, a frame, or gameplay"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
