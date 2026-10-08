"""Provision the selected Tomba! USA executable from a user-supplied disc.

Exit codes: 0 provisioned, 1 selected-disc/identity mismatch, 2 refusal because
the disc, extractor, or boot declaration could not support an assertion.
"""

from __future__ import annotations

import argparse
import os
import pathlib
import re
import subprocess
import tempfile
from collections.abc import Callable, Mapping, Sequence
from dataclasses import dataclass

try:
    from . import verify_executable
except ImportError:
    import verify_executable

TITLE_ROOT = pathlib.Path(__file__).resolve().parents[1]
REPOSITORY_ROOT = TITLE_ROOT.parents[1]
DEFAULT_OUTPUT = REPOSITORY_ROOT / "scratch/bin/tomba1/SCUS_942.36"
DEFAULT_STAGING_ROOT = REPOSITORY_ROOT / "scratch/tomba1"
DISC_ENV = "PSXPORT_TOMBA1_DISC"
DISCDUMP_ENV = "PSXPORT_DISCDUMP"
SYSTEM_CNF = "SYSTEM.CNF"
BOOT_PATTERN = re.compile(
    r"^\s*BOOT\s*=\s*cdrom\s*:\s*([^\r\n]+?)\s*$", re.IGNORECASE | re.MULTILINE
)


class Refused(RuntimeError):
    """The available inputs cannot support a disc-provenance assertion."""


class Mismatch(RuntimeError):
    """The selected disc or executable disagrees with the tracked title."""


@dataclass(frozen=True)
class DiscSelection:
    path: pathlib.Path
    source: str


@dataclass(frozen=True)
class Provisioned:
    boot_path: str
    identity_checks: int
    output: pathlib.Path


Extractor = Callable[[str, pathlib.Path, pathlib.Path], pathlib.Path]


def env_file_value(path: pathlib.Path, key: str) -> str:
    if not path.is_file():
        return ""
    pattern = re.compile(rf"^\s*{re.escape(key)}\s*=\s*(.*?)\s*$")
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        match = pattern.match(line)
        if not match:
            continue
        value = match.group(1)
        if len(value) >= 2 and value[0] == value[-1] and value[0] in "\"'":
            value = value[1:-1]
        return value
    return ""


def rooted_path(value: str | pathlib.Path, root: pathlib.Path) -> pathlib.Path:
    path = pathlib.Path(value).expanduser()
    return path if path.is_absolute() else root / path


def resolve_disc(
    argument: str | None,
    root: pathlib.Path = REPOSITORY_ROOT,
    environ: Mapping[str, str] = os.environ,
) -> DiscSelection:
    candidates = (
        (argument or "", "command line"),
        (environ.get(DISC_ENV, ""), DISC_ENV),
        (env_file_value(root / ".env", DISC_ENV), f"{root / '.env'}:{DISC_ENV}"),
    )
    for value, source in candidates:
        if not value:
            continue
        path = rooted_path(value, root)
        if not path.is_file():
            raise Refused(f"{source} selects missing disc image {path}")
        return DiscSelection(path=path, source=source)

    drops = sorted(
        path
        for path in root.iterdir()
        if path.is_file() and path.suffix.casefold() == ".chd"
    )
    if len(drops) == 1:
        return DiscSelection(path=drops[0], source="single repository-root CHD")
    if len(drops) > 1:
        names = ", ".join(path.name for path in drops)
        raise Refused(
            f"{len(drops)} repository-root CHDs are ambiguous ({names}); pass one explicitly or set {DISC_ENV}"
        )
    raise Refused(
        f"no disc image: pass one as the positional argument, set {DISC_ENV}, add it to .env, "
        "or place exactly one *.chd in the repository root"
    )


def normalize_boot_path(system_cnf: bytes) -> str:
    text = system_cnf.decode("ascii", errors="replace")
    matches = BOOT_PATTERN.findall(text)
    if len(matches) != 1:
        raise Refused(
            f"SYSTEM.CNF must contain exactly one cdrom BOOT declaration; found {len(matches)}"
        )
    value = matches[0].strip().replace("\\", "/")
    value = value.lstrip("/")
    value = re.sub(r";[0-9]+$", "", value)
    if not value or value.startswith("../") or "/../" in value:
        raise Refused(f"SYSTEM.CNF BOOT path is unsafe or empty: {matches[0]!r}")
    return value


