# Prior art

Other work on Tomba! 1 (`SCUS_942.36`, SHA-1 `81cbc79f0230aeb4252e058039f47ac95a777f5a`, SHA-256 `c45cd788…`, `titles/tomba1/executable.json`).
Rows checked through the GitHub API on 2026-11-13; address checks against our images date from 2026-10-13. Where a
reference and our measurement disagree, the measurement wins. Licences are GitHub's SPDX ids; "none" means no licence
file GitHub recognises.

## Tomba! 1

| project | URL | kind | licence | target build, how matched | what we may take | verified features (file) | last push |
|---|---|---|---|---|---|---|---|
| afsenovilla/Tombi-Decomp (submodule `external/tombi-decomp`, pinned `9b591094`, shallow) | https://github.com/afsenovilla/Tombi-Decomp | matching decomp (splat, GCC + maspsx, objdiff) of PAL `SCES_013.31`/`013.30`, USA `SCUS_942.36`, JP `SLPS_011.44`, plus a native port | MIT | USA `SCUS_942.36`: `config/scus94236/scus94236.yaml` carries our SHA-1 `81cbc79f…`, entry `0x8006B58C`; splat configs for the boot exe, `X00`..`X19` and every SYSTEM overlay (`dspsub*`, `gover`, `info*`, `optsub00`); 11,511 `.c` files under `src/` | copy with attribution: names, types, matched C, overlay splits | decomp.dev `SCUS_942.36`: 100.00 % (README: 3681/3681 functions, matched 2026-11-09, `docs/USA.md`). Native PC/Android port of the same C (`port2/`, USA build `tombi-pc-usa`): widescreen 16:9/21:9/32:9 with room-end and top-down extension (`port2/runtime/ws_edge.c`, `port2/runtime/ws_topdown.c`); high frame rates by per-primitive blending between presented frames at the original logic rate (`port2/shims/hfr.inc`); OpenGL and software renderers (`port2/shims/gpu_gl.c`, `gpu.c`); CHD input (`port2/shims/cd_chd.c`) | 2026-11-13 |
| hansbonini/psx_tomba (submodule `external/psx_tomba`, pinned `d848c00f`) | https://github.com/hansbonini/psx_tomba | matching decomp (splat, gcc 2.7.2, objdiff) | none (README badge says MIT; no LICENSE file) | USA `SCUS_942.36`: `config/scus_942.36/checksum.sha` lists our SHA-256 `c45cd788…` and `OPTSUB00.BIN` `f2cbb21b…` (equal to `executable.json`); 2026-10-13 Ghidra checks of `main 0x800163B0`, `flipFrameBuffer 0x80016940`, `dispatchTasks 0x80017024`, `gameplayMainHandler 0x8001B2B4` agree | names only, until a licence is published | decomp.dev 67.27 % decompiled, 27.92 % linked (README: main 78 %, overlays included 38.3 %). Its Psy-Q sources are what Tombi-Decomp ported | 2026-11-08 |
| mstan/TombaRecomp | https://github.com/mstan/TombaRecomp | psxrecomp static recompilation | NOASSERTION (PolyForm-Noncommercial-1.0.0) | USA `SCUS_942.36`: entry `0x8006B58C`; hooks `0x8005E08C`, `0x8005A184` are function starts in our image | nothing (runtime and design excluded) | claims 16:9 and 60 Hz presentation interpolation (release notes); not read | 2026-11-09 |
| hansbonini/tombatools | https://github.com/hansbonini/tombatools | Go format tools (WFM text, GAM) | NOASSERTION (`LICENSE` is empty; README says MIT) | Tomba! 1 data, no addresses | evidence only until the licence is real | | 2025-12-08 |
| zeroviim/TheLiteraturePigBag | https://github.com/zeroviim/TheLiteraturePigBag | WFM text tool | none | US and PAL data | evidence only | | 2021 |
| TwitchyFolkin/Tomba | https://github.com/TwitchyFolkin/Tomba | unused-content research | none | retail and prototypes | evidence only | | 2021 |

Tombi-Decomp is an MIT port of our exact Tomba! 1 build with widescreen and 60 fps presentation already in its tree.
Under the workspace rule a title already ported under a permissive licence is dropped or built by copying from that
port with attribution; that call is the user's.

## Tomba! 2

Moved to `external/tomba2-decomp/docs/prior-art.md` (the Tomba! 2 decomp repository).
