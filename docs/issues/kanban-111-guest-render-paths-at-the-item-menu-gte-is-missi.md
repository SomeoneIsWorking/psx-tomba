---
id: kanban-111
title: Guest render paths at the item menu: gte is missing 58% of the picture, psx draws NOTHING
status: open
labels: [render, oracle, bug]
---


USER 2026-08-20, live windowed run: 'the menu is too dark here, PC renderer is correct'. Their session was on PSXPORT_RENDER_PATH=gte (cvars confirmed it, runtime layer). Evidence: scratch/screenshots/live/menu_dark_now.png.

MEASURED HEADLESS, all three paths, same replay, same frame — replays/bugs/ingame-item-menu.pad at f1120 (PSXPORT_NATIVE_FRAMES=1160 PSXPORT_PRESENT_SHOT_AT=1120 PSXPORT_RENDER_PATH=<p>). Shots: scratch/screenshots/menu_{native,gte,psx}.png.

    path     mean RGB                non-black
    native   (76.14, 70.35, 42.90)     89.4%
    gte      (41.92, 35.43, 12.36)     37.1%
    psx      ( 0.00,  0.00,  0.00)      0.0%

FINDING 1 — 'too dark' is mostly 'MISSING', not 'dimmed'. 58.3% of the frame is lit on native and BLACK on gte. Over the 31.1% of the frame lit on BOTH paths, gte is at 0.665 of native's brightness, and the per-channel ratios are NOT uniform:
    R 0.7373   G 0.7131   B 0.4384
A flat texture-modulation error (treating GP0 colour 0x80 as 128/255 = 0.502 instead of 1.0) would halve all three channels EQUALLY, so that is NOT what this is — the hypothesis was tested and refuted by the numbers. Blue is hit roughly twice as hard as red/green, which points at colour-depth / CLUT handling or a wrong texture format rather than a modulation constant. Do not chase the modulation theory; it is already excluded.

FINDING 2, and it is the bigger one — psx_render draws LITERALLY NOTHING at this frame. 0.0% non-black, a fully black 960x720 frame. Not dark: empty.

FINDING 3 — A STALE CLAIM, now falsified. replays/README.md says of this recording: 'the in-game item/pause menu at frame 1120 ... Pixel-exact against psx_render at f1120, so it doubles as the #21 no-regression gate.' That cannot be true of a psx leg that renders an all-black frame. Either the claim rotted or the gate has been silently passing against nothing. Anything that cited this as a passing gate needs re-checking — grep for who relied on it.

RELATION TO #110 (wire beetle's vendored GPU as the real oracle): this card IS the case that motivates it, and it doubles as #110's acceptance test. The guest paths here are OUR rasterizer, not a reference. When beetle's GPU is wired, re-run this exact three-way capture:
  * if beetle renders the menu at native's brightness and coverage, then gte/psx were wrong all along and #110 fixed it;
  * if beetle also comes out dark or empty, the fault is in the guest's own packets and lives upstream of the rasterizer.
Either answer is decisive, which is what makes this a good gate — and it satisfies the 'an instrument is trusted only once it has shown the other answer' rule for the new backend.

REPRO (now reliable — it was NOT before psxport 088c4722, which stopped the headless frame cap from silently truncating a pad replay at 120 frames):
  PSXPORT_NOAUDIO=1 PSXPORT_NO_FMV=1 PSXPORT_NOPACE=1 PSXPORT_RENDER_PATH=<native|gte|psx> \
  PSXPORT_NATIVE_FRAMES=1160 PSXPORT_PRESENT_SHOT_AT=1120 \
  PSXPORT_PAD_RESUME=replays/bugs/ingame-item-menu.pad ./scratch/bin/tomba2_port scratch/bin/tomba2/MAIN.EXE

SUPERSEDES the open question from 2026-08-19 about whether to delete the gte path. The answer is now measured rather than argued: gte is not empty (it draws 37.1% of the frame here and 76,786 non-black px at free-roam), it is BROKEN — and psx is the one that draws nothing at this frame. Deleting gte would have hidden a rasterizer bug that psx has too.
