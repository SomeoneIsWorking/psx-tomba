# Widescreen ownership

This directory will own the title's widescreen projection and 2D layout policy after RE identifies the
retail projection, visibility-culling, and interface-layout producers. It will use psxport's shared
non-temporal presentation mechanism and preserve the 4:3 path.

There is deliberately no Tomba! 1 interpolation/lerp, temporal history, native renderer, native
producer, or native-depth subsystem. Widescreen must widen game-owned projection and visibility rather
than stretch the final image or derive a picture from OT/GP0/GTE output.

There are also no 60fps or native-rendering options, including hidden or off-by-default compatibility
switches. `enhancement_scope.json` and the title-isolation test enforce that absence.
