# Codemap — Tomba! 1 (`SCUS_942.36`)

Where Tomba! 1's code lives. It reuses only psxport's executor and services; no Tomba! 2 address,
owner or render policy crosses into `titles/tomba1/`. The product catalog is root `docs/codemap.md`;
feature chains are root `docs/features.md`.

Frame loop: owned. `tomba1::FrameDriver` runs the guest task pass and delivers the frame's requested fields `0x1F8001EA`; no guest VSync wait runs.

## Owners

| owner | path | owns |
|---|---|---|
| `tomba1::identity` | `titles/tomba1/game/core/entry/identity.h` | Size, SHA-256, PS-X EXE header and disc facts from `titles/tomba1/executable.json`; the host extracts the executable to `scratch/bin/tomba1/` |
| `tomba1::Runtime` | `titles/tomba1/game/core/entry/runtime.h` | The `GameRuntime`: stock VSync, libcd, libgte and pad buffers, the native overrides, session, frame driver, Record path, widescreen policy, the code-module landing at `0x800E7388`; the only headers `tomba1_runtime` exports |
| `Context` | `titles/tomba1/game/core/state/context.h` | Per-Game title state: the task slots |
| `runNativeBootPrefix` | `titles/tomba1/game/core/boot/native_boot.h` | The finite boot prefix up to retail main's outer loop |
| `initializeSynchronousCd`, CD overrides | `titles/tomba1/game/core/boot/cd_native_startup.h` | CD startup and the CdControl/CdControlF/CdSync/CdReady overrides over `StockLibcd` |
| `bindTitleLibraryOwners` | `titles/tomba1/game/core/boot/sync_native.h` | The stock library bindings, the DMACallback entry (prior pointer and DICR write) and the GPU timeout leaves |
| `tomba1::FrameDriver` | `titles/tomba1/game/core/frame/frame_driver.h` | One field: the scheduled tasks, the measured VBlank delivery, one presentation fence |
| `BootSkip` | `titles/tomba1/game/core/frame/boot_skip.h` | Start or Cross, or a selector preview, ends the SCEA card (`0x800191E0` state 0xB), the logo movie (stream 0x15, stop request `0x1F8001D3`) and the logo hold (`0x80019844` state 0) through each one's own terminal, before the task pass |
| `StreamFieldTurn` | `titles/tomba1/game/core/frame/stream_field_turn.h` | Per field during a continuous stream: SPU/XA advanced once, then the paced CD pump |
| `tomba1::DebugMenu` | `titles/tomba1/game/core/debug/debug_menu.h` | The title debug menu (psxport `TitleDebugMenu`): the warp catalogue `warp <area>/<section>` |
| `tomba1::DevWarp` | `titles/tomba1/game/core/debug/dev_warp.h` | The warp arrival: armed by `DebugMenu`, applied by `FrameDriver::stepFrame` before the tasks run, through the door's own transition (machine 7), from any machine a door leaves (1, 2, 4..6) in its ticking state |
| `GuestTaskSlots` | `titles/tomba1/game/core/task/guest_task_slots.h` | The three-record task table, the guest's start/yield/restart leaves, the bounded resume of a budget exit across fields |
| `MeshChunk`, `installMeshDraw` | `titles/tomba1/game/render/mesh_chunk.h`, `mesh_draw.h` | The world-mesh submitters as native overrides: the twelve chunk kinds (`0x80024CFC`..`0x800263F0`) and the drawers `0x80026694`, `0x80026E48`, `0x80027600`, `0x800279E8`; each culls through `psx::present::ScreenCull` over the record canvas's columns at 224 rows |
| `installSpriteParts` | `titles/tomba1/game/render/sprite_parts.h` | The entity sprite drawers as native overrides: plain `0x800487A4`, scaled `0x80048BF0`, rotated `0x80049134`, rotated and scaled `0x80049994`, screen-space `0x8004BAF0`, the model quad `0x8004AA70` and the overlays' four-corner quad test `0x80045AF8`; each part is a raw-textured quad around the entity's anchor (RTPS, or its own x/y for screen-space), culled through `psx::present::ScreenCull` over the record canvas; and the 2D pool's kind-1 drawer `0x8004DFA0` through `SpriteFrame` |
| `SpriteFrame` | `titles/tomba1/game/render/sprite_frame.h` | An entity's frame as raw sprites (SPRT) between two draw-mode packets, the part rule `0x8004DFA0` and area 4's group share, culled over the record canvas at 241 rows; a copy the guest never draws writes only the parts it keeps |
| `installHudAnchor` | `titles/tomba1/game/render/hud_anchor.h` | The HUD pass's element drawers (`0x8004E590`, `0x8004E714`, `0x8004E900`, `0x8004EB10`, `0x8004ED80`) through their originals, their packets moved by host packets to the drawn picture's edge on their anchor's side |
| `sext8`, `sext16` | `titles/tomba1/game/render/guest_signed.h` | Guest bytes and halfwords as `lb`/`lh` load them |
| `installOverlaySpriteGroups` | `titles/tomba1/game/render/overlay_sprite_groups.h` | The area overlays' screen-space sprite-group drawers as native overrides, bound by `Runtime::stockCdReadLanded` when the overlay's content identity lands: area 4's two-frame group `0x8011DA28` through `SpriteFrame` |
| `BackdropRing` | `titles/tomba1/game/render/backdrop_ring.h` | Area 4's backdrop ring: the column update `0x8011C894` (type 9 in the 2D pool `0x800A3D08`, ten 0x6C-byte slots) natively, its ring rule (phase = (home - camera) / 4 mod 512, placed at u or u - 512, hidden for 384..448), and the ring positions beyond the picture the area 4 group drawer adds at the canvas |
| `PaintedRoomProjection` | `titles/tomba1/game/render/painted_room.h` | The title's `GuestWidescreenProjection`: a painted room's picture columns beyond the buffer (kind-3 block-map scrollers of the area 2 and 5 overlays, kind-2 lists of `0x8004E244`), so the presenter pillarboxes past the picture |
| `PacketPool` | `titles/tomba1/game/render/packet_pool.h` | The two 0xC000-byte packet passes from `0x800B3188` behind the cursor `0x1F800164`; a native packet that would pass its pass's end aborts; `passBytes` is what the frame driver logs per pass |
| build | `titles/tomba1/cmake/runtime.cmake` | The `tomba1_runtime` library linked into `tomba2_port` |

## Where new work goes

- A Tomba! 1 guest address, boot, frame, CD or DMA rule → the smallest owner under `titles/tomba1/game/core/`.
- Visibility or 2D layout for widescreen → `titles/tomba1/game/render/`.
- Per-Game title state → `Context`.
- Tomba! 1 is native 60 fps: no producers, interpolation or 60 fps option.
