#!/usr/bin/env python3
"""Build and launch the Tomba! 2 native PC port."""

from __future__ import annotations

import hashlib
import os
import platform
import re
import runpy
import shutil
import subprocess
import sys
from collections.abc import Sequence
from pathlib import Path
from typing import NoReturn

ROOT = Path(__file__).resolve().parents[1]
CYAN = "\033[1;36m"
RED = "\033[1;31m"
RESET = "\033[0m"
HELP_ARGUMENTS = {"-h", "--help"}

NATIVE_PACKAGES = {
    "cmake": {
        "fedora": "cmake",
        "debian": "cmake",
        "macos": "cmake",
        "windows": "Kitware.CMake",
    },
    "compiler": {
        "fedora": "gcc gcc-c++",
        "debian": "build-essential",
        "macos": "xcode-select --install",
        "windows": "Microsoft.VisualStudio.2022.BuildTools",
    },
    "git": {
        "fedora": "git",
        "debian": "git",
        "macos": "git",
        "windows": "Git.Git",
    },
    "pkg-config": {
        "fedora": "pkgconf-pkg-config",
        "debian": "pkg-config",
        "macos": "pkg-config",
        "windows": "pkgconf",
    },
    "sdl3": {
        "fedora": "SDL3-devel",
        "debian": "libsdl3-dev",
        "macos": "sdl3",
        "windows": "sdl3:x64-windows",
    },
    "sdl3-image": {
        "fedora": "SDL3_image-devel",
        "debian": "libsdl3-image-dev",
        "macos": "sdl3_image",
        "windows": "sdl3-image:x64-windows",
    },
    "freetype2": {
        "fedora": "freetype-devel",
        "debian": "libfreetype-dev",
        "macos": "freetype",
        "windows": "freetype:x64-windows",
    },
    "zlib": {
        "fedora": "zlib-devel",
        "debian": "zlib1g-dev",
        "macos": "zlib",
        "windows": "zlib:x64-windows",
    },
    "libzstd": {
        "fedora": "libzstd-devel",
        "debian": "libzstd-dev",
        "macos": "zstd",
        "windows": "zstd:x64-windows",
    },
}

PKG_CONFIG_DEPENDENCIES = (
    ("sdl3", "SDL3"),
    ("sdl3-image", "SDL3_image"),
    ("freetype2", "FreeType"),
    ("zlib", "zlib"),
    ("libzstd", "zstd"),
)


class LauncherError(RuntimeError):
    """An actionable launcher refusal."""


def say(message: str) -> None:
    print(f"{CYAN}[run]{RESET} {message}", flush=True)


def print_usage() -> None:
    print(
        "Usage: ./run.sh tomba2 [--resume [recording.pad]] [disc.chd]\n"
        "Build and launch the Tomba! 2 native PC product.\n\n"
        "Options:\n"
        "  -h, --help  Show this help and exit"
    )


def run_checked(
    command: Sequence[str],
    *,
    root: Path,
    error: str,
    env: dict[str, str] | None = None,
    quiet: bool = False,
) -> None:
    try:
        subprocess.run(
            list(command),
            cwd=root,
            env=env,
            stdout=subprocess.DEVNULL if quiet else None,
            check=True,
        )
    except (OSError, subprocess.CalledProcessError) as exc:
        raise LauncherError(error) from exc


def command_output(command: Sequence[str], *, root: Path) -> str:
    try:
        return subprocess.run(
            list(command),
            cwd=root,
            check=True,
            capture_output=True,
            text=True,
        ).stdout.strip()
    except (OSError, subprocess.CalledProcessError):
        return ""


def exec_program(
    program: str, argv: Sequence[str], env: dict[str, str], root: Path
) -> NoReturn:
    try:
        os.chdir(root)
        os.execvpe(program, list(argv), env)
    except OSError as exc:
        raise LauncherError(f"could not launch {program}: {exc}") from exc


def os_release(path: Path = Path("/etc/os-release")) -> dict[str, str]:
    if not path.is_file():
        return {}
    values: dict[str, str] = {}
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        key, separator, value = line.partition("=")
        if separator:
            values[key] = value.strip().strip('"')
    return values


