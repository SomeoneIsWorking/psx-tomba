#!/usr/bin/env python3
"""Extract the exact Tomba! 2 runtime images from a user-supplied disc."""

from __future__ import annotations

import argparse
import hashlib
import json
import pathlib
import shutil
import subprocess
import sys
from collections.abc import Callable, Mapping, Sequence
from typing import NamedTuple

ROOT = pathlib.Path(__file__).resolve().parents[1]
DEFAULT_OUTPUT = ROOT / "scratch/bin/tomba2"
DEFAULT_OVERLAYS = ROOT / "scratch/bin/overlays"

MANIFEST = ROOT / "config/tomba2-images.json"


class ImageIdentity(NamedTuple):
    size: int
    sha256: str


def load_images(path: pathlib.Path = MANIFEST) -> Mapping[str, ImageIdentity]:
    manifest = json.loads(path.read_text(encoding="utf-8"))
    return {
        name: ImageIdentity(item["size"], item["sha256"])
        for name, item in manifest["images"].items()
    }


IMAGES = load_images()


class ProvisionError(RuntimeError):
    """The selected disc could not supply the complete target image set."""


def destination_for(
    disc_path: str, executable_root: pathlib.Path, overlay_root: pathlib.Path
) -> pathlib.Path:
    name = pathlib.PurePosixPath(disc_path).name
    return overlay_root / name if disc_path.startswith("BIN/") else executable_root / name


def validate_image(path: pathlib.Path, disc_path: str, identity: ImageIdentity) -> None:
    try:
        with path.open("rb") as source:
            data = source.read(identity.size + 1)
    except OSError as exc:
        raise ProvisionError(f"cannot inspect extracted {disc_path}: {exc}") from exc
    if len(data) != identity.size:
        actual_size = f"more than {identity.size}" if len(data) > identity.size else str(len(data))
        raise ProvisionError(
            f"{disc_path} is {actual_size} bytes; expected {identity.size} for the selected title"
        )
    digest = hashlib.sha256(data).hexdigest()
    if digest != identity.sha256:
        raise ProvisionError(
            f"{disc_path} SHA-256 {digest} does not match supported Tomba! 2 USA "
            f"image {identity.sha256}"
        )



def extract_image(
    discdump: pathlib.Path,
    disc: pathlib.Path,
    disc_path: str,
    destination: pathlib.Path,
) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    completed = subprocess.run(
        [str(discdump), "get", disc_path, str(disc), str(destination.parent)],
        check=False,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.PIPE,
    )
    if completed.returncode != 0 or not destination.is_file():
        diagnostic = completed.stderr.decode(errors="replace").strip()
        suffix = f": {diagnostic}" if diagnostic else ""
        raise ProvisionError(f"discdump could not extract {disc_path}{suffix}")


def provision(
    discdump: pathlib.Path,
    disc: pathlib.Path,
    executable_root: pathlib.Path = DEFAULT_OUTPUT,
    overlay_root: pathlib.Path = DEFAULT_OVERLAYS,
    *,
    images: Mapping[str, ImageIdentity] = IMAGES,
    extractor: Callable[[pathlib.Path, pathlib.Path, str, pathlib.Path], None] = extract_image,
) -> tuple[pathlib.Path, ...]:
    if not disc.is_file():
        raise ProvisionError(f"disc image does not exist: {disc}")
    if not discdump.is_file():
        raise ProvisionError(f"discdump executable does not exist: {discdump}")

    if not images:
        raise ProvisionError("runtime-image manifest is empty; cannot authenticate the selected disc")

    # Re-extract from the selected disc even when cached files exist. Otherwise a
    # valid cache can authenticate an unrelated disc whose streaming assets differ.
    staging = executable_root / ".provisioning"
    try:
        executable_root.mkdir(parents=True, exist_ok=True)
        staging.mkdir()
    except OSError as exc:
        raise ProvisionError(f"cannot acquire provisioning staging directory {staging}: {exc}") from exc

    outputs: list[pathlib.Path] = []
    staged: list[pathlib.Path] = []
    try:
        for disc_path, identity in images.items():
            candidate = destination_for(disc_path, staging, staging / "overlays")
            extractor(discdump, disc, disc_path, candidate)
            validate_image(candidate, disc_path, identity)
            staged.append(candidate)
            outputs.append(destination_for(disc_path, executable_root, overlay_root))

        # No published image changes until the complete selected set is authenticated.
        # Publication is atomic per file, not across the set. Every previously
        # valid image has this same immutable manifest identity, so interruption
        # cannot mix valid revisions. A publication error still refuses launch;
        # repairing an invalid cache may remain incomplete until the next run.
        for candidate, destination in zip(staged, outputs, strict=True):
            destination.parent.mkdir(parents=True, exist_ok=True)
            candidate.replace(destination)
    except OSError as exc:
        raise ProvisionError(f"cannot provision runtime images: {exc}") from exc
    finally:
        shutil.rmtree(staging)
    return tuple(outputs)


def parse_args(arguments: Sequence[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("disc", type=pathlib.Path)
    parser.add_argument("--discdump", required=True, type=pathlib.Path)
    return parser.parse_args(arguments)


def main(arguments: Sequence[str] | None = None) -> int:
    args = parse_args(arguments)
    try:
        outputs = provision(args.discdump, args.disc)
    except ProvisionError as exc:
        print(f"REFUSED: {exc}", file=sys.stderr)
        return 1
    print(f"AUTHENTICATED AND PROVISIONED: {len(outputs)}/{len(IMAGES)} runtime images")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
