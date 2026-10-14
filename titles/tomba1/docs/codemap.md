# Tomba! 1 codemap

This map owns title-local placement only. Progress belongs in `project-state.md`; intent in
`titles/tomba1/docs/project-goals.md`; ordering in `docs/migration.md`; evidence dependencies in
`titles/tomba1/docs/re-frontier.md`. Root `game/` is the Tomba! 2 engine and is outside this map.

`titles/tomba1/game/core/` is split by CONCEPT, so an owner and its dependencies sit together and no
single directory has to mean "the core":

| Directory | Owns |
|---|---|
| `core/entry/` | the title identity and `Tomba1Runtime` composition (the product's `TombaCatalog` runs it); the only include root `tomba1_runtime` exports |
| `core/state/` | the per-`Core` `Context` (task slots, widescreen plan) |
| `core/boot/` | the native boot prefix, synchronous CD startup, and the platform HLE/DMA sync plan |
| `core/frame/` | the finite frame transaction and the stream field turn |
| `core/task/` | the three-record guest task table and its bounded cross-field resume |
| `render/` | title-derived widescreen projection and layout, and nothing imported from Tomba! 2 |

```text
titles/tomba1/game/core/entry     title identity and runtime composition (the exported headers)
    |
titles/tomba1/game/core/state     owns the per-Core Context
    |
titles/tomba1/game/core/boot     owns the native boot prefix and CD/sync platform plan
    |
titles/tomba1/game/core/frame    owns the finite frame transaction and stream field turn
    |
titles/tomba1/game/core/task     owns the guest task table and its bounded resume
    |
psxport                            owns shared Lightrec execution and PSX services
    |
titles/tomba1/game/render         owns only title-derived widescreen projection and layout
```

## Ownership

| Subsystem | Responsibility | Current or target location | Entry point | Deep document |
|---|---|---|---|---|
| Title game tree | Contain every Tomba! 1 application, core, and render owner without importing the root Tomba! 2 engine | `titles/tomba1/game/` | title application | `titles/tomba1/AGENTS.md` |
| Application | Publish the title identity and compose the runtime the product catalog runs | `titles/tomba1/game/core/entry/` | `tomba1::identity()`, `Tomba1Runtime` | `titles/tomba1/game/core/entry/README.md` |
| Executable identity | Compare the selected image with tracked whole-file and PS-X header facts | `titles/tomba1/executable.json`, `titles/tomba1/tools/verify_executable.py` | `verify_executable.py --check` | `titles/tomba1/docs/info/claims/001-executable-identity.md` |
| Shared PSX executor | Translate remaining guest instructions with Lightrec; own state synchronization, bounded exits, image generations, invalidation, and generic override/original-call dispatch | shared `external/psxport/` | shared executor API | psxport codemap and migration docs |
| Framework seam | Own Tomba! 1 immutable executable facts, native boot prefix, finite frame transaction, and title-facing executor composition | `titles/tomba1/game/core/{entry,state,boot,frame,task}/` | `Tomba1Runtime`, `runNativeBootPrefix`, `Tomba1FrameDriver` | `titles/tomba1/game/core/README.md` |
| Native override composition | Bind the title's six engine-neutral native owners by complete resident/overlay identity and address | `titles/tomba1/game/core/` | title override-install composition | `titles/tomba1/docs/re-frontier.md` |
| Frame scheduler | Own the finite per-frame transaction: frame bookkeeping, one pass over the scheduled tasks, the measured VBlank delivery, and one presentation fence | `titles/tomba1/game/core/frame/tomba1_frame_driver.*` | `Tomba1FrameDriver::stepFrame` | `titles/tomba1/docs/re-frontier.md` |
| Guest task slots | Own the three-record task table, one saved R3000 context per slot, the guest's start/yield/restart/exit leaves, and the BOUNDED resume of a budget exit across display fields | `titles/tomba1/game/core/task/guest_task_slots.*` | `GuestTaskSlots::runScheduledTasks`, `GuestTaskSlots::createTask` | `titles/tomba1/docs/issues/0007-a-guest-decompress-spans-more-fields-than-one.md` |
| Disc synchronization | Own reached title CD startup, command/read services, internal/public sync bindings, stream field cadence, and, at the host-served `CdControl`/`CdSync`, the acknowledgement (psxport `psx::cd::acknowledgeCurrentCdResponse`) of controller responses no guest callback receives | `titles/tomba1/game/core/boot/cd_native_startup.*`, `titles/tomba1/game/core/boot/sync_native.*`, `titles/tomba1/game/core/frame/stream_field_turn.*` | `initializeSynchronousCd`, title platform plan | `titles/tomba1/docs/issues/0006-libcd-internal-cdsync-reaches-guest-vsync-after-movie.md` |
| Guest module residency | The guest's load-list walker streams its code module to `0x800E7388`; `Tomba1Runtime::stockCdReadLanded` hands that landing to the framework's `psx::code_module::publishStockReadLanding`, which owns the image identity and invalidation | `titles/tomba1/game/core/entry/tomba1_runtime.cpp` | `Tomba1Runtime::stockCdReadLanded` | `titles/tomba1/docs/issues/0008-guest-streamed-code-module-has-no-image-identity.md` |

| DMA callback registration | Adapt title DMA registration and callback identity while preserving guest-visible return and DICR behavior | `titles/tomba1/game/core/boot/sync_native.*` | title DMA callback override | `titles/tomba1/docs/issues/0005-movie-stream-stops-scheduling-after-lba-57838.md` |
| Projection publication | Preserve the measured `SetGeomOffset`/`SetGeomScreen` state and expose title policy without importing Tomba! 2 rendering | `titles/tomba1/game/core/boot/sync_native.cpp`, target policy in `titles/tomba1/game/render/` | title platform plan | `titles/tomba1/docs/info/claims/006-scus-942-36-publishes-its-resident-projection-th.md` |
| Retired-execution exclusion | Keep the removed generator, emitted guest source, and offline dispatcher absent; prohibit interpreter-first gameplay modes | absent by design; enforced by title composition, provisioning, and policy checks | no entry point | `titles/tomba1/docs/re-frontier.md` |
| Title tests | Own identity, provisioning, isolation, native-boundary, fallback-threshold, and negative checks | `titles/tomba1/tests/` | explicit C++ test executable or Python test | test source nearest the production boundary |
| Title tools | Own identity verification and title-local RE inspection without owning product execution | `titles/tomba1/tools/` | cohesive Python entry point | tool `--help` and nearest evidence doc |
| Test-only independent executor | Reproduce deterministic boundaries and forced negatives; product absence is proved by link and selector inspection | separately built test target, including diagnostics, under `titles/tomba1/tests/` | standalone test executable | `titles/tomba1/docs/re-frontier.md` |
| Widescreen projection | Own title-derived horizontal projection, visibility, and wide-buffer policy | `titles/tomba1/game/render/` | future `WidescreenProjection` | `titles/tomba1/game/render/README.md` |
| Widescreen layout | Preserve authored 2D anchors across wide presentation | `titles/tomba1/game/render/` | future `WidescreenLayout` | `titles/tomba1/game/render/README.md` |

## Where does new work go?

| Concern | Owner |
|---|---|
| Title identity and runtime composition | `titles/tomba1/game/core/entry/` |
| Disc selection and executable extraction | psxport's host (`executable_provisioner`) from `tomba1::identity()` |
| Lightrec execution, runtime image generations, invalidation, bounded exits, or generic scoped original calls | shared psxport |
| Tomba! 1 guest addresses, ABI, boot, frame order, CD/DMA policy, or native behavior | smallest cohesive owner under `titles/tomba1/game/core/` |
| Projection, visibility, edge coverage, wide-buffer placement, and HUD/menu anchoring | `titles/tomba1/game/render/` |
| Offline emission, generated registries, or interpreter-first gameplay selection | nowhere; these have no target owner |
| Epic intent, capability state, atomic work, and evidence | their respective `titles/tomba1/docs/project-goals.md`, `titles/tomba1/docs/project-state.md`, `titles/tomba1/docs/issues/`, and `titles/tomba1/docs/re-frontier.md` authorities |