def host_family(
    system: str | None = None, release: dict[str, str] | None = None
) -> str:
    host = system or platform.system()
    if host == "Darwin":
        return "macos"
    if host == "Windows":
        return "windows"
    if host == "Linux":
        release_values = release if release is not None else os_release()
        distro = release_values.get("ID", "")
        distro_like = release_values.get("ID_LIKE", "")
        identities = {distro, *distro_like.split()}
        if identities & {"fedora", "rhel", "centos"}:
            return "fedora"
        if identities & {"debian", "ubuntu"}:
            return "debian"
    return "unknown"


def install_instruction(dependency: str, family: str | None = None) -> str:
    selected_family = family or host_family()
    package = NATIVE_PACKAGES[dependency].get(selected_family)
    if package is None:
        return (
            f"install the native package providing {dependency}; this Linux distribution "
            "is not mapped, so report its name/version rather than guessing a package"
        )
    if selected_family == "fedora":
        return f"please run: sudo dnf install {package}"
    if selected_family == "debian":
        return f"please run: sudo apt install {package}"
    if selected_family == "macos":
        if dependency == "compiler":
            return f"please run: {package}"
        return f"please run: brew install {package}"
    if dependency in {"sdl3", "sdl3-image", "freetype2", "zlib", "libzstd"}:
        return f"please run: vcpkg install {package}"
    if dependency == "compiler":
        return (
            "please run: winget install Microsoft.VisualStudio.2022.BuildTools, then "
            "add the Desktop development with C++ workload in Visual Studio Installer"
        )
    return f"please run: winget install {package}"


def require_tool(name: str, dependency: str) -> None:
    if shutil.which(name) is None:
        raise LauncherError(
            f"required tool {name!r} was not found; {install_instruction(dependency)}"
        )


