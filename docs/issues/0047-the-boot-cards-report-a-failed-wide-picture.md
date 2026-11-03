# 0047: the boot cards report a failed wide picture

Features: F-WIDE.

Status: cards fixed (2026-11-03); the one handover frame is open. Owner: `game/core/entry/runtime.h` (`AspectPolicy`).

## Reproduction

Hidden, unpaced, ires 0, fps60 1, aspect 16:9, sink 3840x2160, `PSXPORT_AUTO_SKIP=1`, from boot.

## Observed vs expected

The first `[wide]` announcement of every run, on the SCEA license card, was `aspect=1 native_width=256 render_width=256`
followed by `[wide:warn] a wide picture was REQUESTED and did not happen ... any widescreen claim from this run is
void`. The SCEA card (305 frames) and `LOGO.STR` are host pictures drawn by `BootCards` with no guest projection; they
are shown 4:3 inside the 16:9 frame by design, so a wide request there is false. A held Start skips the card
(`BootCards::stepScea`); the movie setting skips `LOGO.STR` (`stepMovie`).

## Cause

`tomba::Runtime` answered `presentationAspect` with psxport's default, the player's setting, on every frame, including
the cards that precede the game's first frame.

## Fix

`AspectPolicy::presentationAspect` returns 4:3 until the frame driver is past its boot prefix
(`FrameDriver::pastBootPrefix`, `BootCards::finished`), then the player's setting.

## Shown

Same run: the cards announce `aspect=0 native_width=256 render_width=256` with no warning; free roam is still drawn
across 16:9 (`scratch/tomba2/evidence/boot_walk_free_roam_small.png`, frame 1034). Announcements:
`boot_wide_announcements.txt`.

## Open

The game's first frame after the cards (frame 562, where autoskip arms) still announces `aspect=1 native_width=256
render_width=256` with the warning: the display area is the GPU's 256-wide reset until the guest's front end sets 320
at frame 563 (`render_width=342`, then `428` from the opening movie at 564). Not traced: what that one frame shows, and
why the front end's first 320-wide frame renders 342 rather than 428.
