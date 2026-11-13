# Prior art

Other work on Tomba! 2 (`SCUS_944.54` → `MAIN.EXE`, SHA-1 `f41c4065788e8c838cbd8b22f0fff6d4bc7de49a`) and Tomba! 1
(`SCUS_942.36`, SHA-1 `81cbc79f0230aeb4252e058039f47ac95a777f5a`, SHA-256 `c45cd788…`, `titles/tomba1/executable.json`).
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

| project | URL | kind | licence | target build, how matched | what we may take | verified features (file) | last push |
|---|---|---|---|---|---|---|---|
| erneztox/tomba2-decomp | https://github.com/erneztox/tomba2-decomp | Ghidra C dumps (`src/main`, `src/scus`, `src/overlays`, 8,984 `.c`), `symbol_addrs.txt` (~2,100 names), and a Rust/Bevy reimplementation (`bevy_tomba2/`) | none | USA `MAIN.EXE` `f41c4065…` and `SCUS_944.54` `9f68ffd6…` (README, 2026-10-13). Not a decomp: no splat config, no build; its README now says it is no longer byte-matching | evidence only (function boundaries; names are unverified and conflict with ours at `0x8001D940`, `0x8001DB8C`, `0x8001CE90`) | none for the PS1 build | 2026-10-14 |
| mstan/Tomba2Recomp | https://github.com/mstan/Tomba2Recomp | psxrecomp static recompilation | NOASSERTION (PolyForm-Noncommercial-1.0.0) | USA `SCUS-94454` (and Italian PAL): boot entry `0x80018B6C` equals our header; hooks `0x8003F9A8`, `0x8003D0BC`, `0x8003FA44` are function starts in our `MAIN.EXE` | nothing (runtime and design excluded) | widescreen plugin `src/mods/tomba2_widescreen_plugin.c`; interpolation retired to `development/retired-interpolation/` | 2026-11-06 |
| igawa6/Tomba2RecompDS | https://github.com/igawa6/Tomba2RecompDS | fork of Tomba2Recomp (dual screen, Android) | NOASSERTION (psxrecomp-v4 PolyForm) | same as Tomba2Recomp | nothing | claims 60 fps interpolation; only mod manifests (`mods/preloaded/packages/tomba2.experimental.interpolated-frame-rate/`) seen | 2026-09-11 |
| dguillot-gh/tomba2-android | https://github.com/dguillot-gh/tomba2-android | psxrecomp Android build scripts | MIT (own code over psxrecomp) | USA disc per README; not address-checked | evidence of target only | | 2026-11-06 |
| SiergiejW/Tomba2Edit | https://github.com/SiergiejW/Tomba2Edit | asset editor/extractor (levels, collision, models, animation, text, audio, movies) | none | Tomba! 2 disc data | evidence only (formats) | | 2026-11-09 |
| WCreve/Tomba2_Randomizer | https://github.com/WCreve/Tomba2_Randomizer | randomizer | GPL-3.0 | US disc, not checked | evidence only | | |
| jywjyw/tomba2-hack | https://github.com/jywjyw/tomba2-hack | Chinese translation from the JP disc | none | JP build | evidence only (text/font formats) | | 2025-04-10 |

No decompilation of `SCUS_944.54`/`MAIN.EXE` exists: Tomba! 2 needs our own MIT decomp repository. No splat config
for any region of Tomba! 2 exists; the only symbol seed is tomba2-decomp's `symbol_addrs.txt` (no licence, names
unverified).

### Tomba! 2 cheat codes (mods plans 0061-0064)

Searched 2026-11-13 for existing Tomba! 2 mods: pit void-out, permanent suit abilities, co-op, sea/sky
replacement. None exists; only GameShark/CodeBreaker/Action Replay codes for the USA disc, which name RAM we can
cross-check (evidence only, no code):

| source | codes | what they tell us |
|---|---|---|
| almarsguides.com (Tomba 2 NTSC-U CodeBreaker, Action Replay, ASM codes pages); cheatcc.com; gamerevolution.com | Infinite Health `800E7FEE 0010`; items `300BFAB4`..`300BFABF`; outfits `300BFAC0`, `800BFAC2 0101`, `300BFACA`; Max AP `800BF874`/`800BF876`; magic `800BF87E`; event flags `0x800BF8A8`/`0x800BF8AA` | the inventory is one count byte per id from `0x800BFAB4` with the suits at `0x800BFAC0`..; vitality halfword `0x800E7FEE` |
| almarsguides.com ASM codes | Squirrel Suit flight `8005721C EA60`, `8005709A 2400`; Walk On Water `800456BE 2400` (held by `D00ECF55`); Golden Power invulnerability (`80059FB8`, `80066260`, `80022C10`, `80020288`, `80022D1C` set to `FE50`/`9042`); Moon Jump `80057638`; Uncap FPS `80050CDA`/`80050CF2 2400` | code patches that name branch sites in the suit, water and damage paths; each needs our own decompilation before any use |

Searched 2026-11-13: GitHub repositories ("tomba", "tomba2", "tomba decomp", "tombi", "evil swine", "Tomba2Engine",
serials), decomp.dev, and the web for Tomba 2 decomps, widescreen and 60 fps patches.