def locate_discdump(
    argument: pathlib.Path | None,
    root: pathlib.Path = REPOSITORY_ROOT,
    environ: Mapping[str, str] = os.environ,
) -> pathlib.Path:
    values: list[tuple[pathlib.Path, str]] = []
    if argument is not None:
        values.append((rooted_path(argument, root), "--discdump"))
    if environ.get(DISCDUMP_ENV):
        values.append((rooted_path(environ[DISCDUMP_ENV], root), DISCDUMP_ENV))
    values.extend(
        (root / relative, str(relative))
        for relative in (
            pathlib.Path("build/tools/discdump"),
            pathlib.Path("build/tools/discdump.exe"),
            pathlib.Path("external/psxport/build/tools/discdump"),
            pathlib.Path("external/psxport/build/tools/discdump.exe"),
        )
    )
    for path, source in values:
        if path.is_file() and os.access(path, os.X_OK):
            return path
        if source in ("--discdump", DISCDUMP_ENV):
            raise Refused(f"{source} selects missing or non-executable discdump {path}")
    raise Refused(
        f"discdump is unavailable; pass --discdump or set {DISCDUMP_ENV} to the framework extractor"
    )


def discdump_extractor(discdump: pathlib.Path) -> Extractor:
    def extract(
        disc_path: str, disc: pathlib.Path, destination: pathlib.Path
    ) -> pathlib.Path:
        destination.mkdir(parents=True, exist_ok=True)
        result = subprocess.run(
            [str(discdump), "get", disc_path, str(disc), str(destination)],
            check=False,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.PIPE,
        )
        output = destination / pathlib.PurePosixPath(disc_path).name
        if result.returncode != 0 or not output.is_file():
            diagnostic = result.stderr.decode(errors="replace").strip()
            detail = f": {diagnostic}" if diagnostic else ""
            raise Refused(f"discdump could not extract {disc_path}{detail}")
        return output

    return extract


def provision(
    disc: pathlib.Path,
    output: pathlib.Path,
    extractor: Extractor,
    manifest: Mapping[str, object],
    staging_root: pathlib.Path = DEFAULT_STAGING_ROOT,
) -> Provisioned:
    expected_disc_path = manifest.get("disc_executable")
    expected_output_name = manifest.get("output_name")
    if not isinstance(expected_disc_path, str) or not expected_disc_path:
        raise Refused("manifest disc_executable is absent")
    if not isinstance(expected_output_name, str) or not expected_output_name:
        raise Refused("manifest output_name is absent")
    if output.name != expected_output_name:
        raise Refused(f"output must be named {expected_output_name}, got {output.name}")

    staging_root.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(
        prefix="provision-", dir=staging_root
    ) as temporary:
        stage = pathlib.Path(temporary)
        system_path = extractor(SYSTEM_CNF, disc, stage / "system")
        try:
            boot_path = normalize_boot_path(system_path.read_bytes())
        except OSError as exc:
            raise Refused(f"cannot read extracted SYSTEM.CNF: {exc}") from exc
        if boot_path.casefold() != expected_disc_path.casefold():
            raise Mismatch(
                f"SYSTEM.CNF boots {boot_path!r}, expected root executable {expected_disc_path!r}"
            )

        executable = extractor(boot_path, disc, stage / "executable")
        try:
            identity = verify_executable.verify_path(executable, manifest)
        except verify_executable.Refused as exc:
            raise Refused(str(exc)) from exc
        if identity.failures:
            details = "\n".join(f"  - {failure}" for failure in identity.failures)
            raise Mismatch(
                f"{len(identity.failures)} of {identity.checks} executable identity fact(s) disagree:\n{details}"
            )

        output.parent.mkdir(parents=True, exist_ok=True)
        os.replace(executable, output)
        return Provisioned(
            boot_path=boot_path,
            identity_checks=identity.checks,
            output=output,
        )


def parse_args(argv: Sequence[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("disc", nargs="?", help="selected Tomba! disc image")
    parser.add_argument(
        "--discdump", type=pathlib.Path, help="framework discdump executable"
    )
    parser.add_argument("--output", type=pathlib.Path, default=DEFAULT_OUTPUT)
    return parser.parse_args(argv)


def main(argv: Sequence[str] | None = None) -> int:
    args = parse_args(argv)
    try:
        selection = resolve_disc(args.disc)
        discdump = locate_discdump(args.discdump)
        manifest = verify_executable.load_manifest()
        output = rooted_path(args.output, REPOSITORY_ROOT)
        result = provision(
            selection.path,
            output,
            discdump_extractor(discdump),
            manifest,
        )
    except Mismatch as exc:
        print(f"MISMATCH: {exc}")
        return 1
    except Refused as exc:
        print(f"REFUSED: {exc}")
        return 2

    print(f"DISC: {selection.path} [{selection.source}]")
    print(f"BOOT: {SYSTEM_CNF} -> {result.boot_path}")
    print(f"IDENTITY: {result.identity_checks}/{result.identity_checks} facts agree")
    print(f"PROVISIONED: {result.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