def require_pkg_config(module: str, label: str, root: Path) -> None:
    try:
        subprocess.run(
            ["pkg-config", "--exists", module],
            cwd=root,
            check=True,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
    except (OSError, subprocess.CalledProcessError) as exc:
        raise LauncherError(
            f"required native library {label} ({module}) was not found; "
            f"{install_instruction(module)}"
        ) from exc


def require_native_dependencies(root: Path, cc: str, cxx: str) -> None:
    require_tool("cmake", "cmake")
    require_tool("git", "git")
    require_tool("pkg-config", "pkg-config")
    require_tool(cc, "compiler")
    require_tool(cxx, "compiler")
    for module, label in PKG_CONFIG_DEPENDENCIES:
        require_pkg_config(module, label, root)


def processor_count() -> int:
    return os.cpu_count() or 4


def player_build_dirs(root: Path, cc: str, cxx: str) -> tuple[Path, Path]:
    """Return isolated player-only build trees for the selected toolchain."""
    identity = hashlib.sha256(f"{cc}\0{cxx}".encode()).hexdigest()[:12]
    base = root / "build/player" / identity
    return base / "framework", base / "game"


def parse_arguments(arguments: Sequence[str]) -> tuple[str | None, list[str]]:
    remaining = list(arguments)
    resume: str | None = None
    if remaining and remaining[0] == "--resume":
        remaining.pop(0)
        if (
            remaining
            and not remaining[0].startswith("-")
            and remaining[0].endswith(".pad")
        ):
            resume = remaining.pop(0)
        else:
            resume = ""
    return resume, remaining


def resolve_resume(requested: str | None, root: Path, env: dict[str, str]) -> None:
    if requested is None:
        return
    if requested:
        display = requested
        resume = Path(display)
    else:
        last = root / "scratch/bin/pad_session.pad"
        if not last.is_file():
            raise LauncherError(
                "--resume: no recording at scratch/bin/pad_session.pad yet — "
                "play a windowed session first, or pass a .pad"
            )
        display = "scratch/bin/pad_resume.pad"
        target = root / display
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(last, target)
        resume = Path(display)

    check_path = resume if resume.is_absolute() else root / resume
    if not check_path.is_file():
        raise LauncherError(f"--resume: no such recording: {resume}")
    frames = check_path.stat().st_size // 2
    say(f"resume: {display} ({frames} pad frames) — fast-forwarding, then it is yours")
    env["PSXPORT_PAD_RESUME"] = display


def env_file_value(path: Path, key: str) -> str:
    if not path.is_file():
        return ""
    pattern = re.compile(rf"^\s*{re.escape(key)}\s*=\s*(.*)$")
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        match = pattern.match(line)
        if match:
            return match.group(1)
    return ""


def resolve_disc(argument: str | None, root: Path, env: dict[str, str]) -> str:
    disc = argument or env.get("PSXPORT_TOMBA2_DISC", "")
    if not disc:
        disc = env_file_value(root / ".env", "PSXPORT_TOMBA2_DISC")
    if not disc:
        disc = env_file_value(root / ".env", "PSXPORT_DISC")
    if not disc:
        drops = sorted(root.glob("*.chd"))
        disc = f"./{drops[0].name}" if drops else ""
    path = Path(disc)
    if not disc or not (path if path.is_absolute() else root / path).is_file():
        raise LauncherError(
            "no disc image — pass it as ./run.sh <disc.chd>, set PSXPORT_TOMBA2_DISC, "
            "or drop a *.chd here"
        )
    return disc


def framework_status(psxport: Path, root: Path, explicit: bool) -> str:
    commit = (
        command_output(
            ["git", "-C", str(psxport), "rev-parse", "--short", "HEAD"], root=root
        )
        or "?"
    )
    dirty = (
        " +dirty"
        if command_output(
            ["git", "-C", str(psxport), "status", "--porcelain"], root=root
        )
        else ""
    )
    if explicit:
        return f"framework: *** {psxport} *** (DEV CLONE {commit}{dirty}) — NOT the recorded pin"
    resolved = (
        psxport.resolve(strict=False)
        if psxport.is_absolute()
        else (root / psxport).resolve(strict=False)
    )
    return f"framework: external/psxport -> {resolved} @ {commit}{dirty}"


def sync_framework(root: Path, env: dict[str, str]) -> Path:
    run_checked(
        [sys.executable, "tools/psxport_fetch.py", "--auto"],
        root=root,
        error="could not resolve external/psxport",
        env=env,
    )
    explicit = bool(env.get("PSXPORT_DIR"))
    psxport = Path(env.get("PSXPORT_DIR", "external/psxport"))
    check_root = psxport if psxport.is_absolute() else root / psxport
    if not (check_root / "cmake/psxport.cmake").is_file():
        raise LauncherError(f"PSXPORT_DIR={psxport} is not a psxport checkout")
    say(framework_status(psxport, root, explicit))

    if (root / ".gitmodules").is_file() and shutil.which("git"):
        sync_script = root / "external/psxport/the former dependency sync tool"
        if not sync_script.is_file():
            say("initializing git submodules…")
            run_checked(
                ["git", "submodule", "update", "--init", "--recursive"],
                root=root,
                error="git submodule update failed",
            )
        if sync_script.is_file():
            run_checked(
                ["bash", str(sync_script)], root=root, error="submodule sync failed"
            )
        else:
            say(
                "WARNING: external/psxport/the former dependency sync tool is absent even after init —"
            )
            say(
                "         submodules were NOT synced and may not match this repo's recorded gitlinks."
            )
    return psxport


def configure_and_build(
    root: Path,
    psxport: Path,
    build_dir: Path,
    cc: str,
    cxx: str,
    jobs: int,
    env: dict[str, str],
) -> Path:
    say("building libchdr + discdump (CMake)…")
    run_checked(
        [
            "cmake",
            "-S",
            str(psxport),
            "-B",
            str(build_dir),
            "-DCMAKE_BUILD_TYPE=Release",
            "-DBUILD_TESTING=OFF",
            f"-DCMAKE_C_COMPILER={cc}",
            f"-DCMAKE_CXX_COMPILER={cxx}",
            f"-DPython3_EXECUTABLE={sys.executable}",
        ],
        root=root,
        error="psxport cmake configure failed",
        env=env,
        quiet=True,
    )
    run_checked(
        [
            "cmake",
            "--build",
            str(build_dir),
            "-j",
            str(jobs),
            "--target",
            "discdump",
        ],
        root=root,
        error="discdump build failed",
        env=env,
        quiet=True,
    )
    discdump = build_dir / "tools/discdump"
    check_discdump = discdump if discdump.is_absolute() else root / discdump
    if not check_discdump.is_file():
        discdump = Path(f"{discdump}.exe")
        check_discdump = discdump if discdump.is_absolute() else root / discdump
    if not os.access(check_discdump, os.X_OK):
        raise LauncherError("discdump build failed")
    return discdump


def provision_and_build_game(
    disc: str,
    discdump: Path,
    psxport: Path,
    build_dir: Path,
    cc: str,
    cxx: str,
    jobs: int,
    root: Path,
    env: dict[str, str],
) -> None:
    boot_stub = Path("scratch/bin/tomba2/SCUS_944.54")
    (root / "scratch/bin").mkdir(parents=True, exist_ok=True)
    run_checked(
        [
            sys.executable,
            "tools/tomba2_provision.py",
            disc,
            "--discdump",
            str(discdump),
        ],
        root=root,
        error="Tomba! 2 runtime-image provisioning failed",
        env=env,
    )
    if not (root / boot_stub).is_file():
        raise LauncherError("Tomba! 2 provisioner did not produce the boot executable SCUS_944.54")

    say(f"building the native port (CMake -j{jobs})…")
    psxport_absolute = (
        psxport.resolve() if psxport.is_absolute() else (root / psxport).resolve()
    )
    run_checked(
        [
            "cmake",
            "-S",
            ".",
            "-B",
            str(build_dir),
            "-DCMAKE_BUILD_TYPE=Release",
            "-DBUILD_TESTING=OFF",
            f"-DPSXPORT_DIR={psxport_absolute}",
            f"-DCMAKE_C_COMPILER={cc}",
            f"-DCMAKE_CXX_COMPILER={cxx}",
            f"-DPython3_EXECUTABLE={sys.executable}",
        ],
        root=root,
        error="cmake configure failed",
        env=env,
        quiet=True,
    )
    run_checked(
        [
            "cmake",
            "--build",
            str(build_dir),
            "-j",
            str(jobs),
            "--target",
            "tomba2_port",
        ],
        root=root,
        error="port build failed",
        env=env,
    )


def main(arguments: Sequence[str] | None = None, *, root: Path = ROOT) -> int:
    args = list(sys.argv[1:] if arguments is None else arguments)
    if args and args[0] in HELP_ARGUMENTS:
        print_usage()
        return 0
    env = dict(os.environ)
    try:
        cc = env.get("CC", "cc")
        cxx = env.get("CXX", "c++")
        require_native_dependencies(root, cc, cxx)

        psxport = sync_framework(root, env)
        requested_resume, remaining = parse_arguments(args)
        resolve_resume(requested_resume, root, env)
        disc = resolve_disc(remaining[0] if remaining else None, root, env)
        say(f"disc: {disc}")

        jobs = processor_count()
        framework_build, game_build = player_build_dirs(root, cc, cxx)
        discdump = configure_and_build(
            root, psxport, framework_build, cc, cxx, jobs, env
        )
        provision_and_build_game(
            disc, discdump, psxport, game_build, cc, cxx, jobs, root, env
        )

        say("launching Tomba (title selector)…")
        policy_root = psxport if psxport.is_absolute() else root / psxport
        policy = runpy.run_path(str(policy_root / "tools/port/launch_environment.py"))
        env = policy["player_environment"](env, product="tomba2")
        env["PSXPORT_ASSET_DIR"] = env.get("PSXPORT_ASSET_DIR") or str(psxport)
        env["PSXPORT_DEBUG_SERVER"] = env.get("PSXPORT_DEBUG_SERVER") or "1"
        env["PSXPORT_NO_TERRAIN"] = env.get("PSXPORT_NO_TERRAIN") or "0"
        env["PSXPORT_TOMBA2_DISC"] = disc
        executable = game_build / "bin" / (
            "tomba2_port.exe" if platform.system() == "Windows" else "tomba2_port"
        )
        exec_program(str(executable), [str(executable)], env, root)
    except LauncherError as exc:
        print(f"{RED}[run] error:{RESET} {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
