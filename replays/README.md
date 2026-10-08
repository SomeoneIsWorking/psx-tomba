# Replay library

Deterministic pad-capture replays — each is a recorded button sequence (2-byte active-low PSX pad
mask per frame, written by `PSXPORT_PAD_RECORD=<path>`) that reproduces a specific game scenario
**headlessly, bit-for-bit**. Use these to reproduce a bug or reach a game state WITHOUT live user
input — critical for scenarios that need real world-navigation (e.g. walking into a hut) that
AUTO-NAV can't drive.

## How to record a new replay

On a **windowed** run (the user's, or your own with a window), input is recorded by default to
`scratch/bin/pad_session.pad`. To capture a specific scenario to the library:

```
PSXPORT_PAD_RECORD=replays/<category>/<name>.pad ./run.sh
# play the scenario with the keyboard/gamepad, then close the game
```

Then add an entry to this README (category / scenario / how-to-replay / what it surfaces) in the
SAME commit. Categories: `boot-smoke/` (boot + menu nav), `scene-transitions/` (entering
huts/areas/doors), `bugs/` (repros for a specific tracked bug).

## How to replay one

```
# standalone (REPL, inspect state the scenario produces):
printf 'newgame\nrun 9999\nquit\n' \
  | PSXPORT_REPL=1 PSXPORT_VK_HEADLESS=1 PSXPORT_NOAUDIO=1 \
    PSXPORT_PAD_REPLAY=replays/<category>/<name>.pad ./scratch/bin/tomba2_port

# under SBS-full (surface memory divergences the scenario reaches):
PSXPORT_NOWINDOW=1 PSXPORT_SBS_MODE=full PSXPORT_SBS_AUTONAV=1 PSXPORT_NOAUDIO=1 \
  PSXPORT_PAD_REPLAY=replays/<category>/<name>.pad ./scratch/bin/tomba2_port

# under SBS-full WITH the debug server (pause at the diverge, then `sbs watch`/`sbs bt`):
PSXPORT_NOWINDOW=1 PSXPORT_DEBUG_SERVER=1 PSXPORT_SBS_MODE=full PSXPORT_SBS_AUTONAV=1 PSXPORT_NOAUDIO=1 \
  PSXPORT_PAD_REPLAY=replays/<category>/<name>.pad ./scratch/bin/tomba2_port &
# then: python3 tools/dbgclient.py sbs status   (connects to 127.0.0.1:5959)
```

`newgame` pulses Start to reach the GAME prologue, then the replay's button sequence drives from
there. Replays ALWAYS start from a fresh boot — `PSXPORT_PAD_REPLAY` overrides input from frame 0.
Once the recorded sequence ends, input falls through to the host (so `run N` after it just idles).

## Index

### scene-transitions/

| file | frames | scenario | surfaces |
|------|--------|----------|----------|
| `hut-entry-door-freeze.pad` | 934 | AUTO-NAV to free-roam, then walk right + up into the fisherman's-hut door | **SBS diverge @0x801FE91A (f389)** + a door-transition freeze (`FUN_80073328 case 3`) reaching both cores. The cleanest repro for the hut-entry diverge the live windowed session hits. **Also the headless way INSIDE the hut interior** (kanban #29): `run 1200` with no `newgame` lands standing in the room with the wall decorations on screen — the pad ends at f934 and the transition has completed by ~f1100; `press left` + `run 80` scrolls the camera to a second interior viewpoint, `press right` walks back out onto the field. |
| `hut-entry-alt.pad` | 535 | alternate, shorter hut-entry capture | same hut-entry transition; a second capture for cross-checking. |

### bugs/

| file | frames | scenario | surfaces |
|------|--------|----------|----------|
| `seesaw-weight.pad` | 16406 | live hand-played session reaching the seaside **water pump**, with the counterweight seesaw beam | kanban #8 — grabbing the seesaw while climbing does not sink it under Tomba's weight. Cut out of the user's LIVE window with `padrec save` (see docs/driving-the-game.md § live session). Replay reaches the pump; padshots at 8000/12000/14000/15500/16350 show the beam static. |
| `dark-screen-repro.pad` | 61030 | long session reaching a dark/wrong-screen state | the screen-fade / scene-darkening bug family; use under a window to eyeball. |
| `house-on-the-point.pad` | 4134 | live hand-played session entering **"House on the Point"** | the entry CORRUPTS GAME STATE (NOT a freeze/crash — the replay runs clean): music stops, the interior geometry vibrates, and on leaving the house the camera stops following Tomba. Cut out of the user's LIVE window with `padrec save`. Verify by STATE/behaviour comparison (camera-follow latch, BGM sequencer state, per-frame interior vertex jitter), not by crash-checking — a headless run will exit 0 while the state is wrong. |
| `sequence-softlock-2.pad` | 7049 | live hand-played session ending in a **sequence softlock** | kanban #60 — the game keeps advancing (~30fps, no crash, no historical guest-entry miss) but the sequence never progresses and control is never returned. Cut live with `padrec save`. State at the lock: stage=GAME, sm[0x48]=2, scene-active=2; Tomba on a slope with a bird enemy adjacent. VERIFY BY STATE, not crash-checking — a headless run exits 0 while stuck. First probe: `PSXPORT_DEBUG=sched` for the #50 yield-truncation signature. |
| `ingame-options-page.pad` | 1158 | AUTO_SKIP to free-roam, Start, Cross — lands on the IN-GAME **Select Options** page at frame **1160** | kanban #38 (the page's missing full-screen backdrop). The whole OPTIONS family is reachable from here: tap Down k times + Cross after f1160 for Messages / Sound / Screen adjust / Controls. Frame 1090 is the START page (#35). Capture a fixed frame with the debug server (pause, ONE `step` to the target, `shot`). |
| `title-options-page.pad` | 1115 | title → New Game menu → **Options** — the FRONT-END options page at frame **1027** | the same five page builders reached from the title (kanban #7). Use it to check the title path has not regressed when the shared OptionsPage producer changes. |
| `walk-dust-puff.pad` | 530 | AUTO_SKIP to free-roam, then walk right / stop / tap left / run-and-reverse — recorded headless with `PSXPORT_PAD_RECORD` | kanban #39. Fires the **FUN_800286CC animated-quad effect** (impact starburst / one-shot puff) at frames **451-468**; `debug fxanim` names the node and its quad count. NOT the FUN_80029F6C movement dust — for that use `seesaw-weight.pad` frames **216-231** (`debug fxdust`, `debug otattr`). |
| `stun-stars.pad` | 2803 | self-contained boot-to-field capture; walks right and hits a real enemy with CIRCLE | kanban #55/#72. The first real-game stun reproduction for orbiting stars: a visible `0x8002B3A4` ring node is live by ~f2750 and the four-star cluster is visible around f2799/f2800. Compare paths on one execution with `renderpath native` then `renderpath psx`; `debug fxsprite` reports the four centres and texture state. |
| `weapon-charge-starburst.pad` | 1020 | self-contained boot-to-field capture; advances menus, walks right, then holds CIRCLE to charge the weapon | kanban #14. The weapon-charge starburst begins at SBS f667; the verified sample window is f650-700. |
| `cliff-fisherman-missing.pad` | 152 | live windowed session PAUSED by the USER on the seaside **cliff plateau over water**, cut with `padrec save` | kanban #95 — the fisherman NPC and his ROD do not render under pc_render while his fishing LINE (the 0x5E family, `game/render/fx_line.cpp`) still does. Reaches the spot in **156 frames**, so it is the cheapest render repro in the library: `run 156` then `shot`. Verify by LOOKING at the shot, not by exit status. **`PSXPORT_ORACLE=1` SEGFAULTS at f73 on this replay (kanban #96)**, so there is no oracle reference in this scene until that is fixed. |
| `machinery-cutscene.pad` | 30580 | a long LIVE hand-played session, cut with `padrec save`; the tail is ~10 min of idle at the spot | kanban #103 — an area-0 cutscene where an NPC works a piece of MACHINERY: the machinery is invisible, the fisherman the user expects is absent, and bridge ropes are missing. The scene is on screen from ~pad frame **30150** (verified: the replay reproduces the user's live shot). LONG — reach it with `PSXPORT_PAD_RESUME=` (fast-forward, ~4.7 min at ~110 fps) rather than `PSXPORT_PAD_REPLAY=` (~17 min at real speed). **Do NOT replay it on a second EXECUTION leg for an oracle compare** — a pad file does not transfer between legs: the `PSXPORT_ORACLE=1` leg needs 56370 native frames where the default leg needs 31050, and it lands on a save prompt instead of the cutscene. Swap renderers on ONE run with `renderpath` instead. |
| `ingame-item-menu.pad` | 1118 | AUTO_SKIP to free-roam, Triangle — the in-game **item/pause menu** at frame **1120** | kanban #21 (the menu's opaque panel). Pixel-exact against psx_render at f1120, so it doubles as the #21 no-regression gate. |

### boot-smoke/

| file | frames | scenario | surfaces |
|------|--------|----------|----------|
| `start-mash-smoke.pad` | 1500 | Start-mash through the intro to free-roam | boot→menu→field smoke check (should reach GAME stage cleanly). |
| `short-session.pad` | 426 | short input session | minimal boot capture. |
| `general-session.pad` | 1200 | general play session | generic smoke. |

## Notes

- **Format:** each `.pad` is a raw stream of little-endian `uint16_t` active-low PSX pad masks, one
  per frame. Decode with: `python3 -c "import struct; [print(hex(struct.unpack('<H',open('X.pad','rb').read()[i:i+2])[0])) for i in range(0,len(open('X.pad','rb').read()),2)]"`.
  Active-low bits (0=pressed): 0=R 1=L 2=U 3=D 4=tri 5=cross 6=o 7=sq 8=L2 9=R2 10=L1 11=R1 … start.
- **Determinism:** replays are bit-exact (validated by the door_freeze finding — reproduces
  frame-for-frame across runs). Safe to gate fixes on.
- **Cross-exec-path replay: UNKNOWN, do not assume either way.** One observation (2026-07-21,
  `bugs/seesaw-weight.pad`) had the pc_faithful and `PSXPORT_GATE=1` legs rendering different areas at
  the same replay frame (`scratch/screenshots/seesaw_pc_vs_gate.png`) — but nothing established that
  the input stream was the cause rather than a real behavioural difference between the paths. An
  earlier revision of this file asserted a desync mechanism as fact; that was unverified and is
  withdrawn. For path comparison prefer SBS (lockstep, one input stream, two cores) until someone
  actually measures this.
- **Originals** also live at `scratch/bin/*.pad`; the copies here are
  the canonical, categorized home going forward.

- `bugs/save-confirm-crash.pad` — 1673 frames, cut from the USER's live windowed session with
  `padrec save` on 2026-08-19, so it replays from boot. Ends with Tomba at the seaside SAVE SIGN and
  the "Save? / Yes / No" prompt already on screen, which is the state the crash report was made from.
  Drive on from there with the REPL: `tap x 8` accepts the prompt, `tap x 8` again picks the card
  slot, and a third opens "OK to save?". Reproduced the memory-card device-table crash (the port
  aborted on an unmapped read from FUN_80080940's device walk) and now covers the whole save round
  trip plus every card screen's chrome.
- `bugs/save-card-pages.pad` — 2400 frames. NOT a hand capture: built from
  `bugs/save-confirm-crash.pad`, which keeps that route's 1673
  frames byte-for-byte and appends three Cross taps (f1700-1707, f1760-1767, f1820-1827). Those are
  the three the save-confirm entry describes reaching by REPL, so this drives the whole save round
  trip headlessly: "Save? Yes/No" (~f1690), "Select slot / MEMORY CARD slot 1 / slot 2" (~f1740),
  the "01 NEW" file list and "OK to save? Yes/No" (~f1870), and back to free-roam (~f2300). Do NOT
  add `PSXPORT_AUTO_SKIP` — the base replays from boot through the opening FMV and desyncs without
  it. This is the widescreen coverage route for the memory-card scene kind (project state S005);
  all five of those frames measure drawn aspect 1.333 -> 1.784.

- `bugs/save-prompt-black-screen.pad` — 12500 frames, cut from a live debug-server session
  (`padrec save`) on top of `bugs/bucket-softlock.pad`, so it replays from boot: it closes the
  bucket-pickup dialog with CIRCLE, walks east, and DIES, landing in the GAME OVER / CONTINUE
  screen (state machine FUN_80106478; sub-state at task 0x801FE000+0x4C = 3, menu cursor at +0x4E).
  Confirm there with CROSS, not Circle. What it surfaces (kanban #62): on that black game-over
  screen pc_render still draws FIELD geometry — a stray rope/vine from the top of the screen and a
  garbled sprite cluster beside the text — while the guest emits exactly one black FILL.
  ```
  PSXPORT_VK_HEADLESS=1 PSXPORT_NOAUDIO=1 PSXPORT_DEBUG_SERVER=5965 \
    PSXPORT_PAD_REPLAY=replays/bugs/save-prompt-black-screen.pad ./scratch/bin/tomba2_port <MAIN.EXE>
  # then: dbgclient --port 5965 vkshot <path>   (the prompt is up from ~f12500)
  ```
- `bugs/weapon-impact-bucket.pad` — 686 frames, self-contained from boot (do NOT add
  `PSXPORT_AUTO_SKIP` or `newgame`, it desyncs). Tomba walks right along the seaside start route into
  the bucket obstacle and swings (CIRCLE); the swing connects at pad frames 654-660, peak 656. The
  resolved repro for kanban #15 (weapon impact burst formerly missing its mesh half under pc_render):
  `PSXPORT_PAD_REPLAY=replays/bugs/weapon-impact-bucket.pad PSXPORT_PAD_SHOT_AT=654,656,658 ... run 700`
- `bugs/walk-dust-puff.pad` — also the bounded A00 water-jet mesh fallback gate. In a true SBS
  software-oracle run, sample f450/460/470/480/490/500/510/520 and exit at f530. The fallback is live
  at f460/470/480/490/510/520 (two exact guest GT4 packets and 8/8 packet-addressed guest-depth hits
  per call); f450/f500 are no-call negative controls. Pane B must identify
  `PURE-ORACLE(interp+softGPU)` and remain byte-identical to the retained pre-fallback captures. This
  is explicit `render-fallback-water-jet-guest-gte` debt, not evidence of a native `0x8013D454`
  producer.

## `bugs/long-session-many-bugs.pad` (2026-08-19)

51,170 pad frames — the USER's own long play session, cut live with `padrec save` while the window
was still running, at their request ("it's very long and covers a TON of bugs"). Not yet triaged:
the bugs it reaches have not been enumerated. Known to pass through the machinery cutscene (#103)
and the bridge-rope area (#107's centring, which is what revealed the ropes are drawn at all).

Resume it with `./run.sh --resume replays/bugs/long-session-many-bugs.pad`, or headless with
`PSXPORT_PAD_RESUME=replays/bugs/long-session-many-bugs.pad PSXPORT_NO_FMV=1 PSXPORT_NOPACE=1`.
NO_FMV matters: without it the boot FMVs play in real time and the watchdog kills the run before
the fast-forward starts.
