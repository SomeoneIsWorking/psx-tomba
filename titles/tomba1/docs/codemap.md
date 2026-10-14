# Codemap — Tomba! 1 (`SCUS_942.36`)

Where Tomba! 1's code lives. It reuses only psxport's executor and services; no Tomba! 2 address,
owner or render policy crosses into `titles/tomba1/`. The product catalog is root `docs/codemap.md`;
feature chains are root `docs/features.md`.

## Owners

| owner | path | owns |
|---|---|---|
| `tomba1::identity` | `titles/tomba1/game/core/entry/tomba1_identity.h` | Size, SHA-256, PS-X EXE header and disc facts from `titles/tomba1/executable.json`; the host extracts the executable to `scratch/bin/tomba1/` |
| `Tomba1Runtime` | `titles/tomba1/game/core/entry/tomba1_runtime.h` | The `GameRuntime`: stock VSync, libcd, libgte and pad buffers, the native overrides, session, frame driver, Record path, widescreen policy, the code-module landing at `0x800E7388`; the only headers `tomba1_runtime` exports |
| `Context` | `titles/tomba1/game/core/state/context.h` | Per-Game title state: the task slots |
| `runNativeBootPrefix` | `titles/tomba1/game/core/boot/native_boot.h` | The finite boot prefix up to retail main's outer loop |
| `initializeSynchronousCd`, CD overrides | `titles/tomba1/game/core/boot/cd_native_startup.h` | CD startup and the CdControl/CdControlF/CdSync/CdReady overrides over `StockLibcd` |
| `bindTitleLibraryOwners` | `titles/tomba1/game/core/boot/sync_native.h` | The stock library bindings, the DMACallback entry (prior pointer and DICR write) and the GPU timeout leaves |
| `Tomba1FrameDriver` | `titles/tomba1/game/core/frame/tomba1_frame_driver.h` | One field: the scheduled tasks, the measured VBlank delivery, one presentation fence |
| `StreamFieldTurn` | `titles/tomba1/game/core/frame/stream_field_turn.h` | Per field during a continuous stream: SPU/XA advanced once, then the paced CD pump |
| `GuestTaskSlots` | `titles/tomba1/game/core/task/guest_task_slots.h` | The three-record task table, the guest's start/yield/restart leaves, the bounded resume of a budget exit across fields |
| tests | `titles/tomba1/tests/` | Stream field turn and its refusals, bounded task resume over real guest code |
| tools | `titles/tomba1/tools/verify_executable.py` | Executable identity check against `executable.json` |
| build | `titles/tomba1/cmake/tomba1_runtime.cmake` | The `tomba1_runtime` library linked into `tomba2_port` |

## Where new work goes

- A Tomba! 1 guest address, boot, frame, CD or DMA rule → the smallest owner under `titles/tomba1/game/core/`.
- Visibility or 2D layout for widescreen → a new `titles/tomba1/game/render/` owner.
- Per-Game title state → `Context`.
- Tomba! 1 is native 60 fps: no producers, interpolation or 60 fps option.
