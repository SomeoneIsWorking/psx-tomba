# 0047: the boot cards report a failed wide picture

Features: F-WIDE.

Status: fixed (2026-11-03). Owners:
`game/core/entry/runtime.h` (`AspectPolicy`); for the handover, psxport `runtime/psx/render/render_present.cpp`
(`RenderState::buildDisplay`) and `runtime/psx/present/display_presenter.cpp` (`DisplayPresenter::buildDisplay`).

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

## Open: the handover frames 562 and 563

The game's first frame after the cards (562) announces `aspect=1 native_width=256 render_width=256` with the
warning, and 563, the front end's first 320-wide frame, `render_width=342`; 428 follows from 564.

Cause: the width is taken from the previous present's projection plan. `DisplayPresenter::buildDisplay` announces
(`picture::announceOnChange`) and `RenderState::buildDisplay` sizes `lastDisplay_` from
`present::widePresentation`/`widePresentationWidth`, both reading `guestDisplay.plan()`, before
`RenderState::presentRecord` latches this present's plan (`present::latchRecordDisplay`, `record_present.cpp`). At 562
the plan is still the cards' 4:3 one, so a 16:9 request reads as unwidened; at 563 it is the 256-wide reset display's,
widened (256 x 4/3 = 342) where the 320 display needs 428. The record picture itself is laid out from the plan
latched in `presentRecord`, so only the announcement and `lastDisplay_` lag.

Fix (psxport): `RenderState::buildDisplay` latches this present's plan (`RenderState::latchDisplay`) before it sizes
`lastDisplay_`, and `DisplayPresenter::buildDisplay` announces after it; `presentRecord` reads the latched plan
instead of latching again.

Shown, same reproduction from boot: the cards announce `aspect=0 native_width=256 render_width=256`, 562
`aspect=1 native_width=256 render_width=342`, 563 `aspect=1 native_width=320 render_width=428`, and no `[wide:warn]`
line (`scratch/tomba2c/evidence/0047_handover_announcements.txt`).
