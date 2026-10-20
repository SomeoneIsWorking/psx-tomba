"""The built tomba2_port binary, its boot executable and the one headless launch environment.

Every agent driver of the product (tools/oracle_compare.py, tools/picture_oracle.py) builds its
environment here so they cannot drift apart.
"""
from __future__ import annotations

import os
import sys
from pathlib import Path

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PSXPORT = os.path.join(REPO, 'external', 'psxport')

# The canonical maintainer build; the player build under build/player/<toolchain> belongs to run.sh.
BIN = os.path.join(REPO, 'build', 'ci', 'bin', 'tomba2_port')
# The boot stub the catalog selects Tomba! 2 by; the product host extracts it from the disc.
EXE = os.path.join(REPO, 'scratch', 'bin', 'tomba2', 'SCUS_944.54')
SHIPPING_SETTINGS = os.path.join(REPO, 'tools', 'shipping_settings.ini')


class DiscMissing(RuntimeError):
    """No Tomba! 2 disc is named for an oracle run."""


def tomba2_disc(env: dict) -> str:
    """The Tomba! 2 disc the product resolves: the environment, then the repository .env."""
    disc = env.get('PSXPORT_TOMBA2_DISC', '')
    dotenv = Path(REPO) / '.env'
    if not disc and dotenv.is_file():
        for line in dotenv.read_text(encoding='utf-8', errors='replace').splitlines():
            key, separator, value = line.strip().partition('=')
            if separator and key.strip() == 'PSXPORT_TOMBA2_DISC':
                disc = value.strip()
                break
    if not disc or not Path(disc).is_file():
        raise DiscMissing('no Tomba! 2 disc: set PSXPORT_TOMBA2_DISC in the environment or .env')
    return disc


def native_environment(watchdog: int, debug: str = '', extra_env: dict | None = None,
                       settings: str | None = None) -> dict:
    """The one headless REPL launch environment for the built tomba2_port binary.

    The launch-environment policy (psxport tools/port/launch_environment.py) owns the
    headless/silent/unpaced knobs; the disc follows the product's own environment/.env lookup.

    `settings` names the tracked .ini the run uses, defaulting to the shipping one. The picture
    oracle passes psxport's reference settings instead: it photographs the product against a 4:3
    console, and a widescreen frame is a different SIZE, which the comparison refuses."""
    sys.path.insert(0, os.path.join(PSXPORT, 'tools'))
    from port.launch_environment import agent_environment
    env = agent_environment(dict(os.environ), settings or SHIPPING_SETTINGS)
    env['PSXPORT_ASSET_DIR'] = env.get('PSXPORT_ASSET_DIR') or PSXPORT
    env['PSXPORT_TOMBA2_DISC'] = tomba2_disc(env)
    env['PSXPORT_REPL'] = '1'
    env['PSXPORT_WATCHDOG'] = str(watchdog)
    if debug:
        env['PSXPORT_DEBUG'] = debug
    env.update(extra_env or {})
    return env
