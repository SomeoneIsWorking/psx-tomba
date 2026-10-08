# 0024 — the render-mode prologue's guest return constants are per (mode, render mode) PAIR

Found 2026-09-28 during the readability pass over `game/camera/cutscene_camera.cpp`. **Not fixed**,
because the pass was behaviour-preserving and this one is a behaviour question, not a readability one.

## What the refactor nearly did, and why that is worth writing down

`CutsceneCamera::dispatchMode` and `CutsceneCamera::dispatchModeFaithful` each ran the same eighteen
driver modes through their own 18-arm switch, and each of the two switches that implement modes 0 and
1 tested three render modes for a dedicated overlay prologue. The six overlay ENTRIES were duplicated
across the two switches and are now one table (`game/camera/camera_mode.h`, `kRenderModePrologues`).

When that table was first written it carried the two overlay addresses per row and nothing else, and
the faithful dispatcher used **one** guest return constant per mode. That compiles, runs, and is
wrong.

The guest-faithful path must arm `ra` with the address of the instruction after each jump, because
the leaf it reaches pushes a guest stack frame and writes that address into guest RAM, which a
side-by-side comparison reads. The guest emits a SEPARATE jump for each of the six (mode, render
mode) combinations, so the six constants are:

| mode | render mode 2 | render mode 7 | render mode 20 |
|---|---|---|---|
| 0 (main follow) | `0x8006ED38` | `0x8006ED48` | `0x8006ED58` |
| 1 (track follow) | `0x8006EE0C` | `0x8006EDFC` | `0x8006EE1C` |

All six are distinct. Collapsing them to one per mode — the obvious "tidy the table" edit — makes
three of the six arms write the wrong return address into guest stack bytes, on exactly the legs
whose whole point is that they are compared.

**The table now carries all six** (`mainFollowReturn` / `trackFollowReturn` per row), the
guest-faithful dispatcher reads them, and
`tests/test_camera_mode_table.cpp::test_the_prologue_return_constants_are_all_six_distinct` fails if
any two of the six collide. Planting that collision is mutant C1 in the pass's report and it turns
that test red.

## The part that is STILL unresolved

**No side-by-side run has exercised the render-mode prologue on render mode 2, 7 or 20 since the
constants moved into the table.** They are transcribed from the previous inline literals, which is
the same provenance the literals had, so this is a refactor of an unverified value rather than a new
claim — but the value itself has never been compared against a reference, in this table or before it.

The three render modes are the ones the header's own note says run a FIELD OVERLAY handler: the
overlay lives in a loaded area image, so reaching render mode 2, 7 or 20 needs an area loaded, and
the recorded runs in `docs/project-state.md` (S001, S004) are free-roam in one area. Establishing it
means running an area whose render mode is 2, 7 or 20 under a comparison, which is an integration
question for whoever reaches gameplay — not something a readability pass can settle.

## What would close it

- One run in an area whose render mode is 2, 7 or 20, with the substrate leg enabled, compared against
  the reference, reporting the guest stack bytes in the six prologue arms' frames.
- If the six constants are right, that run is 0-diff and this issue closes with the numbers attached.
- If it is not, the constants are the first thing to re-derive, and the table is the right place to
  re-derive them into — which is the only reason this was worth writing down now.
