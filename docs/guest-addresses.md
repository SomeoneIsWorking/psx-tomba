# Tomba! 2 guest address index

Guest function to native owner. Before porting a `FUN_xxxx`, look it up here or grep the
`declareOverride`/`declareOverlayOverride` call sites; update the row in the change that lands or moves
an owner. Overlay addresses repeat across images, so an overlay row names its image in the symbol column.

Status: **LIVE** = some product code path calls the native; **ORPHAN** = the native exists and nothing
calls it. A row whose handler is file-local names the file that declares the override.

| addr | status | symbol | file | depends-on (still-PSX) | summary |
|------|--------|--------|-----------|------------------------|---------|
| 0x8001D364 | LIVE | `AudioDispatch::voiceFetchBits` | game/audio/audio_dispatch.cpp | 0x8001D2A8 | AudioDispatch::voiceFetchBits — native ownership of FUN_8001D364 (Ghid… |
| 0x8001D71C | LIVE | `AudioDispatch::zoneTransitionSetup` | game/audio/audio_dispatch.cpp | 0x8001CF2C 0x8001D2A8 | AudioDispatch::zoneTransitionSetup — native ownership of the tiny disp… |
| 0x8001F40C | LIVE | `CollisionResolve::classifyBodyContact` | game/world/collision_resolve.cpp |  |  |
| 0x8001F9DC | LIVE | `MeleeProximity::isAtApproachAnchor` | game/ai/melee_proximity.cpp | 0x80084080 |  |
| 0x8001F9DC | LIVE | `MeleeProximity::isAtApproachAnchorFramed` | game/ai/melee_proximity.cpp |  |  |
| 0x8001F9DC | LIVE | `MeleeProximity::registerOverrides` | game/ai/melee_proximity.cpp |  |  |
| 0x8001FAE0 | LIVE | `ActorTargeting::tryAcquireTarget` | game/player/actor_targeting.cpp |  | ORACLE: guest 0x8001FAE0 |
| 0x80020364 | LIVE | `ActorTomba::stepModeInteract` | game/player/actor_tomba.cpp |  | postInteractWalk case 0xF/0x14/0x56 (mode=0) / 0x2F (mode=2). |
| 0x800205CC | LIVE | `ActorTomba::type8Interact` | game/player/actor_tomba.cpp |  | postInteractWalk case 8. |
| 0x80022060 | LIVE | `ActorTomba::proximityCheck` | game/player/actor_tomba.cpp |  | cylinder proximity + Y-band check. |
| 0x80022190 | LIVE | `ActorTomba::subHitboxCheck` | game/player/actor_tomba.cpp |  | per-sub-hitbox collision variant. |
| 0x80022760 | LIVE | `ActorTomba::interactWalk` | game/player/actor_tomba.cpp |  |  |
| 0x80022C78 | LIVE | `ActorTomba::growthYSnap` | game/player/actor_tomba.cpp |  | leaf, no guest-stack frame. Operates on G (postFrameWaterCheck's |
| 0x800235A0 | LIVE | `ActorTomba::type7Interact` | game/player/actor_tomba.cpp |  | postInteractWalk case 7. |
| 0x80023A04 | LIVE | `CollisionResolve::resolveByContactPolicy` | game/world/collision_resolve.cpp |  |  |
| 0x80023D48 | LIVE | `CollisionResolve::cylinderResolve` | game/world/collision_resolve.cpp |  | ORACLE: guest 0x80023D48 |
| 0x8002423C | LIVE | `CollisionResolve::landOnObjectTop` | game/world/collision_resolve.cpp |  |  |
| 0x80024794 | LIVE | `interact_scan` | game/player/interact_scan.cpp |  | (player) -> 1 if something was activated this call, else 0. |
| 0x80025588 | LIVE | `Engine::sceneEventFifo` | game/core/engine/engine_task_machine.cpp |  | Native FUN_80025588 — the field EVENT/COMMAND-QUEUE state machine (str… |
| 0x800263E8 | LIVE | `Pool::seedAreaObjects` | game/world/pool.cpp |  | area object-record seeding. Selects a per-area byte sequence (table 0x… |
| 0x80026470 | LIVE | `BgSceneTransitionSm::midTransitionGate` | game/scene/bg_scene_transition_sm.cpp |  | Common guard shared by FUN_80026470/80026510/800264BC — three inline a… |
| 0x80026470 | LIVE | `BgSceneTransitionSm::audioStub26470` | game/scene/bg_scene_transition_sm.cpp |  |  |
| 0x800264BC | LIVE | `BgSceneTransitionSm::audioStub264BC` | game/scene/bg_scene_transition_sm.cpp |  |  |
| 0x80026510 | LIVE | `BgSceneTransitionSm::audioStub26510` | game/scene/bg_scene_transition_sm.cpp |  |  |
| 0x8002655C | LIVE | `BgSceneTransitionSm::body` | game/scene/bg_scene_transition_sm.cpp |  |  |
| 0x80026864 | LIVE | `LetterboxBars::tick` | game/render/letterbox_bars.cpp | 0x8007FCC8 | cutscene letterbox, 8-slot array type 1; bars span the canvas margins |
| 0x80026C88 | LIVE | `ObjectTable::dispatch` | game/world/object_table.cpp | 0x80026C88 |  |
| 0x80027254 | LIVE | `ObjectTable::handler27254` | game/world/object_table.cpp |  |  |
| 0x80027768 | LIVE | `PackedMesh::write` | game/render/packed_mesh.cpp |  | The shared packed-mesh writer: one POLY_GT4 per 36-byte record, depth-cued by scratch IR0. |
| 0x80027CB4 | LIVE | `FxSpriteAnchored::emitUniformScale` | game/render/fx_sprite_anchored.cpp |  | seaside hut-roof flames; projection via `FxSpritePublish` |
| 0x80027E5C | LIVE | `FxSpriteAnchored::emitByteScale` | game/render/fx_sprite_anchored.cpp |  | weapon-impact burst sprite; projection via `FxSpritePublish` |
| 0x800281EC | LIVE | `FxSpriteSwarm::emitPerParticle` | game/render/fx_sprite_swarm.cpp |  | per-particle flames; projection via `FxSpritePublish` |
| 0x8002918C | LIVE | `beh_rand_phase_cull` | game/ai/beh_rand_phase_cull.cpp |  |  |
| 0x80029B40 | LIVE | `beh_pos_history_trail` | game/ai/beh_pos_history_trail.cpp |  |  |
| 0x8002AB5C | LIVE | `DropShadow::draw` | game/render/drop_shadow.cpp |  0x800318A0 0x80027768 | Actor drop shadow: mesh 0x8009FAE8 scaled by the node ramp, faded by IR0. |
| 0x8002B278 | LIVE | `Cull::coneCullBody` | game/render/cull.cpp |  | standalone view-CONE cull (3.9% field hot). a0 = node. The multiply-fo… |
| 0x8002B278 | LIVE | `Cull::coneCull2b278` | game/render/cull.cpp |  |  |
| 0x8003116C | LIVE | `Spawn::spawnAndInitBody` | game/world/spawn.cpp | 0x80028E10 | SPAWN-AND-INIT helper: spawn a type-6 object on list 1 (via the owned … |
| 0x8003116C | LIVE | `Spawn::spawnAndInit` | game/world/spawn.cpp |  |  |
| 0x80031558 | LIVE | `Spawn::spawnEffectChild` | game/world/spawn.cpp | 0x8007A980 | Spawn::spawnEffectChild. One of the near-identical MAIN.EXE "spawn a c… |
| 0x80031708 | LIVE | `ScriptInterp::refreshCachedTailHi` | game/scene/script_interp.cpp |  | ORACLE: guest 0x80031708 |
| 0x80031744 | LIVE | `ScriptInterp::refreshCachedTailLo` | game/scene/script_interp.cpp |  | ORACLE: guest 0x80031744 |
| 0x80031780 | LIVE | `Collision::listScan` | game/player/collision.cpp | 0x80031780 | list-tail resolver / reset. Walks the 8-byte-stride linked list rooted… |
| 0x800318A0 | LIVE | `ObjModelView::composeIntoGte` | game/render/obj_model_view.cpp |  | ORACLE: guest 0x800318A0 (tools/dynamic differential evidence equivale… |
| 0x80032A44 | LIVE | `Rng::inRange` | game/math/rng.cpp |  | scaled random. Disas 0x80032A44..0x80032A84 verbatim: `sra v0, 15` on … |
| 0x80036DFC | LIVE | `SaveMenu::runHandler` | game/ui/save_menu.cpp |  |  |
| 0x80036DFC | LIVE | `SaveMenu::dispatchBody` | game/ui/save_menu.cpp |  |  |
| 0x80039F4C | LIVE | `ov_textLabelEmit` | game/render/text_label.cpp |  |  |
| 0x80039F4C | LIVE | `Render::textLabelEmit` | game/render/text_label.cpp |  |  |
| 0x8003AD48 | LIVE | `beh_cube_text_spawn` | game/ai/beh_cube_text_spawn.cpp | 0x8003A790 0x8003A9A0 0x8003ABE4 0x8009A730 |  |
| 0x8003B054 | LIVE | `QuadRtptSubmit::rotateQuadCorners` | game/render/quad_rtpt_submit.cpp |  |  |
| 0x8003B054 | LIVE | `QuadRtptSubmit::registerOverrides` | game/render/quad_rtpt_submit.cpp |  | Wiring (frontier, 2026-07-08): both leaves are reached only via direct… |
| 0x8003B320 | LIVE | `QuadRtptSubmit::submitQuad` | game/render/quad_rtpt_submit.cpp |  |  |
| 0x8003BB50 | LIVE | `Render::objListWalk1` | game/render/objlist_walk.cpp | 0x8002AE0C 0x8003C5F8 0x8003C788 0x80122974 |  |
| 0x8003BB50 | LIVE | `ov_objListWalk1` | game/render/objlist_walk.cpp |  |  |
| 0x8003BCF4 | LIVE | `Render::objListWalk2` | game/render/objlist_walk.cpp | 0x80123C14 0x801341E8 0x80136748 |  |
| 0x8003BCF4 | LIVE | `ov_objListWalk2` | game/render/objlist_walk.cpp |  |  |
| 0x8003BF00 | LIVE | `Render::objListWalk3` | game/render/objlist_walk.cpp | 0x8003C5F8 0x8003C788 0x8004CC88 0x8010FC70 |  |
| 0x8003BF00 | LIVE | `ov_objListWalk3` | game/render/objlist_walk.cpp |  |  |
| 0x8003C048 | LIVE | `Render::renderWalk` | game/render/render_walk_dispatch.cpp | 0x80039F4C 0x8003C5F8 0x8003C788 0x8003EF9C 0x8003F174 0x800726D4 … |  |
| 0x8003C048 | LIVE | `ov_renderWalk` | game/render/render_walk_dispatch.cpp |  |  |
| 0x8003C2D4 | LIVE | `Render::billboardCompose1` | game/render/perobj_billboard.cpp |  |  |
| 0x8003C2D4 | LIVE | `ov_billboardCompose1` | game/render/perobj_billboard.cpp |  |  |
| 0x8003C464 | LIVE | `Render::billboardCompose2` | game/render/perobj_billboard.cpp | 0x800517BC |  |
| 0x8003C464 | LIVE | `ov_billboardCompose2` | game/render/perobj_billboard.cpp |  |  |
| 0x8003C5F8 | LIVE | `Render::billboardComposeC5F8` | game/render/perobj_billboard.cpp |  |  |
| 0x8003C5F8 | LIVE | `ov_billboardComposeC5F8` | game/render/perobj_billboard.cpp |  |  |
| 0x8003C788 | LIVE | `Render::billboardCompose3` | game/render/perobj_billboard.cpp |  |  |
| 0x8003C788 | LIVE | `ov_billboardCompose3` | game/render/perobj_billboard.cpp |  |  |
| 0x8003C8F4 | LIVE | `Render::billboardEmit` | game/render/perobj_billboard.cpp | 0x8003B054 0x8003B220 |  |
| 0x8003C8F4 | LIVE | `ov_billboardEmit` | game/render/perobj_billboard.cpp |  |  |
| 0x8003CCA4 | LIVE | `Render::perObjRenderDispatch` | game/render/perobj_billboard.cpp |  |  |
| 0x8003CCA4 | LIVE | `ov_perObjRenderDispatch` | game/render/perobj_billboard.cpp |  | Engine/game natives installed into the per-Core image-qualified runtim… |
| 0x8003CDD8 | LIVE | `MarginRenderer::collect` | game/render/margin_render.cpp |  | Record a re-include-eligible node (deduped within the frame). FILTER t… |
| 0x8003CDD8 | LIVE | `Render::cmdListDispatch` | game/render/perobj_dispatch.cpp |  | per-object cmd-list dispatch: composes the WORLD object transform (cam… |
| 0x8003CDD8 | LIVE | `ov_cmdListDispatch` | game/render/perobj_dispatch.cpp |  |  |
| 0x8003D0BC | LIVE | `Render::overlayTypeDispatch` | game/render/overlay_type_dispatch.cpp | 0x8010AA20 0x8010B0B8 0x8010B5BC 0x8010BA40 0x8010C2A4 0x8011024C … |  |
| 0x8003D0BC | LIVE | `ov_overlayTypeDispatch` | game/render/overlay_type_dispatch.cpp |  |  |
| 0x8003D584 | LIVE | `Render::effectColorAdd` | game/render/effect_mod.cpp |  | modulate each colour channel by the node's per-channel amount, rather … |
| 0x8003EEC0 | LIVE | `Render::objListWalk4` | game/render/objlist_walk.cpp | 0x8003B704 |  |
| 0x8003EEC0 | LIVE | `ov_objListWalk4` | game/render/objlist_walk.cpp |  |  |
| 0x8003EF9C | LIVE | `Render::composeTintGate` | game/render/compose_tint_gate.cpp | 0x8003D584 0x8003F07C | ORACLE: guest 0x8003EF9C |
| 0x8003F07C | LIVE | `Render::sharedTransformWalk` | game/render/subpart_walk_shared.cpp | 0x8003F698 | ORACLE: guest 0x8003F07C |
| 0x8003F174 | LIVE | `Render::subPartWalk` | game/render/subpart_walk.cpp | 0x8003F698 | ORACLE: guest 0x8003F174 |
| 0x8003F344 | LIVE | `Render::effectClutSwap` | game/render/effect_mod.cpp |  | stamp the node's CLUT id onto every colour-bearing packet, repointing … |
| 0x8003F3F4 | LIVE | `Render::effectSemiOn` | game/render/effect_mod.cpp |  | turn semi-transparency ON for every colour-bearing packet in the span. |
| 0x8003F4C4 | LIVE | `Render::effectSemiOff` | game/render/effect_mod.cpp |  | the exact inverse: turn semi-transparency OFF. |
| 0x8003F594 | LIVE | `Render::effectFlatTint` | game/render/effect_mod.cpp |  | overwrite the packet's colour word(s) with one flat colour and force s… |
| 0x8003F698 | LIVE | `Render::resolvePerModeEmitter` | game/render/perobj_dispatch.cpp |  | WHICH GUEST EMITTER a cmd with this `flag` resolves to — the ONE encod… |
| 0x8003F698 | LIVE | `Render::perModeDispatch` | game/render/perobj_dispatch.cpp | 0x800803DC | per-mode render dispatcher: routes to the area's per-mode renderer (mo… |
| 0x8003F698 | LIVE | `ov_perModeDispatch` | game/render/perobj_dispatch.cpp |  |  |
| 0x8003F9A8 | LIVE | `Render::frame` | game/render/render_frame.cpp |  | per-frame render orchestrator. The render-queue WALK passes (0x8003bf0… |
| 0x8003FA44 | LIVE | `Render::frameX` | game/render/render_frame.cpp |  | mid-transition render orchestrator twin (reduced pass set). Same rule:… |
| 0x80040558 | LIVE | `PlacedPropSm::step` | game/ai/placed_prop_sm.cpp |  | ORACLE: guest 0x80040558 |
| 0x80040A58 | LIVE | `SceneEvents::classSize` | game/scene/scene_events.cpp |  |  |
| 0x80040AA4 | LIVE | `CubeTextLedger::spawnPopup` | game/object/cube_text_ledger.cpp |  |  |
| 0x80040B48 | LIVE | `SceneEvents::armBody` | game/scene/scene_events.cpp |  |  |
| 0x80040B48 | LIVE | `SceneEvents::arm` | game/scene/scene_events.cpp |  |  |
| 0x80040B48 | LIVE | `SceneEvents::armOverride` | game/scene/scene_events.cpp |  | override entry (guest ABI: slot in r4, ret in r2). Single canonical bo… |
| 0x80040C00 | LIVE | `CubeTextLedger::deactivateSlot` | game/object/cube_text_ledger.cpp |  |  |
| 0x80040CDC | LIVE | `ScriptInterp::init` | game/scene/script_interp.cpp |  |  |
| 0x80040DE0 | LIVE | `ScriptInterp::loadCurrentEntry` | game/scene/script_interp_entry.cpp |  |  |
| 0x80040E54 | LIVE | `ScriptInterp::loadNextEntry` | game/scene/script_interp.cpp |  | loadNextEntry(obj, kindArg): THE ENTRY ADVANCE. 1:1 with guest 0x80040… |
| 0x80040FA0 | LIVE | `ScriptInterp::advanceStep` | game/scene/script_interp_entry.cpp | 0x80040E54 | VERIFIED + WIRED (frontier tier, 2026-07-10; advanceEntry() now calls … |
| 0x80041098 | LIVE | `beh_script_interp_step` | game/scene/script_interp.cpp |  |  |
| 0x80041098 | LIVE | `ScriptInterp::step` | game/scene/script_interp.cpp |  |  |
| 0x800412CC | LIVE | `ScriptInterp::callFnptr` | game/scene/script_interp.cpp |  |  |
| 0x8004139C | LIVE | `ScriptInterp::stepAngleToward` | game/scene/script_interp.cpp |  | leaf angle-stepper (no guest frame). See script_interp.h for the seman… |
| 0x80041438 | LIVE | `ScriptInterp::turnFacing` | game/scene/script_interp.cpp |  | thin wrapper: turnFacing(obj, targetAngle, step) = stepAngleToward(obj… |
| 0x80041438 | LIVE | `ScriptInterp::turnFacingFramed` | game/scene/script_interp.cpp |  | Guest-ABI twin — mirrors FUN_80041438's own sp-=24 / ra-spill-at-+16 f… |
| 0x80041468 | LIVE | `ScriptInterp::op31TurnTowardTarget` | game/scene/script_interp.cpp | 0x80085690 | op31 — FUN_80041468 (opcode table index 31). See script_interp.h for t… |
| 0x8004190C | LIVE | `Engine::animTick` | game/ai/sop_intro_events.cpp |  | Engine::animTick — FUN_8004190C. Ticks the animation VM (native |
| 0x8004201C | LIVE | `ScriptInterp::op04SceneFlagRendezvous` | game/scene/script_interp.cpp |  | the SCENE-FLAG RENDEZVOUS opcode (table index 4). 1:1 with authenticat… |
| 0x80042090 | LIVE | `ScriptInterp::op05WaitFrames` | game/scene/script_interp.cpp |  | VERIFIED + WIRED (frontier tier, 2026-07-10; return-value fix 2026-07-… |
| 0x800420AC | LIVE | `ScriptInterp::op06TestSceneFlag` | game/scene/script_interp.cpp |  | VERIFIED + WIRED (frontier tier, 2026-07-10). 1:1 with authenticated e… |
| 0x80042170 | LIVE | `ScriptInterp::matchesActiveByKind` | game/scene/script_interp.cpp |  | ORACLE: guest 0x80042170 |
| 0x80042258 | LIVE | `SceneEvents::delayedTrigger` | game/scene/scene_events.cpp |  | ORACLE: guest 0x80042258 |
| 0x80042258 | LIVE | `SceneEvents::delayedTriggerOverride` | game/scene/scene_events.cpp |  |  |
| 0x80042310 | LIVE | `ActorTomba::resetLoadGate` | game/player/actor_tomba.cpp |  | resetLoadGate — guest FUN_80042310. See actor_tomba.h for the full RE … |
| 0x80042448 | LIVE | `SceneEvents::applyFlagOp` | game/scene/scene_events.cpp |  | ORACLE: guest 0x80042448 |
| 0x80042448 | LIVE | `SceneEvents::applyFlagOpOverride` | game/scene/scene_events.cpp |  |  |
| 0x80042728 | LIVE | `BgSceneTransitionSm::readyForProgress` | game/scene/bg_scene_transition_sm.cpp |  |  |
| 0x80042758 | LIVE | `BgSceneTransitionSm::opSceneEventArmWait` | game/scene/bg_scene_transition_sm.cpp | 0x80040B48 0x80042728 | - Cutscene-script opcode leaves (adjacent to readyForProgress in the g… |
| 0x80042884 | LIVE | `BgSceneTransitionSm::opClearSceneFlag80a` | game/scene/bg_scene_transition_sm.cpp |  | opClearSceneFlag80a (FUN_80042884) — one-shot opcode leaf: clear the s… |
| 0x80042E10 | LIVE | `ScriptInterp::op34ClaimGate` | game/scene/script_interp.cpp |  | VERIFIED + WIRED (frontier tier, 2026-07-10; §9 re-verify caught+fixed… |
| 0x80042EA4 | LIVE | `ScriptInterp::stepEventPulse` | game/scene/script_interp.cpp |  | see script_interp.h for the full semantics writeup. |
| 0x80042EA4 | LIVE | `ScriptInterp::stepEventPulseFramed` | game/scene/script_interp.cpp |  | Guest-ABI twin — mirrors FUN_80042EA4's own sp-=24 / ra-spill-at-+16 f… |
| 0x80043108 | LIVE | `ScriptInterp::op36MoveTowardScriptTarget` | game/scene/script_interp.cpp | 0x80084080 0x80085690 | op36 — FUN_80043108 (opcode table index 36). See script_interp.h for t… |
| 0x80044090 | LIVE | `ScriptInterp::mirrorGlobalStatusByte` | game/scene/script_interp.cpp |  | ORACLE: guest 0x80044090 |
| 0x80044BD4 | LIVE | `Demo::s0PreYield` | game/scene/demo.cpp |  |  |
| 0x80044BD4 | LIVE | `FieldTransition::areaLoadBd4` | game/scene/field_transition.cpp |  | Native replacement for FUN_80044bd4(0x800452c0, area, mode, 1): seed t… |
| 0x80044BD4 | LIVE | `StartBinStage::advanceWithBootPreload` | game/scene/start_bin_stage.cpp |  | native_sync only — the pc_faithful body splits these writes across the… |
| 0x80044D8C | LIVE | `Asset::lzDecompress` | game/core/assets/asset.cpp |  |  |
| 0x80044E84 | LIVE | `Asset::unpackGroup` | game/core/assets/asset.cpp | 0x80080F6C | PC-owned texture-group unpacker — replaces guest FUN_80044E84 (0x80044… |
| 0x80044E84 | LIVE | `Asset::unpackGroupFaithful` | game/core/assets/asset.cpp | 0x80080F6C 0x80081218 | FAITHFUL texture-group unpacker — FUN_80044E84 with full guest-stack d… |
| 0x80044F58 | LIVE | `Asset::loadTexgroup` | game/core/assets/asset.cpp | 0x8001DC40 | PC-native TEXTURE-GROUP LOADER — owns the asset-load ORCHESTRATION FUN… |
| 0x80044F58 | LIVE | `Asset::preloadTexgroup` | game/core/assets/asset.cpp |  | texture-group load, synchronous. (Mirrors loadTexgroup but driven by e… |
| 0x8004514C | LIVE | `Asset::preloadStage1` | game/core/assets/asset.cpp |  | the stage-1 callback body. SWDATA + DAT load, shared texgroup sub-load… |
| 0x8004514C | LIVE | `Asset::preloadStage1` | game/core/assets/asset.cpp | 0x8001DC40 0x800754F4 | FUN_8004514C run synchronously: SWDATA + DAT load, texgroup sub-load, relocation, cel/sprite VRAM build |
| 0x80045258 | LIVE | `Asset::loadDescriptorChunk` | game/core/assets/asset.cpp |  | loadDescriptorChunk(descIdx, slot): FAITHFUL FUN_80045258 — a leaf ind… |
| 0x800452C0 | LIVE | `FieldTransition::areaLoadBd4` | game/scene/field_transition.cpp | 0x8001DC40 | the area-data task FUN_80044BD4 spawns, replaced by a synchronous load |
| 0x80045558 | LIVE | `ov_loadAreaSlotFile` | game/core/assets/asset.cpp |  | (idx): load indexed file idx (0 = OPN, 1 = CRD) into the shared AREA s… |
| 0x80045558 | LIVE | `Asset::registerOverrides` | game/core/assets/asset.cpp |  |  |
| 0x80045580 | LIVE | `ActorTomba::ov_turnBiasCompute` | game/player/actor_tomba.cpp |  | ov_turnBiasCompute/ov_outerTransitionGate/ov_outerTransitionCommit/ov_… |
| 0x80045580 | LIVE | `ActorTomba::assetReady` | game/player/actor_tomba.cpp |  | assetReady — guest FUN_80045580. See actor_tomba.h for the full RE wri… |
| 0x8004766C | LIVE | `Collision::snapObjectToTerrain` | game/player/collision.cpp | 0x80047778 0x80047CBC 0x80048034 0x80048134 0x80049968 | Collision::snapObjectToTerrain. THE object-level entry point of the gr… |
| 0x8004798C | LIVE | `Collision::gridStep` | game/player/collision.cpp | 0x8004798C |  |
| 0x80047CBC | LIVE | `Collision::gridQuery` | game/player/collision.cpp | 0x80047CBC |  |
| 0x800498C8 | LIVE | `Collision::gridResolve` | game/player/collision.cpp | 0x800498C8 |  |
| 0x80049968 | LIVE | `Collision::gridSetup` | game/player/collision.cpp | 0x80049968 | collision-grid ROW-POINTER setup. a0 = grid/layer index (&0xff). Reads… |
| 0x800499E8 | LIVE | `Engine::task0Bootstrap` | game/core/engine/engine_frame_ticks.cpp |  | resolve \BIN\START.BIN natively, record its {LBA,size}, switch |
| 0x80049A60 | LIVE | `ActorReward::smWindowScroll` | game/object/actor_sm_reward.cpp |  | ActorReward::smWindowScroll(c) — FUN_80049A60(obj a0, side a1). Scroll… |
| 0x80049E54 | LIVE | `ActorReward::smTallyTick` | game/object/actor_sm_reward.cpp |  | ActorReward::smTallyTick(c) — FUN_80049E54(obj a0, step a1) -> v0. Tic… |
| 0x8004A3D4 | LIVE | `ActorReward::smEventDispatch` | game/object/actor_sm_reward.cpp |  | ActorReward::smEventDispatch(c) — FUN_8004A3D4(obj a0) -> v0. Mechanic… |
| 0x8004B150 | LIVE | `ActorReward::smBlinkA` | game/object/actor_sm_reward.cpp |  | ActorReward::smBlinkA(c) — FUN_8004B150(obj a0, side a1). One-shot ini… |
| 0x8004B208 | LIVE | `ActorReward::smBlinkB` | game/object/actor_sm_reward.cpp |  | ActorReward::smBlinkB(c) — FUN_8004B208(obj a0, side a1). Same shape a… |
| 0x8004B3F4 | LIVE | `Spawn::dropScoreGem` | game/world/spawn.cpp | 0x80071B44 | SCORE-GEM DROP wrapper. Every callsite passes one of the eight fixed A… |
| 0x8004BD64 | LIVE | `GraphicsBind::posComposeBody` | game/world/graphics_bind.cpp |  | per-object POSITION-COMPOSE + render-state refresh. RE'd from disas 0x… |
| 0x8004BD64 | LIVE | `GraphicsBind::posCompose` | game/world/graphics_bind.cpp |  |  |
| 0x8004C238 | LIVE | `beh_visibility_gate_dispatch` | game/ai/beh_visibility_gate_dispatch.cpp | 0x80049A60 0x80049E54 0x8004A118 0x8004A2A0 0x8004A3D4 0x8004B150 … |  |
| 0x8004C324 | LIVE | `state1_gate` | game/ai/beh_visibility_gate_dispatch.cpp |  | --- STATE 1 shared VISIBILITY GATE (the body at 0x8004c324 / c3a4 / c4… |
| 0x8004CE14 | LIVE | `beh_record_list_scanner` | game/ai/beh_record_list_scanner.cpp | 0x80111CCC |  |
| 0x8004D338 | LIVE | `Inventory::addNative` | game/items/inventory.cpp |  | PC-native reimplementation of FUN_8004D338 (inventory_add). Writes are… |
| 0x8004D338 | LIVE | `Inventory::addBody` | game/items/inventory.cpp |  | --- the FUN_8004D338 override + invverify gate -----------------------… |
| 0x8004D338 | LIVE | `Inventory::addEntry` | game/items/inventory.cpp |  |  |
| 0x8004D338 | LIVE | `Inventory::add` | game/items/inventory.cpp |  | --- PC-shape mutators: set the guest ABI regs and route through the st… |
| 0x8004D4C4 | LIVE | `Inventory::giveAndFlagBody` | game/items/inventory.cpp | 0x8004ED0C | give_and_flag(type, amount): native add, then dispatch the PSX flag/ev… |
| 0x8004D4C4 | LIVE | `Inventory::giveAndFlagEntry` | game/items/inventory.cpp |  |  |
| 0x8004D4C4 | LIVE | `Inventory::giveAndFlag` | game/items/inventory.cpp |  |  |
| 0x8004D4F4 | LIVE | `Inventory::giveBody` | game/items/inventory.cpp |  | give_only(type, amount): native add only. |
| 0x8004D4F4 | LIVE | `Inventory::giveEntry` | game/items/inventory.cpp |  |  |
| 0x8004D4F4 | LIVE | `Inventory::give` | game/items/inventory.cpp |  |  |
| 0x8004D7EC | LIVE | `Bit::test7EC` | game/math/mathlib.cpp | 0x8004D7EC | pure bitmap bit-test (~2%, 6.8k calls): byte = bitmap[(int16)(idx/8)] … |
| 0x8004D868 | LIVE | `Bit::test868` | game/math/mathlib.cpp | 0x8004D868 | sibling of FUN_8004D7EC (bit-test) against a fixed third bitmap @0x800… |
| 0x8004EB94 | LIVE | `emitSegmentLayout` | game/render/hud_gauge_emitter.cpp |  | (descAddr, sign_extend16(spanBase + spanBias + bias)) call shape, shar… |
| 0x8004ED0C | LIVE | `Inventory::abGate` | game/items/inventory.cpp |  | Full RAM+scratchpad A/B vs original guest-body call. The pure-leaf cor… |
| 0x8004ED94 | LIVE | `Engine::announcerCue` | game/core/engine/engine_object_leaves.cpp | 0x8004FA38 | Engine::announcerCue — FUN_8004ED94. `id` sign-extended s16, then time… |
| 0x8004FA38 | LIVE | `Inventory::abGate` | game/items/inventory.cpp |  | Full RAM+scratchpad A/B vs original guest-body call. The pure-leaf cor… |
| 0x8004FB20 | LIVE | `Pool::clearBf548Region` | game/world/pool.cpp |  | zero 700 bytes at 0x800BF548. Trivial memset wrapper. Every field of t… |
| 0x8004FB4C | LIVE | `HudGaugeEmitter::emitItem` | game/render/hud_gauge_emitter.cpp |  |  |
| 0x8004FD30 | LIVE | `HudGaugeEmitter::emitFrame` | game/render/hud_gauge_emitter.cpp |  |  |
| 0x8004FE84 | LIVE | `Engine::sceneRenderListBuilder` | game/core/engine/engine_scene_frame.cpp |  | Native FUN_8004FE84 — a 2-phase scene/render-list builder driver (stru… |
| 0x8004FFB4 | LIVE | `Panel::fillQuad` | game/ui/panel_fill.cpp |  | EQUIVALENCE. This is a REBUILD, not a transcription, so `port_check` c… |
| 0x8005082C | LIVE | `ModeStateArm::arm` | game/scene/mode_state_arm.cpp |  | ModeStateArm::arm — native ownership of FUN_8005082C (Ghidra decomp sc… |
| 0x800508A8 | LIVE | `ModeStateArm::armFromAreaTable` | game/scene/mode_state_arm.cpp |  | ModeStateArm::armFromAreaTable — native ownership of FUN_800508A8 (Ghi… |
| 0x80050970 | LIVE | `BgSceneTransitionSm::bf816Dispatch` | game/scene/bg_scene_transition_sm.cpp |  | tiny dispatcher on the 800BF816 mode byte: 0 = ModeStateArm::armFromAr… |
| 0x800509B4 | LIVE | `Engine::initDisplay` | game/scene/startup.cpp | 0x80050738 |  |
| 0x80050A0C | LIVE | `Engine::initFrameState` | game/scene/startup.cpp |  |  |
| 0x80050A80 | LIVE | `Engine::initCamera` | game/scene/startup.cpp |  | engine CAMERA init: identity camera-rotation matrix at scratchpad 0x1F… |
| 0x80050DE4 | LIVE | `Engine::sceneStateStep` | game/core/engine/engine_state_dispatch.cpp |  | Engine::sceneStateStep — the SCENE-INIT / SCENE-RUN state machine at g… |
| 0x80051128 | LIVE | `NodeXform::propagate` | game/render/node_xform.cpp |  | per-object CHILD-NODE TRANSFORM loop. RE'd from disas: |
| 0x80051300 | LIVE | `NodeXform::propagateRotmat` | game/render/node_xform.cpp |  | per-object CHILD-NODE TRANSFORM loop, rotmat-single-call variant. RE'd… |
| 0x80051464 | LIVE | `NodeXform::propagateAxis` | game/render/node_xform.cpp |  | sibling of propagateRotmat(): identical control flow, but the child's … |
| 0x80051614 | LIVE | `NodeXform::buildFromChild` | game/render/node_xform.cpp |  | RE'd from authenticated executable/overlay evidence guest 0x80051614 (… |
| 0x80051794 | LIVE | `Mtx::identity` | game/math/mtx.cpp |  |  |
| 0x80051794 | LIVE | `Mtx::registerOverrides` | game/math/mtx.cpp |  |  |
| 0x800517BC | LIVE | `NodeXform::seedBlock` | game/render/node_xform.cpp |  | trivial 8-word block seeder: {x,0,y,0,z,0,0,0}. RE'd + cross-checked v… |
| 0x800517F8 | LIVE | `GraphicsBind::renderUpdateBody` | game/world/graphics_bind.cpp | 0x80051300 | per-object RENDER-STATE UPDATE: build the object's transform, then sna… |
| 0x800517F8 | LIVE | `GraphicsBind::renderUpdate` | game/world/graphics_bind.cpp |  |  |
| 0x80051844 | LIVE | `NodeXform::build` | game/render/node_xform.cpp |  | REGISTER FAITHFULNESS (2026-07-08, the f117 residual root cause): fram… |
| 0x800518FC | LIVE | `NodeXform::buildWithOffset` | game/render/node_xform.cpp |  | NodeXform::buildWithOffset — PC-native reimpl of guest FUN_800518FC. |
| 0x800519E0 | LIVE | `GraphicsBind::recordArrayInit` | game/world/graphics_bind.cpp |  |  |
| 0x80051B04 | LIVE | `GraphicsBind::installSceneRecord` | game/world/graphics_bind.cpp |  | two-level scene-data-table pointer resolve. Pure address arithmetic, n… |
| 0x80051B34 | LIVE | `NodeXform::copyMatrixBlock` | game/render/node_xform.cpp |  | frameless leaf, verbatim from authenticated executable/overlay evidenc… |
| 0x80051B70 | LIVE | `GraphicsBind::recordInitBody` | game/world/graphics_bind.cpp |  | per-object render-record INIT. Allocates a record (FUN_8007AAE8), zero… |
| 0x80051B70 | LIVE | `GraphicsBind::recordInit` | game/world/graphics_bind.cpp |  |  |
| 0x80051C8C | LIVE | `NodeXform::buildAxis` | game/render/node_xform.cpp |  | node-level sibling of build(): composes THIS node's own world matrix v… |
| 0x80051D20 | LIVE | `NodeXform::worldPosFromComposed` | game/render/node_xform.cpp |  | sibling of worldPosFromLocal() using node's COMPOSED world matrix and … |
| 0x80051D90 | LIVE | `NodeXform::worldPosFromLocal` | game/render/node_xform.cpp |  | RE'd from authenticated executable/overlay evidence guest 0x80051D90 (… |
| 0x80052078 | LIVE | `Engine::startStage` | game/core/engine/engine_frame_ticks.cpp | 0x80080870 0x80080890 0x800808A0 | -- PC-native task-0 bootstrap: own the START.BIN resolve + stage-0 ove… |
| 0x800520E0 | LIVE | `Engine::initSubsystems` | game/scene/startup.cpp |  |  |
| 0x8005229C | LIVE | `Engine::padFenceTail` | game/input/pad_edge_fence.cpp | 0x80087AEC 0x80087E2C 0x80087EAC | Override wrapper + install (guest ABI is all-implicit — the fence take… |
| 0x8005229C | LIVE | `ov_padFenceTail` | game/input/pad_edge_fence.cpp |  |  |
| 0x800527C8 | LIVE | `beh_actor_tomba_proximity_combat` | game/ai/beh_actor_tomba_proximity_combat.cpp | 0x80041718 0x80041768 0x8004190C 0x80042728 0x800518FC 0x800519E0 … |  |
| 0x80053E50 | LIVE | `ActorTomba::outerTransitionGate` | game/player/actor_tomba.cpp |  |  |
| 0x80053FDC | LIVE | `ActorTomba::outerTransitionCommit` | game/player/actor_tomba.cpp |  | outerTransitionCommit — guest FUN_80053FDC(G, mode). See actor_tomba.h… |
| 0x80054198 | LIVE | `SceneTransition::clearSwapBlock` | game/scene/scene_transition.cpp |  | small swap-block ephemeral clear. RE'd from disas 0x80054198..0x800541… |
| 0x80054650 | LIVE | `ActorTomba::settleStep` | game/player/actor_tomba.cpp | 0x8004954C |  |
| 0x80054D14 | LIVE | `Engine::walkStart` | game/ai/sop_intro_events.cpp |  | Engine::walkStart — FUN_80054D14. |
| 0x80055C9C | LIVE | `gov_turnBiasCompute` | game/player/actor_tomba.cpp |  | installed via tomba::native::declareOverride() at game/player/actor_to… |
| 0x80056B48 | LIVE | `ActorTomba::velocityIntegrate` | game/player/actor_tomba.cpp |  |  |
| 0x80057DC0 | LIVE | `ActorTomba::growthStep` | game/player/actor_tomba.cpp |  |  |
| 0x80058304 | LIVE | `Engine::gStateMutate` | game/core/engine/engine_object_leaves.cpp | 0x800310F4 | Engine::gStateMutate — native ownership of FUN_80058304 (Ghidra decomp |
| 0x8005950C | LIVE | `ActorTomba::frameTick` | game/player/actor_tomba.cpp |  |  |
| 0x80059D28 | LIVE | `Engine::frameStartTick` | game/core/engine/engine_frame_ticks.cpp |  | Engine::frameStartTick — per-frame prologue at guest 0x80059D28 (FIRST… |
| 0x80059ED8 | LIVE | `beh_camera_target_follow` | game/ai/beh_camera_target_follow.cpp | 0x800312D4 0x800489E4 0x8010B238 0x8010BC10 0x8010C5A8 0x8011332C … |  |
| 0x8005A910 | LIVE | `ActorTomba::mode0ActionGate` | game/player/actor_tomba.cpp |  |  |
| 0x80067DA8 | LIVE | `Engine::uploadModeSprites` | game/ai/beh_sop_intro_pilot.cpp | 0x80081218 | Engine::uploadModeSprites — native ownership of FUN_80067DA8 (Ghidra d… |
| 0x8006C80C | LIVE | `CutsceneCamera::yFloor` | game/camera/cutscene_camera.cpp |  | yFloor (camera-Y floor clamp, per render mode) |
| 0x8006C988 | LIVE | `CutsceneCamera::shakeTail` | game/camera/cutscene_camera.cpp |  | post-mode TAIL (0x8006C988) — the camera SHAKE state machine |
| 0x8006CBA8 | LIVE | `CutsceneCamera::initSeedGrp` | game/camera/cutscene_camera.cpp |  |  |
| 0x8006CBD0 | LIVE | `GraphicsBind::setXformBlkBody` | game/world/graphics_bind.cpp |  | copy a 6-halfword TRANSFORM BLOCK from a1 into the scratchpad camera/t… |
| 0x8006CBD0 | LIVE | `GraphicsBind::setXformBlk` | game/world/graphics_bind.cpp |  |  |
| 0x8006D02C | LIVE | `CutsceneCamera::lookAt` | game/camera/cutscene_camera.cpp |  |  |
| 0x8006D2AC | LIVE | `CutsceneCamera::distSolve` | game/camera/cutscene_camera.cpp |  | distSolve (distance/zoom solver) |
| 0x8006D654 | LIVE | `CutsceneCamera::pitch` | game/camera/cutscene_camera.cpp |  | pitch (vertical-look height smoother) |
| 0x8006D934 | LIVE | `CutsceneCamera::snapAccXZ` | game/camera/cutscene_camera.cpp |  | orchestrators (per-frame camera modes) |
| 0x8006D950 | LIVE | `CutsceneCamera::snapAccY` | game/camera/cutscene_camera.cpp |  |  |
| 0x8006D960 | LIVE | `CutsceneCamera::trackXZ` | game/camera/cutscene_camera.cpp |  | follow accumulators |
| 0x8006DA54 | LIVE | `CutsceneCamera::trackY` | game/camera/cutscene_camera.cpp |  |  |
| 0x8006DAD8 | LIVE | `CutsceneCamera::posBuildB` | game/camera/cutscene_camera.cpp |  |  |
| 0x8006DC38 | LIVE | `CutsceneCamera::posBuildA` | game/camera/cutscene_camera.cpp |  | scripted-camera look-angle builders (0x8006DC38/DAD8/DF88/DEF0 — us… |
| 0x8006DCF4 | LIVE | `CutsceneCamera::heading` | game/camera/cutscene_camera.cpp |  | heading (heading tracker) |
| 0x8006DEF0 | LIVE | `CutsceneCamera::headBuildB` | game/camera/cutscene_camera.cpp |  |  |
| 0x8006DF88 | LIVE | `CutsceneCamera::headBuildA` | game/camera/cutscene_camera.cpp |  |  |
| 0x8006E010 | LIVE | `CutsceneCamera::angleStep` | game/camera/cutscene_camera.cpp |  | angleStep |
| 0x8006E0F0 | LIVE | `CutsceneCamera::mainFollow` | game/camera/cutscene_camera.cpp |  |  |
| 0x8006E1C0 | LIVE | `CutsceneCamera::pushMode` | game/camera/cutscene_camera.cpp |  |  |
| 0x8006E1E4 | LIVE | `CutsceneCamera::restoreMode` | game/camera/cutscene_camera.cpp |  |  |
| 0x8006E228 | LIVE | `CutsceneCamera::trackFollow` | game/camera/cutscene_camera.cpp |  |  |
| 0x8006E294 | LIVE | `CutsceneCamera::snapFollowA` | game/camera/cutscene_camera.cpp |  |  |
| 0x8006E2FC | LIVE | `CutsceneCamera::snapFollowB` | game/camera/cutscene_camera.cpp |  |  |
| 0x8006E360 | LIVE | `CutsceneCamera::pitchFollow` | game/camera/cutscene_camera.cpp |  |  |
| 0x8006E3B0 | LIVE | `CutsceneCamera::snapFollow` | game/camera/cutscene_camera.cpp |  |  |
| 0x8006E3F4 | LIVE | `CutsceneCamera::simpleFollow` | game/camera/cutscene_camera.cpp |  |  |
| 0x8006E464 | LIVE | `CutsceneCamera::rotBuild` | game/camera/cutscene_camera.cpp |  |  |
| 0x8006E8F8 | LIVE | `CutsceneCamera::resetFollowAccum` | game/camera/cutscene_camera.cpp |  | Wiring pass (2026-07-08 frontier follow-up) |
| 0x8006E918 | LIVE | `CutsceneCamera::initPlace` | game/camera/cutscene_camera.cpp |  |  |
| 0x8006EA00 | LIVE | `CutsceneCamera::snapToMasterOffsetY200` | game/camera/cutscene_camera.cpp |  | pushes a real 32-byte guest frame (r29-=32, s0/s1/ra spilled at +16/+2… |
| 0x8006EA7C | ORPHAN | `CutsceneCamera::init` | game/camera/cutscene_camera.cpp |  |  |
| 0x8006EC44 | LIVE | `CutsceneCamera::update` | game/camera/cutscene_camera.cpp |  |  |
| 0x8006EC44 | LIVE | `CutsceneCamera::updateFaithful` | game/camera/cutscene_camera.cpp | 0x8006C988 0x8006EA7C | pc_faithful mirror of guest 0x8006EC44 (authenticated executable/overl… |
| 0x8006EF38 | LIVE | `CutsceneCamera::orbitTick` | game/camera/cutscene_camera.cpp |  | pushes the same shape of 32-byte frame (r29-=32, s0/s1/ra spilled at +… |
| 0x8006EFF4 | LIVE | `Bit::testFE48` | game/math/mathlib.cpp |  | u32 flag-bit TEST on the fixed 32-bit word at 0x800BFE48. Pure 5-instr… |
| 0x8006F00C | LIVE | `Bit::setFE48` | game/math/mathlib.cpp |  | sibling of setFE34: u32 flag-bit SET on 0x800BFE48 (the word testFE48 … |
| 0x8006F02C | LIVE | `Bit::setFE34` | game/math/mathlib.cpp |  | u32 flag-bit SET on the fixed 32-bit word at 0x800BFE34. 7-instruction… |
| 0x8006F04C | LIVE | `Bit::processLinkRequest` | game/math/mathlib.cpp |  | child-link REQUEST-mailbox arbiter. disas 0x8006F04C..0x8006F0E0: |
| 0x8006F2D0 | LIVE | `beh_pad_child_linker` | game/ai/beh_pad_child_linker.cpp | 0x8004766C 0x80047B5C 0x8006F138 |  |
| 0x80070018 | LIVE | `ActorReward::update` | game/object/actor_sm_reward.cpp |  |  |
| 0x800702C0 | LIVE | `ActorReward::resolvePosition` | game/object/actor_sm_reward.cpp |  |  |
| 0x80070650 | LIVE | `ActorReward::approachTargetX` | game/object/actor_sm_reward.cpp |  | ActorReward::approachTargetX(c) — FUN_80070650(obj a0). Trivial ease: … |
| 0x80071A3C | LIVE | `beh_area_event_dispatch` | game/ai/beh_area_event_dispatch.cpp | 0x800716B4 0x80071768 0x801178E4 0x8011B79C |  |
| 0x80072A78 | LIVE | `Placement::placeAreaObjects` | game/world/placement.cpp | 0x80072A78 |  |
| 0x80072DDC | LIVE | `Placement::spawnWithParent` | game/world/placement.cpp | 0x80072DDC |  |
| 0x80073194 | LIVE | `ScriptInterp::advanceGauge` | game/scene/script_interp.cpp | 0x80074590 | ORACLE: guest 0x80073194 |
| 0x80073260 | LIVE | `SceneTransition::resetSwap` | game/scene/scene_transition.cpp |  |  |
| 0x800732C0 | LIVE | `SceneTransition::beginSwap` | game/scene/scene_transition.cpp |  |  |
| 0x80073300 | LIVE | `SceneTransition::completeSwap` | game/scene/scene_transition.cpp |  |  |
| 0x80073328 | LIVE | `SceneTransition::stepSwapWaiter` | game/scene/scene_transition.cpp | 0x80073328 |  |
| 0x800735F4 | LIVE | `Spawn::tickLinkedOverlay` | game/world/spawn.cpp |  | per-object controller that owns exactly ONE linked "variant overlay" c… |
| 0x80073750 | LIVE | `Font::measureLineWidth` | game/ui/font.cpp |  | pure string measurer (disas 0x80073750..0x80073798, no sub-calls): |
| 0x800739AC | LIVE | `beh_scene_ui_trigger` | game/ai/beh_scene_ui_trigger.cpp | 0x800737F8 0x800738B0 0x80074BF8 |  |
| 0x80073CD8 | LIVE | `beh_typed_init_scene_trigger` | game/ai/beh_typed_init_scene_trigger.cpp |  |  |
| 0x800741DC | LIVE | `beh_pickup_collect_trigger` | game/ai/beh_pickup_collect_trigger.cpp |  |  |
| 0x80074590 | LIVE | `Sfx::trigger` | game/audio/sfx.cpp | 0x80074BF8 0x80074EEC 0x80075E04 |  |
| 0x80074810 | LIVE | `Sfx::triggerPanned` | game/audio/sfx.cpp | 0x80074590 | ORACLE: guest 0x80074810 |
| 0x80074810 | LIVE | `Sfx::registerOverrides` | game/audio/sfx.cpp |  |  |
| 0x8007496C | LIVE | `AreaSlots::updateCell` | game/world/area_slots.cpp | 0x80092E3C | AreaSlots::updateCell — FUN_8007496C body. sigArg carries {idx: low by… |
| 0x80074A38 | LIVE | `AreaSlots::primeCountdown` | game/world/area_slots.cpp |  | AreaSlots::primeCountdown — FUN_80074A38 body. Pure 1-store leaf: tabl… |
| 0x80074A38 | LIVE | `AreaSlots::registerOverrides` | game/world/area_slots.cpp |  |  |
| 0x80074AF0 | LIVE | `AreaSlots::ackIfMatch` | game/world/area_slots.cpp |  | AreaSlots::ackIfMatch — FUN_80074AF0 body. Pure 21-instruction primiti… |
| 0x80074BC4 | LIVE | `AudioDispatch::settleField` | game/audio/audio_dispatch.cpp | 0x8001CF2C 0x80074B44 0x80074E48 | AudioDispatch::settleField — native ownership of FUN_80074BC4 (Ghidra … |
| 0x80074F24 | LIVE | `Pool::selectStateIndex` | game/world/pool.cpp |  | per-area STATE-INDEX select + apply. Early-out if scratchpad 0x1F80013… |
| 0x80075024 | LIVE | `AudioDispatch::selectStateRemap` | game/audio/audio_dispatch.cpp | 0x800750D8 | AudioDispatch::selectStateRemap — native ownership of FUN_80075024. Ma… |
| 0x80075070 | LIVE | `AudioDispatch::publishStateFade` | game/audio/audio_dispatch.cpp | 0x80075CEC | AudioDispatch::publishStateFade — native ownership of FUN_80075070. Pu… |
| 0x800750A4 | LIVE | `AudioDispatch::selectState` | game/audio/audio_dispatch.cpp |  | AudioDispatch::selectState — native ownership of FUN_800750A4 (Ghidra … |
| 0x800750D8 | LIVE | `AudioDispatch::dispatch3Way` | game/audio/audio_dispatch.cpp | 0x8001CF2C | AudioDispatch::dispatch3Way — native ownership of FUN_800750D8 (Ghidra… |
| 0x80075130 | LIVE | `Font::init` | game/ui/font.cpp |  | font / text system init orchestrator. No args, no return. Mirrors the … |
| 0x80075240 | LIVE | `Pool::reset75240` | game/world/pool.cpp |  | reset the control block at 0x800BE1F8: call 0x80075D58 leaf, seed clam… |
| 0x800752B4 | LIVE | `Font::glyphClassFill` | game/ui/font.cpp |  | glyph-class table fill. Iterates i = 0..23 over the 24-entry table. Th… |
| 0x800753AC | LIVE | `preload_build_vram` | game/core/assets/asset.cpp | 0x80075448 | cel/sprite VRAM build, synchronous. FUN_800753ac is itself an async CD… |
| 0x800753D4 | LIVE | `preload_cel` | game/core/assets/asset.cpp | 0x80096480 0x80096980 0x80096A40 | cel-load, SYNCHRONOUS. Original: FUN_80096480 (slot alloc + BAV cel lo… |
| 0x800753D4 | LIVE | `preload_build_vram` | game/core/assets/asset.cpp | 0x80075448 | cel/sprite VRAM build, synchronous. FUN_800753ac is itself an async CD… |
| 0x80075448 | LIVE | `preload_build_vram` | game/core/assets/asset.cpp | 0x80075448 | cel/sprite VRAM build, synchronous. FUN_800753ac is itself an async CD… |
| 0x800754F4 | LIVE | `preload_build_vram` | game/core/assets/asset.cpp | 0x80075448 | cel/sprite VRAM build, synchronous. FUN_800753ac is itself an async CD… |
| 0x80075824 | LIVE | `MusicCoord::voiceMixTick` | game/audio/music_coord.cpp |  | Per-frame VOICE-CHANNEL VOLUME MIXER — port of FUN_80075824 (RE'd via … |
| 0x80075A80 | LIVE | `AreaSlots::updateTail` | game/world/area_slots.cpp | 0x80074BF8 0x80074E48 0x8008E0C0 0x80092660 0x80098F90 0x80099490 … | AreaSlots::updateTail — the last direct child of ov_field_frame at gue… |
| 0x80075CEC | LIVE | `BgSceneTransitionSm::audioFadeTarget` | game/scene/bg_scene_transition_sm.cpp |  | - Native ports of the tiny sub-leaves this SM calls ------------------… |
| 0x80075D24 | LIVE | `MusicCoord::setGain2` | game/audio/music_coord.cpp |  | MusicCoord::setGain2 — FUN_80075D24 body. See music_coord.h for the RE… |
| 0x80075D24 | LIVE | `MusicCoord::registerOverrides` | game/audio/music_coord.cpp |  |  |
| 0x80075F0C | LIVE | `Animation::applyFrame` | game/object/animation.cpp |  |  |
| 0x80076904 | LIVE | `Animation::loadFrame` | game/object/animation.cpp |  |  |
| 0x80076904 | LIVE | `Animation::registerOverrides` | game/object/animation.cpp |  |  |
| 0x80076D68 | LIVE | `Animation::stepFramed` | game/object/animation.cpp |  | Animation::stepFramed — GUEST-ABI ENTRY ONLY for FUN_80076D68 (RE: aut… |
| 0x8007703C | LIVE | `Cull::enqueueByClass` | game/render/cull.cpp |  | Cull::enqueueByClass — PC-native FUN_8007703C body. Class-keyed queue … |
| 0x8007712C | LIVE | `Cull::decide` | game/render/cull.cpp |  | Pure (read-only) cull decision — reproduces FUN_8007712c's control flo… |
| 0x8007712C | LIVE | `Cull::performBaseCull` | game/render/cull.cpp |  | Cull::performBaseCull — byte-exact PC-native FUN_8007712C body (no mar… |
| 0x8007712C | LIVE | `Cull::objectCull` | game/render/cull.cpp |  |  |
| 0x8007712C | LIVE | `Cull::performBaseCullFramed` | game/render/cull.cpp |  | performBaseCullFramed — mirrors FUN_8007712C's OWN real 40-byte guest-… |
| 0x80077768 | LIVE | `Trig::angleCmp` | game/math/trig.cpp |  |  |
| 0x8007778C | LIVE | `Cull::wrapFrame` | game/render/cull.cpp |  | camera-relative cull WRAPPER. Computes obj-cam delta (wrapping s16, si… |
| 0x8007778C | LIVE | `Cull::cullWrapper` | game/render/cull.cpp |  |  |
| 0x800777FC | LIVE | `Cull::cullWrapperFlag2` | game/render/cull.cpp |  | UNFRAMED — the public entry point EXISTING native beh_ callers (beh_id… |
| 0x80077870 | LIVE | `Cull::cullWrapperFlag1` | game/render/cull.cpp |  | cull-wrapper variant: byte-identical to cullWrapper (obj in c->r[4], d… |
| 0x800778E4 | LIVE | `Cull::cullWrapperOffsetY` | game/render/cull.cpp |  |  |
| 0x800779D0 | LIVE | `Cull::cullWrapperOffset` | game/render/cull.cpp |  |  |
| 0x80077A4C | LIVE | `Cull::cullWrapperOffsetFlag1` | game/render/cull.cpp |  |  |
| 0x80077ACC | LIVE | `Cull::cullWrap77acc` | game/render/cull.cpp |  | UNFRAMED — the public entry point EXISTING native callers (beh_record_… |
| 0x80077B38 | LIVE | `GraphicsBind::setGeomBody` | game/world/graphics_bind.cpp |  | set an object's GEOMETRY-BLOCK pointer from a table. RE'd from disas 0… |
| 0x80077B38 | LIVE | `GraphicsBind::setGeom` | game/world/graphics_bind.cpp |  |  |
| 0x80077B5C | LIVE | `Animation::advanceLinkChain` | game/object/animation.cpp |  |  |
| 0x80077C40 | LIVE | `Animation::attach` | game/object/animation.cpp | 0x80075FF8 |  |
| 0x80077E7C | LIVE | `Cull::enqueueQueueA` | game/render/cull.cpp |  | Cull::enqueueQueueA — PC-native FUN_80077E7C body. Manual push of `obj… |
| 0x80077EBC | LIVE | `Cull::enqueueVisibleClass4` | game/render/cull.cpp |  | Cull::enqueueVisibleClass4 — PC-native FUN_80077EBC body. Manual push … |
| 0x80077EFC | LIVE | `Cull::enqueueQueueC` | game/render/cull.cpp |  | Cull::enqueueQueueC — PC-native FUN_80077EFC body. Manual push onto qu… |
| 0x80077FB0 | LIVE | `eov_isqrt16` | game/math/gte_math.cpp |  | installed via tomba::native::declareOverride() at game/math/gte_math.c… |
| 0x80078240 | LIVE | `Trig::vecLen` | game/math/trig.cpp |  | vecLen (guest FUN_80078240) — the integer 3-D length approximation. Th… |
| 0x80078240 | LIVE | `eov_approxDist3` | game/math/gte_math.cpp |  | installed via tomba::native::declareOverride() at game/math/gte_math.c… |
| 0x800782F0 | LIVE | `SceneTransition::areaMaskTrigger` | game/scene/scene_transition.cpp | 0x800782F0 |  |
| 0x800783DC | LIVE | `Pool::setupViewScroll` | game/world/pool.cpp |  | per-area VIEW/SCROLL setup. Calls a leaf (0x80048D3C), builds the view… |
| 0x80078610 | LIVE | `Pool::finalViewInit` | game/world/pool.cpp |  | final per-area view init: zero two control blocks, seed fixed view par… |
| 0x80078824 | LIVE | `Engine::setAreaStartPos` | game/core/engine/engine_frame_ticks.cpp |  | Engine::setAreaStartPos. Loads the player's per-area spawn |
| 0x800788AC | LIVE | `Engine::padEdgeFence` | game/input/pad_edge_fence.cpp |  | per-frame input-edge fence. See the file header above for the full RE … |
| 0x800788AC | LIVE | `ov_padEdgeFence` | game/input/pad_edge_fence.cpp |  |  |
| 0x80078988 | LIVE | `Font::iconGlyphEmit` | game/ui/font.cpp |  | iconGlyphEmit — FUN_80078988, the SJIS/token ICON-GLYPH string emitter… |
| 0x80078CA8 | LIVE | `Font::glyphEmit` | game/ui/font.cpp | 0x80078988 0x80083DE0 | producer (a3 string, element = byte offset) |
| 0x80078CA8 | LIVE | `Font::registerOverrides` | game/ui/font.cpp |  |  |
| 0x80079324 | LIVE | `Font::drawTextSmall` | game/ui/font.cpp |  | ORACLE: guest 0x80079324 |
| 0x80079324 | LIVE | `ov_drawTextSmall` | game/ui/font.cpp |  | ov_drawTextSmall: sibling of ov_drawText for FUN_80079324 — same guest… |
| 0x80079374 | LIVE | `Font::drawText` | game/ui/font.cpp |  | WIDE-RE TIER DRAFT (2026-07-09), UNWIRED/UNVERIFIED. See header doc fo… |
| 0x80079374 | LIVE | `ov_drawText` | game/ui/font.cpp |  | ov_drawText: extracts drawText's typed args from the guest ABI registe… |
| 0x80079528 | LIVE | `Str::length` | game/core/assets/str.cpp |  | strlen. RE (tools/disas.py 0x80079528 --all 20, cross-checked against |
| 0x80079528 | LIVE | `ov_strLength` | game/core/assets/str.cpp |  |  |
| 0x800796DC | LIVE | `Pool::resetControlBlock` | game/world/pool.cpp |  | zero the 104-byte control block at 0x800BF808, seed two bytes, clear ~… |
| 0x800798F8 | LIVE | `Pool::initTypedPools` | game/world/pool.cpp |  | the 5 typed object pools + list-head init. See pool.h for the pool tab… |
| 0x80079C3C | LIVE | `Spawn::spawnLinkStamp` | game/world/spawn.cpp |  | Link `node` into active list `list` at position `mode` relative to `re… |
| 0x80079C3C | LIVE | `Spawn::entitySpawnBody` | game/world/spawn.cpp |  |  |
| 0x80079DDC | LIVE | `Spawn::spawnPool2Body` | game/world/spawn.cpp |  |  |
| 0x80079F90 | LIVE | `Spawn::poolSpawn` | game/world/spawn.cpp |  |  |
| 0x8007A624 | LIVE | `Spawn::despawn` | game/world/spawn.cpp | 0x8007A624 |  |
| 0x8007A980 | LIVE | `Spawn::dispatch` | game/world/spawn.cpp |  | Run the per-class spawn VARIANT NATIVELY (the 5 bodies are all owned i… |
| 0x8007AAE8 | LIVE | `GraphicsBind::recordAlloc` | game/world/graphics_bind.cpp |  |  |
| 0x8007AAE8 | LIVE | `RenderRecordPool::allocateForGuest` | game/world/render_record_pool.cpp |  | FUN_8007AAE8 as guest code calls it: pops the render-record free stack and begins the record's next life. |
| 0x8007B008 | LIVE | `ObjectList::walkList2` | game/object/object_list.cpp |  |  |
| 0x8007B04C | LIVE | `TransitionState3::walkOnce` | game/scene/transition_state3.cpp |  |  |
| 0x8007B18C | LIVE | `Pool::init` | game/world/pool.cpp |  | top-level object-pool init. Zeroes 520 68-byte slots at 0x800F2740; bu… |
| 0x8007B2C0 | LIVE | `Engine::seedDirectionMasks` | game/scene/startup.cpp |  | direction-mask seeder. Called with 0 at boot (initEntityPool above) an… |
| 0x8007B328 | LIVE | `Engine::initEntityPool` | game/scene/startup.cpp |  | engine SUBSYSTEM init (init-prefix slot, dispatched at native_boot.cpp… |
| 0x8007B3F4 | LIVE | `Engine::reloadEntityPool` | game/scene/startup.cpp |  | re-copy the staged per-area entity-pool control bytes onto the live he… |
| 0x8007D0D0 | LIVE | `DialogTextStream::applyRenderMode` | game/ui/dialog_text_stream.cpp |  | (obj a0) -- LEAF (guest 0x8007D0D0 has no `sp` descent). Cross-checked… |
| 0x8007DC38 | LIVE | `beh_variant_overlay_lifecycle` | game/ai/beh_variant_overlay_lifecycle.cpp | 0x8007C0D0 | NOT port_check-able as it stands: this is a hand-written REBUILD, not … |
| 0x8007E038 | LIVE | `Spawn::spawnOverlayVariantBody` | game/world/spawn.cpp |  | VARIANT-OVERLAY SPAWN primitive. RE'd from disas 0x8007E038..0x8007E10… |
| 0x8007E038 | LIVE | `Spawn::spawnOverlayVariant` | game/world/spawn.cpp |  |  |
| 0x8007E110 | LIVE | `Spawn::sceneEntityBody` | game/world/spawn.cpp |  | SCENE-ENTITY SPAWN primitive. RE'd from disas 0x8007E110..0x8007E1B4. |
| 0x8007E110 | LIVE | `Spawn::sceneEntity` | game/world/spawn.cpp |  |  |
| 0x8007E1B8 | LIVE | `UiSprite::quads` | game/ui/ui_sprite_quads.cpp |  | One POLY_FT4 per definition piece, mirrored by the attribute flags. |
| 0x8007E6DC | LIVE | `UiSprite::compose` | game/ui/ui_sprite_compose.cpp | 0x80083DE0 | (placement r4, indexPtr r5, defBase r6, attrs r7) |
| 0x8007E8DC | LIVE | `UiSprite::drawFromTable` | game/ui/ui_sprite.cpp | 0x8007E1B8 | (x r4, y r5, attr r6, defIndex r7) |
| 0x8007E998 | LIVE | `UiSprite::drawFixedDef152` | game/ui/ui_sprite.cpp | 0x8007E8DC | (x r4, y r5, attr r6) — drawFromTable with the definition index pinned… |
| 0x8007E9C8 | LIVE | `ScreenFade::draw` | game/render/screen_fade.cpp | 0x80083DE0 | The fade leaf: a fill across the draw window and a DR_MODE in the caller's OT slot. |
| 0x8007E9C8 | LIVE | `BgSceneTransitionSm::fadeRect` | game/scene/bg_scene_transition_sm.cpp | 0x8007E9C8 | Guest FUN_8007E9C8(color, P[3], 4). |
| 0x8007FC24 | LIVE | `OptionsPage::pushBackdrop` | game/ui/options_page.cpp |  | ORACLE: guest 0x8007FC24 |
| 0x8007FC24 | LIVE | `OptionsPage::install` | game/ui/options_page.cpp |  |  |
| 0x8007FCC8 | LIVE | `Panel::pushDialogBackdrop` | game/ui/dialog_backdrop.cpp |  | ORACLE: guest 0x8007FCC8 |
| 0x8007FCC8 | LIVE | `ov_push_dialog_backdrop` | game/ui/dialog_backdrop.cpp |  | Guest-ABI entry: x/y/w/h in r4-r7, mode off the caller's stack (see th… |
| 0x8007FDB0 | LIVE | `UnlitModelEmitter::gt3` | game/render/unlit_model_emitter.cpp |  | Resident unlit GT3 list emitter; install site. |
| 0x8008007C | LIVE | `UnlitModelEmitter::gt4` | game/render/unlit_model_emitter.cpp |  | Resident unlit GT4 list emitter; install site. |
| 0x80080F6C | LIVE | `Render::drawSync` | game/render/wide_re_libgpu_leaves.cpp |  | guest 0x80080F6C (0x80080F6C) — DrawSync(mode). VERIFIED & WIRED 2026-… |
| 0x80080F6C | LIVE | `ov_drawSync` | game/render/wide_re_libgpu_leaves.cpp |  |  |
| 0x80081218 | LIVE | `Asset::uploadImage` | game/core/assets/asset.cpp |  | DO NOT REGISTER 0x80081218 IN THE OVERRIDE REGISTRY. It surfaces near … |
| 0x80081458 | LIVE | `Render::clearOTagR` | game/render/wide_re_libgpu_leaves.cpp |  | guest 0x80081458 (0x80081458) — ClearOTagR(OT, entries). VERIFIED & WI… |
| 0x80081458 | LIVE | `ov_clearOTagR` | game/render/wide_re_libgpu_leaves.cpp |  |  |
| 0x80081560 | LIVE | `Engine::drawOTag` | game/game_tomba2.cpp |  | Native ownership of DrawOTag (libgpu FUN_80081560, the per-frame draw … |
| 0x800815D0 | LIVE | `nativePutDrawEnv` | game/render/wide_re_gpu_putdrawenv.cpp |  | nativePutDrawEnv (0x800815D0) — libgpu PutDrawEnv(drawEnvPtr). DRAFT. … |
| 0x80081CF8 | LIVE | `buildDrawAreaRect` | game/render/hud_gauge_emitter.cpp |  |  |
| 0x80081CF8 | LIVE | `emitDrawAreaAndLink` | game/render/hud_gauge_emitter.cpp |  | Emit the DR_AREA packet built from the sp+rectOff rect into the packet… |
| 0x80081FB0 | LIVE | `LibgpuDrawEnv::setDrawEnv` | game/render/libgpu_draw_env.cpp |  | GUEST_ADDRESS: 80081FB0 authenticated executable/overlay evidence |
| 0x80082220 | LIVE | `nativeDrawMode` | game/render/wide_re_gpu_putdrawenv.cpp |  | nativeDrawMode (0x80082220) — DR_TPAGE mode-word builder. DRAFT. RE'd … |
| 0x80082240 | LIVE | `nativeClipTopLeft` | game/render/wide_re_gpu_putdrawenv.cpp |  | nativeClipTopLeft (0x80082240) — SetDrawAreaTopLeft(x,y) word builder.… |
| 0x800822D8 | LIVE | `nativeClipBottomRight` | game/render/wide_re_gpu_putdrawenv.cpp |  | nativeClipBottomRight (0x800822D8) — SetDrawAreaBottomRight(x,y) word … |
| 0x80082370 | LIVE | `nativeDrawOffset` | game/render/wide_re_gpu_putdrawenv.cpp |  | nativeDrawOffset (0x80082370) — SetDrawingOffset(x,y) word builder. DR… |
| 0x8008238C | LIVE | `nativeTextureWindow` | game/render/wide_re_gpu_putdrawenv.cpp |  | nativeTextureWindow (0x8008238C) — DR_TWIN word builder. DRAFT. RE'd f… |
| 0x80082424 | LIVE | `Render::gpuDmaSend` | game/render/wide_re_gpu_dma_queue.cpp |  | guest 0x80082424 (0x80082424) — GpuDmaSend(arrayPtr, count). VERIFIED … |
| 0x80082424 | LIVE | `ov_gpuDmaSend` | game/render/wide_re_gpu_dma_queue.cpp |  |  |
| 0x80082734 | LIVE | `Render::gpuLoadImageStream` | game/render/wide_re_gpu_loadimage_streamer.cpp |  | guest 0x80082734 (0x80082734) — libgpu LoadImage()-internal chunked GP… |
| 0x80082734 | LIVE | `ov_gpuLoadImageStream` | game/render/wide_re_gpu_loadimage_streamer.cpp |  |  |
| 0x80082C68 | LIVE | `libgpuDmaStatusReset` | game/render/wide_re_libgpu_leaves.cpp |  | libgpuDmaStatusReset (0x80082C68) — GPU-DMA status-block RESET. RE-VER… |
| 0x80082D04 | LIVE | `Render::gpuDmaQueueEnqueue` | game/render/wide_re_gpu_dma_queue.cpp |  | guest 0x80082D04 (0x80082D04) — GpuDmaQueueEnqueue(fn, argValOrPtr, si… |
| 0x80082D04 | LIVE | `ov_gpuDmaQueueEnqueue` | game/render/wide_re_gpu_dma_queue.cpp |  |  |
| 0x80082FB4 | LIVE | `Render::gpuDmaQueueDrain` | game/render/wide_re_gpu_dma_queue.cpp |  | guest 0x80082FB4 (0x80082FB4) — GpuDmaQueueDrain(). VERIFIED & WIRED 2… |
| 0x80082FB4 | LIVE | `ov_gpuDmaQueueDrain` | game/render/wide_re_gpu_dma_queue.cpp |  |  |
| 0x80083364 | LIVE | `Render::gpuDmaQueueSync` | game/render/wide_re_gpu_dma_queue.cpp |  | guest 0x80083364 (0x80083364) — GpuDmaQueueSync(mode). VERIFIED & WIRE… |
| 0x80083364 | LIVE | `ov_gpuDmaQueueSync` | game/render/wide_re_gpu_dma_queue.cpp |  |  |
| 0x80083DE0 | LIVE | `libgpuSetDrawMode` | game/render/wide_re_libgpu_leaves.cpp |  | libgpuSetDrawMode (0x80083DE0) — libgpu **SetDrawMode(DR_MODE* p, int … |
| 0x80083E80 | LIVE | `Trig::rsin` | game/math/trig.cpp |  |  |
| 0x80083E80 | LIVE | `Trig::registerOverrides` | game/math/trig.cpp |  | UNREGISTERED (2026-07-15): rsin/ratan2 are NOT safe as overrides. Thei… |
| 0x80083F50 | LIVE | `Trig::rcos` | game/math/trig.cpp |  |  |
| 0x80084080 | LIVE | `Math::sqrtLzc` | game/math/gte_math.cpp |  |  |
| 0x80084110 | LIVE | `Math::matMul` | game/math/gte_math.cpp |  |  |
| 0x80084220 | LIVE | `Math::applyMatlv` | game/math/gte_math.cpp |  |  |
| 0x80084250 | LIVE | `GteTransform3::rotate3AndPackIr` | game/math/wide_re_gte_transform3.cpp |  |  |
| 0x80084360 | LIVE | `Math::matLoadLV` | game/math/gte_math.cpp |  |  |
| 0x80084470 | LIVE | `Math::applyMatrixLV` | game/math/gte_math.cpp |  |  |
| 0x800844C0 | LIVE | `Math::applyMatrixSV` | game/math/gte_math.cpp |  |  |
| 0x80084520 | LIVE | `Math::matColScale` | game/math/gte_math.cpp |  | ORACLE: guest 0x80084520 |
| 0x800847B0 | LIVE | `vertexHeaderRepack` | game/render/wide_re_libgpu_leaves.cpp |  | vertexHeaderRepack (0x800847B0) — 20-byte SoA->AoS vertex-header REPAC… |
| 0x800847F0 | LIVE | `Math::rotMatSoft` | game/math/gte_math.cpp |  |  |
| 0x80084A80 | LIVE | `Math::rotMatSoftInverse` | game/math/gte_math.cpp |  |  |
| 0x80084D10 | LIVE | `Math::rotX` | game/math/gte_math.cpp |  |  |
| 0x80084EB0 | LIVE | `Math::rotY` | game/math/gte_math.cpp |  |  |
| 0x80085050 | LIVE | `Math::rotZ` | game/math/gte_math.cpp |  |  |
| 0x800851F0 | LIVE | `Math::rotMatSoftYXZ` | game/math/gte_math.cpp |  |  |
| 0x80085480 | LIVE | `Math::rotmat` | game/math/gte_math.cpp |  |  |
| 0x80085690 | LIVE | `Trig::ratan2` | game/math/trig.cpp |  |  |
| 0x80085C9C | LIVE | `tomba::LibapiIntr::setIntrMask` | game/core/hle/libapi_intr.cpp |  |  |
| 0x80086230 | LIVE | `tomba::LibapiIntr::initVblankCallbacks` | game/core/hle/libapi_intr.cpp |  | FUN_0x80086230 — VBlank-callback subsystem init: clear the 8-slot VSyn… |
| 0x80086288 | LIVE | `tomba::LibapiIntr::runVblankCallbacks` | game/core/hle/libapi_intr.cpp |  | FUN_0x80086288 — the VBlank handler itself: bump the tick counter, the… |
| 0x80086320 | LIVE | `tomba::LibapiIntr::clearWords` | game/core/hle/libapi_intr.cpp |  | FUN_0x80086320 — the word-fill helper: writes N words of a constant. |
| 0x80086604 | LIVE | `Engine::activeModeCtx` | game/scene/startup.cpp |  | Engine::activeModeCtx. Accessor: returns the active mode/draw-env cont… |
| 0x80086604 | LIVE | `ov_engineActiveModeCtx` | game/core/engine/engine_frame_ticks.cpp |  | installed via tomba::native::declareOverride() at game/core/engine.cpp… |
| 0x80086620 | LIVE | `eng_init_mode_ctrl` | game/scene/startup.cpp |  | engine MODE control: file-local helper (only called from Engine::initS… |
| 0x80086738 | LIVE | `Engine::installModeHandlers` | game/scene/startup.cpp |  | Engine::installModeHandlers. Installs the mode handler table at 0x8010… |
| 0x80086738 | LIVE | `ov_engineInstallModeHandlers` | game/core/engine/engine_frame_ticks.cpp |  | installed via tomba::native::declareOverride() at game/core/engine.cpp… |
| 0x80086764 | LIVE | `Engine::runModeEnter` | game/scene/startup.cpp |  | Engine::runModeEnter. If both bit0 flags in the mode ctx (*0x800ABE98)… |
| 0x80086764 | LIVE | `ov_engineRunModeEnter` | game/core/engine/engine_frame_ticks.cpp |  | installed via tomba::native::declareOverride() at game/core/engine.cpp… |
| 0x80087A60 | LIVE | `Engine::initInput` | game/scene/startup.cpp | 0x80080890 0x800808A0 0x80085B10 0x800873F0 0x80087400 | a thin wrapper that just calls FUN_80086970; owned as initInput(). |
| 0x80088B00 | LIVE | `Engine::initAlloc` | game/scene/startup.cpp | 0x80086738 0x80089160 0x8009A340 | engine ALLOCATOR / dispatch-table init. `s1` / `s2` are the struct-spa… |
| 0x8008913C | LIVE | `Engine::allocRecordForSelector` | game/scene/startup.cpp |  | returns the base of record[0] or record[1] of the 240-byte-stride, 2-e… |
| 0x8008913C | LIVE | `ov_allocRecordForSelector` | game/scene/startup.cpp |  |  |
| 0x8008A110 | LIVE | `LibcdNative::posToInt` | game/cd/libcd_native.cpp |  |  |
| 0x8008B8F0 | LIVE | `LibcdNative::searchFile` | game/cd/libcd_native.cpp |  |  |
| 0x8008BBE8 | LIVE | `LibcdDirCache::newMedia` | game/cd/libcd_dir_cache.cpp |  |  |
| 0x8008BBE8 | LIVE | `LibcdNative::newMedia` | game/cd/libcd_native.cpp |  |  |
| 0x8008BF50 | LIVE | `LibcdDirCache::cacheFile` | game/cd/libcd_dir_cache.cpp |  |  |
| 0x8008BF50 | LIVE | `LibcdNative::cacheFile` | game/cd/libcd_native.cpp |  |  |
| 0x80090160 | LIVE | `Sequencer::channelStreamAccumulate` | game/audio/sequencer_tone_records.cpp |  | channelStreamAccumulate — true leaf (no stack frame). Faithful to gues… |
| 0x800909C0 | LIVE | `Sequencer::frameTick` | game/audio/sequencer.cpp |  | libsnd per-VBlank tick wrapper. WIDE-RE DRAFT, UNWIRED (see header). |
| 0x80090BD0 | LIVE | `Sequencer::seqChannelDispatch` | game/audio/sequencer.cpp | 0x80090E40 0x80091050 0x80091910 0x80092080 | SsSeqCalled — the per-VBlank sequence/channel scheduler. Faithful to |
| 0x80090E40 | LIVE | `Sequencer::channelPitchSlideTick` | game/audio/sequencer_pitch_envelope.cpp |  | channelPitchSlideTick — pitch-slide/portamento per-tick interpolator (… |
| 0x80091050 | LIVE | `Sequencer::channelReleaseClear` | game/audio/sequencer_channel_flags.cpp |  | "release"/note-off housekeeping: zeroes the per-channel status byte at… |
| 0x800910F0 | LIVE | `Sequencer::channelPitchSelectDispatch` | game/audio/sequencer.cpp |  | thin arg-repacking wrapper: sign-extend (seq,chan) to 32-bit and tail-… |
| 0x80091810 | LIVE | `Sequencer::channelVoiceKeyOn` | game/audio/sequencer_voice_write.cpp |  | channelVoiceKeyOn — true leaf (no stack frame). Faithful to guest 0x80… |
| 0x80091910 | LIVE | `Sequencer::channelStopFlagSet` | game/audio/sequencer_channel_flags.cpp |  | sets the per-channel status byte at +20 to 1, clears flags bit3 (value… |
| 0x80091970 | LIVE | `Sequencer::channelNoteInit` | game/audio/sequencer.cpp | 0x800931A0 | channelNoteInit — per-channel note retrigger (SsSeqCalled flags bit2 r… |
| 0x80092080 | LIVE | `Sequencer::channelEnvelopeRampTick` | game/audio/sequencer_pitch_envelope.cpp |  | channelEnvelopeRampTick — ADSR/envelope ramp (SsSeqCalled flags bit6 A… |
| 0x80092310 | LIVE | `Sequencer::channelToneRecordCopy` | game/audio/sequencer_tone_records.cpp |  | channelToneRecordCopy — stack frame present (sp-32, spill r16@16/r17@2… |
| 0x80092420 | LIVE | `Sequencer::channelToneRecordCopyWide` | game/audio/sequencer_tone_records.cpp |  | channelToneRecordCopyWide — stack frame present (sp-32, spill r16@16/r… |
| 0x800931C0 | LIVE | `Sequencer::voiceStateFlush` | game/audio/sequencer_voice_state.cpp | 0x80097E10 0x80098DB0 0x80098F90 0x80099970 0x8009A1D0 | the sound driver's per-frame SPU voice-state flush. 12,000 substrate d… |
| 0x80094150 | LIVE | `Sequencer::voiceAllocateOrSteal` | game/audio/sequencer_voice_alloc.cpp |  | voiceAllocateOrSteal — true leaf (no stack frame). Faithful to guest 0… |
| 0x80094474 | LIVE | `Sequencer::channelNotePeriodCompute` | game/audio/sequencer_tone_records.cpp |  | channelNotePeriodCompute — true leaf (no stack frame). Faithful to gue… |
| 0x80094B50 | LIVE | `Sequencer::channelKeyRegisterMerge` | game/audio/sequencer_channel_flags.cpp |  | channelKeyRegisterMerge — true leaf (no stack frame). Faithful to gues… |
| 0x80095530 | LIVE | `Sequencer::channelVoiceRegisterWrite` | game/audio/sequencer_voice_write.cpp |  | channelVoiceRegisterWrite — the "SPU voice-register write leaf" channe… |
| 0x80095A9C | LIVE | `Sequencer::channelVolumeSnapshot` | game/audio/sequencer_channel_flags.cpp |  | channelVolumeSnapshot — true leaf (no stack frame). Faithful to guest … |
| 0x80095B90 | LIVE | `Sequencer::channelKeyEventScan` | game/audio/sequencer_channel_flags.cpp |  | channelKeyEventScan — stack frame present (sp-32, spill ra/s16/s17/s18… |
| 0x800962B0 | LIVE | `Sequencer::channelVoiceSelectPrep` | game/audio/sequencer_voice_write.cpp |  | channelVoiceSelectPrep — called mid-loop by channelVoiceRegisterWrite(… |
| 0x80096370 | LIVE | `Font::bank2Store` | game/ui/font.cpp |  | font-bank2 store. `*kFontBank2Addr(sb) = bank; jr ra`. Leaf; does NOT … |
| 0x800963A0 | LIVE | `Font::bankSelect` | game/ui/font.cpp |  | font-bank selector. If ((bank-1)&0xff) < 24, store the bank byte at |
| 0x800998E4 | LIVE | `AreaSlots::classifySlotStates` | game/world/area_slots.cpp |  | ORACLE: guest 0x800998E4 |
| 0x8009A3E0 | LIVE | `Str::copyBytes` | game/core/assets/str.cpp |  | memcpy(dst, src, n). RE from authenticated executable/overlay evidence… |
| 0x8009A3E0 | LIVE | `ov_copyBytes` | game/core/assets/str.cpp |  |  |
| 0x8009A640 | LIVE | `Str::compareBytes` | game/core/assets/str.cpp |  | FUN_0x8009A640 — byte compare, sibling of the memcpy already owned her… |
| 0x800A33C8 | LIVE | `tbl_strp` | game/ai/beh_cube_text_spawn.cpp |  | string-table entry pointer: mem32(0x800a33c8 + (node[0x60]*3 << 2) + 4… |
| 0x800BE224 | LIVE | `MusicCoord::musicFadeIn` | game/audio/music_coord.cpp |  | PC-added helper (NOT a port of any FUN_XXXX): snap the game's CD-volum… |
| 0x800BED80 | LIVE | `MusicCoord::dialogToneActive` | game/audio/music_coord.cpp |  |  |
| 0x800BF842 | LIVE | `Engine::postRenderTick` | game/core/engine/engine_frame_ticks.cpp |  | Engine::postRenderTick — 3-state fx-trigger + countdown on byte 0x800B… |
| 0x801062E4 | LIVE | `Demo::stageMain` | game/scene/demo.cpp | 0x800810F0 | DEMO stage entry (0x801062E4) — own the prologue PC-native, then hand … |
| 0x8010637C | LIVE | `Engine::stagePrologue` | game/core/engine/engine_state_dispatch.cpp |  | GAME stage TOP-LEVEL ENTRY 0x8010637C — task-0's stage driver: a one-t… |
| 0x801063C0 | LIVE | `Demo::s0` | game/scene/demo.cpp |  | s0 0x801063C0 — run-once INIT then loaders; FALLS THROUGH into s1 same… |
| 0x801063F4 | LIVE | `Engine::frame` | game/core/engine/engine_state_dispatch.cpp |  | One native loop iteration of the guest body 0x801063F4: dispatch sm[0x… |
| 0x801063F4 | ORPHAN | `Engine::stageMain` | game/core/engine/engine_state_dispatch.cpp |  | OLD guest-loop entry (prologue + guest-continuation into the guest loo… |
| 0x8010641C | LIVE | `Demo::s1` | game/scene/demo.cpp | 0x80106F80 | s1 0x8010641C — wait/advance: v0 = inner menu input machine 0x80106f80… |
| 0x80106464 | LIVE | `Demo::s2` | game/scene/demo.cpp | 0x8001CF2C 0x8010696C | s2 0x80106464 — sub-machine v0 = 0x8010696c(). Outcome 1 -> go to s7 (… |
| 0x80106478 | LIVE | `Engine::areaLoadState` | game/core/engine/engine_task_machine.cpp | 0x8001CF2C 0x8004D8B0 0x80078824 0x8007E8DC 0x8007ED5C 0x8007EE74 … | Engine::areaLoadState — native ownership of FUN_80106478 (the |
| 0x8010649C | LIVE | `StartBinStage::runNative` | game/scene/start_bin_stage.cpp |  | START.BIN stage 0: file tables from the host ISO9660, boot preloads inline |
| 0x801064E8 | LIVE | `Demo::s3` | game/scene/demo.cpp | 0x800750D8 0x80106AC4 | s3 0x801064E8 — sub-machine v0 = 0x80106ac4() (mirror of 0x8010696c). … |
| 0x80106580 | LIVE | `demo_frame_s4` | game/scene/demo.cpp |  | Substate s4 (0x80106580) — LOAD GAME. The body runs the load sub-machi… |
| 0x801065DC | LIVE | `demo_frame_s5` | game/scene/demo.cpp |  | Substate s5 (0x801065DC) — LEAVE DEMO: the body is `jal 0x80052078(2)`… |
| 0x801065EC | LIVE | `Demo::s6` | game/scene/demo.cpp | 0x8007B45C 0x80106690 0x80106824 | s6 0x801065EC — page sub-machine 0x8007b45c(); if sm[0x50]==3 fire the… |
| 0x8010696C | LIVE | `Demo::s2SubMachine` | game/scene/demo.cpp | 0x80106690 0x80106824 |  |
| 0x80106AC4 | LIVE | `Demo::s3SubMachine` | game/scene/demo.cpp | 0x80106690 0x80106824 |  |
| 0x80106AC4 | LIVE | `Demo::registerOverrides` | game/scene/demo.cpp |  |  |
| 0x80106F80 | LIVE | `demo_menu_machine` | game/scene/demo.cpp | 0x8001CF00 0x8008CCE0 0x8008CD40 0x8009C820 0x8009C8BC 0x80106F80 | s1's inner menu input machine (0x80106F80): an 8-way state machine on … |
| 0x80107AFC | LIVE | `FieldTransition::main` | game/scene/field_transition.cpp |  | the MAIN door/sub-scene transition (sm[0x4c]==1..4). sm[0x4e]: |
| 0x80107D3C | LIVE | `FieldTransition::d3c` | game/scene/field_transition.cpp |  | transition variant (sm[0x4c]==5/6). sm[0x4e]: 0 load, 1 effect |
| 0x80107E20 | LIVE | `FieldTransition::e20` | game/scene/field_transition.cpp |  | transition variant (sm[0x4c]==7). sm[0x4e]: 0 setup+load, 1 |
| 0x80107F3C | LIVE | `FieldTransition::f3c` | game/scene/field_transition.cpp |  | transition variant (sm[0x4c]==8), a 7-state machine. NB case 0 |
| 0x8010810C | LIVE | `Engine::submitPage810c` | game/core/engine/engine_task_machine.cpp | 0x801084F8 | page-1 dim-fade branch (task+0x6B == 1, "draw main pause menu" — |
| 0x801086E0 | LIVE | `Engine::stageAreaInit` | game/core/engine/engine_task_machine.cpp |  | sm[0x48] == 0 — area INIT: advance to running (sm[0x48]=2), reset the |
| 0x80108720 | LIVE | `Engine::stageResumeInit` | game/core/engine/engine_task_machine.cpp |  | sm[0x48] == 1 — area RESUME-INIT (re-enter a running area, sub-mode 1)… |
| 0x8010882C | LIVE | `Engine::stageRunning` | game/core/engine/engine_task_machine.cpp |  | sm[0x48]==2 RUNNING, per-frame variant: dispatch sm[0x4a] handler. han… |
| 0x8010882C | LIVE | `Engine::submode0` | game/core/engine/engine_task_machine.cpp | 0x80109450 | GAME sub-mode-0 bridge 0x8010882c (sm[0x4c]/sm[0x4e] dispatch) — nativ… |
| 0x80108A60 | LIVE | `FieldTransition::step` | game/scene/field_transition.cpp |  | sm[0x4a]==5 transition dispatcher on sm[0x4c]. 0/9 = done |
| 0x80108B0C | LIVE | `Engine::devTeleportApply` | game/core/engine/engine_scene_frame.cpp |  | FIELD PER-FRAME UPDATE 0x80108b0c — native control flow (the field fra… |
| 0x80109164 | LIVE | `Sop::areaLoad` | game/scene/sop.cpp | 0x8001DC40 | Owned synchronous area-DATA load (replaces the body of LAB_80109164 |
| 0x801092B4 | LIVE | `Sop::fieldUpdate` | game/scene/sop.cpp |  | SOP per-frame FIELD UPDATE — native ownership of FUN_801092b4 (decomp |
| 0x80109450 | LIVE | `Sop::fieldMode` | game/scene/sop.cpp |  | SOP FIELD-MODE MACHINE — native ownership of FUN_80109450 (decomp |
| 0x8010957C | LIVE | `AreaFadeSequencer::step` | game/scene/area_fade_sequencer.cpp | 0x8007E9C8 0x8010CC68 0x8010D030 | A0L per-node fade sequencer run by `Engine::fieldRun` while sm[0x4e]==0xb. |
| 0x801099B4 | LIVE | `UnlitModelEmitter::gt3` (SOP) | game/render/unlit_model_emitter.cpp |  | SOP unlit GT3; A0B and A0C copy its body. |
| 0x80109C80 | LIVE | `UnlitModelEmitter::gt4` (SOP) | game/render/unlit_model_emitter.cpp |  | SOP unlit GT4. |
| 0x80109FE0 | LIVE | `SopGround::draw` | game/render/sop_ground.cpp | 0x801099B4 0x80109C80 | SOP ground blocks; producer, object = block |
| 0x8010A0E0 | LIVE | `Sop::scenePrepass` | game/scene/sop.cpp |  | SOP scene cam-frustum prepass — native ownership of FUN_8010A0E0 (Ghid… |
| 0x8010A3AC | LIVE | `Sop::sceneGridGather` | game/scene/sop.cpp |  | sceneGridGather — native port of guest FUN_8010A3AC (Ghidra decomp |
| 0x8010A3AC | LIVE | `UnlitModelEmitter::gt3` (A0J) | game/render/unlit_model_emitter.cpp |  | SOP GT3 with a near clamp (kSopNearLifted). |
| 0x8010A69C | LIVE | `UnlitModelEmitter::gt4` (A0J) | game/render/unlit_model_emitter.cpp |  | SOP GT4 with a near clamp (kSopNearLifted). |
| 0x8010AA4C | LIVE | `OverlayGroundGt3Gt4::gt3` (A0L) | game/render/overlay_ground_gt3gt4.cpp |  | Ground GT3 copy (kA0L). |
| 0x8010AB20 | LIVE | `UnlitModelEmitter::gt3` (A0J) | game/render/unlit_model_emitter.cpp |  | Flagged unlit GT3 (kFlagged). |
| 0x8010AB38 | LIVE | `beh_sop_overlay_shadow` | game/ai/sop_overlay_shadow.cpp |  |  |
| 0x8010ACFC | LIVE | `beh_sop_intro_pilot` | game/ai/beh_sop_intro_pilot.cpp |  |  |
| 0x8010AD40 | LIVE | `OverlayGroundGt3Gt4::gt4` (A0L) | game/render/overlay_ground_gt3gt4.cpp |  | Ground GT4 copy (kA0L). |
| 0x8010ADC4 | LIVE | `UnlitModelEmitter::gt4` (A0J) | game/render/unlit_model_emitter.cpp |  | Flagged unlit GT4 (kFlagged). |
| 0x8010AE30 | LIVE | `native_sop_overlay_shadow_spawn` | game/ai/sop_overlay_shadow.cpp |  | (parent) -> node ptr (0 on pool exhaustion). |
| 0x8010AF58 | LIVE | `UnlitModelEmitter::gt3` (A0H) | game/render/unlit_model_emitter.cpp |  | SOP GT3 with a near clamp (kSopNearWide). |
| 0x8010AF60 | LIVE | `sopBeatAdvanceWalk` | game/ai/sop_intro_events.cpp |  |  |
| 0x8010B078 | LIVE | `sopBeatAdvanceNarration` | game/ai/sop_intro_events.cpp |  |  |
| 0x8010B11C | LIVE | `sopOrbitPathStep` | game/ai/sop_intro_events.cpp | 0x80077C40 |  |
| 0x8010B240 | LIVE | `UnlitModelEmitter::gt4` (A0H) | game/render/unlit_model_emitter.cpp |  | SOP GT4 with a near clamp (kSopNearWide). |
| 0x8010B2D4 | LIVE | `sopIntroEffectTick` | game/ai/sop_intro_events.cpp | 0x800519E0 0x8007778C 0x80077C40 |  |
| 0x8010B3DC | LIVE | `UnlitModelEmitter::gt3` (A0I) | game/render/unlit_model_emitter.cpp |  | SOP GT3 with a near clamp (kSopNear). |
| 0x8010B44C | LIVE | `sopIntroEffectSpawn` | game/ai/sop_intro_events.cpp |  |  |
| 0x8010B588 | LIVE | `sopLiftedSubtick` | game/ai/sop_intro_events.cpp |  | GUEST FRAME (2026-07-10 §9 fix): overlay guest 0x8010B588 pushes `addi… |
| 0x8010B6BC | LIVE | `UnlitModelEmitter::gt3` (A0H) | game/render/unlit_model_emitter.cpp |  | Flagged unlit GT3 (kFlagged). |
| 0x8010B6C4 | LIVE | `UnlitModelEmitter::gt4` (A0I) | game/render/unlit_model_emitter.cpp |  | SOP GT4 with a near clamp (kSopNear). |
| 0x8010B798 | LIVE | `beh_sop_intro_lifted` | game/ai/beh_sop_intro_lifted.cpp |  |  |
| 0x8010B960 | LIVE | `UnlitModelEmitter::gt4` (A0H) | game/render/unlit_model_emitter.cpp |  | Flagged unlit GT4 (kFlagged). |
| 0x8010B990 | LIVE | `beh_sop_intro_narration` | game/ai/beh_sop_intro_narration.cpp |  |  |
| 0x8010BB40 | LIVE | `UnlitModelEmitter::gt3` (A0I) | game/render/unlit_model_emitter.cpp |  | Flagged unlit GT3 (kFlagged). |
| 0x8010BC40 | LIVE | `UnlitModelEmitter::gt3` (A0G) | game/render/unlit_model_emitter.cpp |  | SOP GT3 with a near clamp (kSopNear). |
| 0x8010BDE4 | LIVE | `UnlitModelEmitter::gt4` (A0I) | game/render/unlit_model_emitter.cpp |  | Flagged unlit GT4 (kFlagged). |
| 0x8010BEAC | LIVE | `beh_orbit_spark_effect` | game/ai/sop_intro_events.cpp |  |  |
| 0x8010BF28 | LIVE | `UnlitModelEmitter::gt4` (A0G) | game/render/unlit_model_emitter.cpp |  | SOP GT4 with a near clamp (kSopNear). |
| 0x8010C26C | LIVE | `TileGridLayer::emitSop` | game/render/tile_grid_layer.cpp | 0x80083DE0 0x80081218 | SOP tile grid + CLUT palette cycles; producer, object = map cell |
| 0x8010E258 | LIVE | `ActorObjectContact::resolveHitOrProximity` | game/ai/actor_object_contact.cpp |  | ORACLE: overlay guest 0x8010E258 |
| 0x8010E904 | LIVE | `ActorTomba::postFrameWaterCheck` | game/player/actor_tomba.cpp |  |  |
| 0x8010EA80 | LIVE | `ActorBump::respondToContact` | game/ai/actor_bump.cpp |  | ORACLE: overlay guest 0x8010EA80 |
| 0x801103F4 | LIVE | `UnlitModelEmitter::gt3` (A0A) | game/render/unlit_model_emitter.cpp |  | Flagged unlit GT3 (kFlagged). |
| 0x80110698 | LIVE | `UnlitModelEmitter::gt4` (A0A) | game/render/unlit_model_emitter.cpp |  | Flagged unlit GT4 (kFlagged). |
| 0x80112188 | LIVE | `ActorMeleeEngage::doIt` | game/ai/actor_melee_engage.cpp | 0x80022C78 0x80055844 0x80084080 |  |
| 0x80112188 | LIVE | `ActorMeleeEngage::registerOverrides` | game/ai/actor_melee_engage.cpp |  |  |
| 0x80112A24 | LIVE | `UnlitModelEmitter::gt3` (A0B) | game/render/unlit_model_emitter.cpp |  | SOP's GT3 body. |
| 0x80112A60 | LIVE | `aux_list_walk` | game/ai/area_seaside_perframe.cpp |  | Walk the aux render list, dispatching FUN_80112A60(item) per item type… |
| 0x80112CF0 | LIVE | `UnlitModelEmitter::gt4` (A0B) | game/render/unlit_model_emitter.cpp |  | SOP's GT4 body. |
| 0x80112DEC | LIVE | `OverlayGt3Gt4::gt3` (A0L) | game/render/overlay_gt3gt4.cpp |  | A00's GT3 body. |
| 0x80112FBC | LIVE | `OverlayGt3Gt4::gt4` (A0L) | game/render/overlay_gt3gt4.cpp |  | A00's GT4 body. |
| 0x801130C4 | LIVE | `ActorTomba::postInteractWalk` | game/player/actor_tomba.cpp |  |  |
| 0x80113150 | LIVE | `UnlitModelEmitter::gt3` (A0B) | game/render/unlit_model_emitter.cpp |  | Flagged unlit GT3 (kFlagged). |
| 0x801133F4 | LIVE | `UnlitModelEmitter::gt4` (A0B) | game/render/unlit_model_emitter.cpp |  | Flagged unlit GT4 (kFlagged). |
| 0x80113748 | LIVE | `UnlitModelEmitter::gt3` (A0D) | game/render/unlit_model_emitter.cpp |  | Flagged unlit GT3 (kFlagged). |
| 0x80113788 | LIVE | `UnlitModelEmitter::gt3` (A0C) | game/render/unlit_model_emitter.cpp |  | SOP's GT3 body. |
| 0x801139EC | LIVE | `UnlitModelEmitter::gt4` (A0D) | game/render/unlit_model_emitter.cpp |  | Flagged unlit GT4 (kFlagged). |
| 0x80113A54 | LIVE | `UnlitModelEmitter::gt4` (A0C) | game/render/unlit_model_emitter.cpp |  | SOP's GT4 body. |
| 0x80113C5C | LIVE | `Behaviors::areaSeasidePerframe` | game/ai/area_seaside_perframe.cpp | 0x8002288C |  |
| 0x80113EB4 | LIVE | `UnlitModelEmitter::gt3` (A0C) | game/render/unlit_model_emitter.cpp |  | Flagged unlit GT3 (kFlagged). |
| 0x80114158 | LIVE | `UnlitModelEmitter::gt4` (A0C) | game/render/unlit_model_emitter.cpp |  | Flagged unlit GT4 (kFlagged). |
| 0x801141B0 | LIVE | `TileGridLayer::emitUnbiased` (A0B) | game/render/tile_grid_layer.cpp |  | Tile grid with no V bias; producer, object = map cell. |
| 0x801142EC | LIVE | `TileGridLayer::emitUnbiased` (A0A) | game/render/tile_grid_layer.cpp |  | Tile grid with no V bias; producer, object = map cell. |
| 0x80114458 | LIVE | `UnlitModelEmitter::gt3` (A0E) | game/render/unlit_model_emitter.cpp |  | Flagged unlit GT3 (kFlagged). |
| 0x801146FC | LIVE | `UnlitModelEmitter::gt4` (A0E) | game/render/unlit_model_emitter.cpp |  | Flagged unlit GT4 (kFlagged). |
| 0x80114E74 | LIVE | `ActorTomba::type4GuardedCheck` | game/player/actor_tomba.cpp |  | type-4 guarded proximity. |
| 0x8011534C | LIVE | `TileGridLayer::scrollStep` | game/render/tile_grid_layer.cpp |  |  |
| 0x80115598 | LIVE | `TileGridLayer::emit` | game/render/tile_grid_layer.cpp | 0x80083DE0 |  |
| 0x801157CC | LIVE | `UnlitModelEmitter::gt3` (A0F) | game/render/unlit_model_emitter.cpp |  | Flagged unlit GT3 (kFlagged). |
| 0x80115A70 | LIVE | `UnlitModelEmitter::gt4` (A0F) | game/render/unlit_model_emitter.cpp |  | Flagged unlit GT4 (kFlagged). |
| 0x80116778 | LIVE | `TileGridLayer::emitUnbiased` (A0F) | game/render/tile_grid_layer.cpp |  | Tile grid with no V bias; producer, object = map cell. |
| 0x80116904 | LIVE | `RainStreaks::draw` | game/render/rain_streaks.cpp | 0x80083DE0 0x80084220 0x80084660 0x80084690 | A08 rain streaks; producer, element = drop |
| 0x80116B9C | LIVE | `TileGridLayer::emitUnbiased` (A0D) | game/render/tile_grid_layer.cpp |  | Tile grid with no V bias; producer, object = map cell. |
| 0x80117658 | LIVE | `beh_prng_velocity_machine` | game/ai/beh_prng_velocity_machine.cpp |  |  |
| 0x801178A4 | LIVE | `whiteFlashPhaseRamp` | game/ai/beh_a06_multi_actor.cpp |  | the 5-phase white-flash phase ramp SM. See the file header for the pha… |
| 0x80117AAC | LIVE | `whiteFadeHold` | game/ai/beh_a06_multi_actor.cpp |  | 3-state fade-hold-fade-back companion to whiteFlashPhaseRamp. |
| 0x80118240 | LIVE | `beh_typed_init_exit_poker` | game/ai/beh_typed_init_exit_poker.cpp |  |  |
| 0x80118690 | LIVE | `shared_8690` | game/ai/beh_typed_init_exit_poker.cpp |  | Shared block @0x80118690: FUN_80051D90(node[0x10], a1_buf, 0x1F8000C0)… |
| 0x801189E8 | LIVE | `beh_a06_multi_actor` | game/ai/beh_a06_multi_actor.cpp |  | The guest body 0x801189E8 is ONE function that descends sp by 32 and s… |
| 0x80118B10 | LIVE | `AssemblyRider::rideSlotAndReactToStroke` | game/ai/assembly_rider.cpp |  | ORACLE: overlay guest 0x80118B10 |
| 0x8011C164 | LIVE | `beh_typed_variant_router` | game/ai/beh_typed_variant_router.cpp |  |  |
| 0x8011CBD0 | LIVE | `beh_node3_router` | game/ai/beh_node3_router.cpp |  |  |
| 0x8011D578 | LIVE | `beh_variant_actor_sm` | game/ai/beh_variant_actor_sm.cpp |  |  |
| 0x8011D988 | LIVE | `beh_actor_move_sm` | game/ai/beh_actor_move_sm.cpp |  |  |
| 0x80121978 | LIVE | `beh_id_routed_dispatch` | game/ai/beh_id_routed_dispatch.cpp |  |  |
| 0x80122BF4 | LIVE | `beh_id_routed_offset_point` | game/ai/beh_id_routed_dispatch.cpp | 0x800844C0 | FUN_0x80122BF4 — keeps a point pinned 119 world units ABOVE a linked o… |
| 0x80123E9C | LIVE | `ReleaseTriggerMotion::hoverBobCycle` | game/ai/release_trigger_motion.cpp | 0x80077B5C |  |
| 0x801241BC | LIVE | `ReleaseTriggerMotion::leaderFollowSync` | game/ai/release_trigger_motion.cpp | 0x80051D90 0x80123C94 0x8012400C |  |
| 0x80124328 | LIVE | `ReleaseTriggerMotion::xSweepCycle` | game/ai/release_trigger_motion.cpp |  | a per-frame X-sweep cycle on the release-trigger object. 6,355 substra… |
| 0x801244E8 | LIVE | `ReleaseTriggerMotion::driftReposition` | game/ai/release_trigger_motion.cpp | 0x80051794 0x80077B5C 0x80084360 0x800847F0 0x80124328 |  |
| 0x801246A4 | LIVE | `OverlayGroundGt3Gt4::gt3` (A02) | game/render/overlay_ground_gt3gt4.cpp |  | Ground GT3 copy (kFarthest). |
| 0x801246B4 | LIVE | `ReleaseTriggerMotion::arcSwoopMotion` | game/ai/release_trigger_motion.cpp | 0x80077B5C |  |
| 0x8012496C | LIVE | `OverlayGroundGt3Gt4::gt4` (A02) | game/render/overlay_ground_gt3gt4.cpp |  | Ground GT4 copy (kFarthest). |
| 0x801249D4 | LIVE | `ReleaseTriggerMotion::doubleArcMotion` | game/ai/release_trigger_motion.cpp | 0x80077B5C |  |
| 0x80124C6C | LIVE | `ReleaseTriggerMotion::circleOrbitMotion` | game/ai/release_trigger_motion.cpp | 0x80077B5C |  |
| 0x80124E74 | LIVE | `beh_jumptable_release_trigger` | game/ai/beh_jumptable_release_trigger.cpp | 0x8004B0D8 0x8004DAEC 0x80051D90 0x80077B5C 0x80123E9C 0x801241BC … |  |
| 0x80125E0C | LIVE | `beh_pure_substate_dispatch` | game/ai/beh_pure_substate_dispatch.cpp |  |  |
| 0x80125FE0 | LIVE | `TiltFollower::applyHalvedOwnerPartPitch` | game/ai/tilt_follower.cpp |  | ORACLE: overlay guest 0x80125FE0 |
| 0x80127420 | LIVE | `beh_arm_countdown_if_linked_ready_80127420` | game/ai/beh_toy_spawn_family.cpp |  | (obj) — arm a 20-frame countdown if the linked object (obj[+0x10]'s ta… |
| 0x801274BC | LIVE | `beh_distance_band_predicate_801274bc` | game/ai/beh_toy_spawn_family.cpp |  | (obj) — a distance-band predicate. `row` is looked up from a per-slot … |
| 0x80127510 | LIVE | `beh_spawn_toy_child_type2_80127510` | game/ai/beh_toy_spawn_family.cpp |  | (owner, subtype) — spawn a child whose sub-behavior is picked by `subt… |
| 0x8012763C | LIVE | `beh_spawn_toy_child_type4_8012763c` | game/ai/beh_toy_spawn_family.cpp | 0x8004D650 | (owner) — spawn a type-4 companion child, then feed GBASE's mode byte … |
| 0x80127720 | LIVE | `beh_spawn_toy_child_type5_80127720` | game/ai/beh_toy_spawn_family.cpp |  | (owner) — spawn a type-5 companion child via the legacy allocator, no … |
| 0x80127798 | LIVE | `beh_area_transition_machine` | game/ai/beh_area_transition_machine.cpp | 0x80041194 |  |
| 0x80127C58 | LIVE | `cutsceneDirector` | game/ai/beh_a08_scene_actor.cpp | 0x80081218 0x8013DD48 | FUN_80127C58 — the 10-state cutscene director |
| 0x80127C9C | LIVE | `dat_tail` | game/ai/beh_area_transition_machine.cpp |  |  |
| 0x80127CD0 | LIVE | `cd0_tail` | game/ai/beh_area_transition_machine.cpp |  |  |
| 0x801280D0 | LIVE | `beh_a08_scene_actor` | game/ai/beh_a08_scene_actor.cpp |  |  |
| 0x801281B8 | LIVE | `RopeSwing::swingTickAndBendSegments` | game/ai/rope_swing.cpp |  | ORACLE: overlay guest 0x801281B8 |
| 0x80128760 | LIVE | `beh_linked_advance_branch` | game/ai/beh_linked_advance_branch.cpp |  |  |
| 0x80129BAC | LIVE | `LitModelEmitter::gt3` (A08) | game/render/lit_model_emitter.cpp |  | Lit GT3 list emitter; the A05 and A07 copies share the native. |
| 0x80129C00 | LIVE | `beh_anim_trigger_gates` | game/ai/beh_anim_trigger_gates.cpp |  |  |
| 0x8012A06C | LIVE | `LitModelEmitter::gt4` (A08) | game/render/lit_model_emitter.cpp |  | Lit GT4 list emitter. |
| 0x8012A0B8 | LIVE | `beh_box_seed_phase_gate` | game/ai/beh_box_seed_phase_gate.cpp |  |  |
| 0x8012C7E0 | LIVE | `OverlayGroundGt3Gt4::gt3` (A07) | game/render/overlay_ground_gt3gt4.cpp |  | Ground GT3 copy (kFarthest). |
| 0x8012CAA8 | LIVE | `OverlayGroundGt3Gt4::gt4` (A07) | game/render/overlay_ground_gt3gt4.cpp |  | Ground GT4 copy (kFarthest). |
| 0x8012CDF4 | LIVE | `LitModelEmitter::gt3` (A07) | game/render/lit_model_emitter.cpp |  | A08's lit GT3 body. |
| 0x8012D27C | LIVE | `SwaySchedule::advanceRateThenSway` | game/ai/sway_schedule.cpp |  | ORACLE: overlay guest 0x8012D27C |
| 0x8012D2B4 | LIVE | `LitModelEmitter::gt4` (A07) | game/render/lit_model_emitter.cpp |  | A08's lit GT4 body. |
| 0x8012D404 | LIVE | `beh_cull_tick_render` | game/ai/beh_cull_tick_render.cpp |  |  |
| 0x8012D4EC | LIVE | `beh_jumptable_flag_gate` | game/ai/beh_jumptable_flag_gate.cpp |  |  |
| 0x8012D6AC | LIVE | `step_node18` | game/ai/beh_jumptable_flag_gate.cpp |  | LAB_8012d7d8 (and its inline copy at 0x8012d6ac): node[0x18,0x19,0x1a]… |
| 0x8012D78C | LIVE | `advance_node32` | game/ai/beh_jumptable_flag_gate.cpp |  | LAB_8012d78c: node[0x32]+=4; if (int16)node[0x32] < -0x64e -> tail_set… |
| 0x8012D7D8 | LIVE | `step_node18` | game/ai/beh_jumptable_flag_gate.cpp |  | LAB_8012d7d8 (and its inline copy at 0x8012d6ac): node[0x18,0x19,0x1a]… |
| 0x8012D7FC | LIVE | `despawn_flag_block` | game/ai/beh_jumptable_flag_gate.cpp |  | LAB_8012d82c..8012d840: set bit (4 if node[3]==0 else 8) in DAT_800bf9… |
| 0x8012D844 | LIVE | `tail_set1_and_render` | game/ai/beh_jumptable_flag_gate.cpp |  | LAB_8012d844: v0=1; fall into 8012d848 (node[1]=1); then 8012d84c (jal… |
| 0x8012D848 | LIVE | `tail_set1_and_render` | game/ai/beh_jumptable_flag_gate.cpp |  | LAB_8012d844: v0=1; fall into 8012d848 (node[1]=1); then 8012d84c (jal… |
| 0x8012D84C | LIVE | `tail_set1_and_render` | game/ai/beh_jumptable_flag_gate.cpp |  | LAB_8012d844: v0=1; fall into 8012d848 (node[1]=1); then 8012d84c (jal… |
| 0x8012DA04 | LIVE | `beh_typed_anim_spawn` | game/ai/beh_typed_anim_spawn.cpp |  |  |
| 0x8012EB54 | LIVE | `beh_substate_edge_orchestrator` | game/ai/beh_substate_edge_orchestrator.cpp | 0x8012E8A8 0x8012ED84 0x8012F494 0x8012F5B4 0x8012FD88 0x80130524 … |  |
| 0x8012F8D8 | LIVE | `SwayModelEmitter::cueGt3` | game/render/sway_model_emitter.cpp | 0x80083E80 0x80083F50 | A01 depth-cue GT3; sways through rsin/rcos. |
| 0x8013000C | LIVE | `SwayModelEmitter::cueGt4` | game/render/sway_model_emitter.cpp |  | A01 depth-cue GT4. |
| 0x80130838 | LIVE | `SwayModelEmitter::swayGt3` | game/render/sway_model_emitter.cpp | 0x80083E80 0x80083F50 | A01 sway GT3. |
| 0x80130D9C | LIVE | `SwayModelEmitter::scrollGt4` | game/render/sway_model_emitter.cpp |  | A01 UV-scroll GT4. |
| 0x801311D0 | LIVE | `OverlayGt3Gt4::gt3` (A07) | game/render/overlay_gt3gt4.cpp |  | A00's GT3 body. |
| 0x801313A0 | LIVE | `OverlayGt3Gt4::gt4` (A07) | game/render/overlay_gt3gt4.cpp |  | A00's GT4 body. |
| 0x801316A8 | LIVE | `LitModelEmitter::gt3` (A01) | game/render/lit_model_emitter.cpp |  | A08's lit GT3 body plus the hide and deepen flag bits. |
| 0x801316CC | LIVE | `SubstateEdgeLeaves::tickChildOscillators` | game/ai/substate_edge_native.cpp | 0x80130D5C |  |
| 0x80131BB0 | LIVE | `LitModelEmitter::gt4` (A01) | game/render/lit_model_emitter.cpp |  | A08's lit GT4 body plus the hide and deepen flag bits. |
| 0x80131D08 | LIVE | `beh_two_child_steer` | game/ai/beh_two_child_steer.cpp |  |  |
| 0x80132400 | LIVE | `beh_single_child_cull` | game/ai/beh_single_child_cull.cpp |  |  |
| 0x8013259C | LIVE | `beh_cull_substate_orchestrator` | game/ai/beh_cull_substate_orchestrator.cpp | 0x8013272C 0x80132954 0x80132A88 0x80132D58 0x80132EDC 0x80133184 … |  |
| 0x80132690 | LIVE | `UnlitModelEmitter::gt3` (A01) | game/render/unlit_model_emitter.cpp |  | A01 third-branch unlit GT3 (kA01Cue). |
| 0x801329C4 | LIVE | `UnlitModelEmitter::gt4` (A01) | game/render/unlit_model_emitter.cpp |  | A01 third-branch unlit GT4 (kA01Cue). |
| 0x80133C14 | LIVE | `beh_typed_table_seed_gate` | game/ai/beh_typed_table_seed_gate.cpp |  |  |
| 0x80133D6C | LIVE | `beh_twin_record_steer` | game/ai/beh_twin_record_steer.cpp |  |  |
| 0x80134FD8 | LIVE | `beh_multi_record_phase_machine` | game/ai/beh_multi_record_phase_machine.cpp |  |  |
| 0x801353C8 | LIVE | `common_tail` | game/ai/beh_multi_record_phase_machine.cpp |  | COMMON TAIL (0x801353C8): node[8]++ / FUN_800517F8(node) / node[8]--. |
| 0x8013544C | LIVE | `LitModelEmitter::gt3` (A05) | game/render/lit_model_emitter.cpp |  | A08's lit GT3 body. |
| 0x8013590C | LIVE | `LitModelEmitter::gt4` (A05) | game/render/lit_model_emitter.cpp |  | A08's lit GT4 body. |
| 0x80135D64 | LIVE | `beh_quad_record_table_seed` | game/ai/beh_quad_record_table_seed.cpp |  |  |
| 0x801360F4 | LIVE | `Spawn::spawnTypedChild` | game/world/spawn.cpp |  | TYPED-CHILD SPAWN wrappers (A00 overlay, |
| 0x801360F4 | LIVE | `Spawn::spawnQuadRecordChild` | game/world/spawn.cpp |  |  |
| 0x80136158 | LIVE | `beh_sine_motion_sfx` | game/ai/beh_sine_motion_sfx.cpp | 0x8004766C 0x80048750 |  |
| 0x80136954 | LIVE | `beh_event_record_machine` | game/ai/beh_event_record_machine.cpp |  |  |
| 0x80136D9C | LIVE | `beh_pure_inner_dispatch` | game/ai/beh_pure_inner_dispatch.cpp |  |  |
| 0x801389C8 | LIVE | `AssemblyCompanion::composeRigAndApplyPartScales` | game/ai/assembly_companion.cpp |  | AssemblyCompanion::composeRigAndApplyPartScales, guest FUN_801389C8 — … |
| 0x80138A64 | LIVE | `AssemblyCompanion::endCamHoldAndRearmOnStroke` | game/ai/assembly_companion.cpp |  | ORACLE: overlay guest 0x80138A64 |
| 0x80138FC8 | LIVE | `beh_typed_jumptable_pair` | game/ai/beh_typed_jumptable_pair.cpp | 0x8004ED94 0x80138B04 0x80138C70 |  |
| 0x801395C0 | LIVE | `beh_sibling_angle_track` | game/ai/beh_sibling_angle_track.cpp |  |  |
| 0x80139728 | LIVE | `beh_a06_fade_flash_ramp_80139728` | game/ai/beh_a06_script_fades.cpp |  |  |
| 0x80139838 | LIVE | `Spawn::spawnTypedChild` | game/world/spawn.cpp |  | TYPED-CHILD SPAWN wrappers (A00 overlay, |
| 0x80139838 | LIVE | `Spawn::spawnSiblingAngleChild` | game/world/spawn.cpp |  |  |
| 0x80139A28 | LIVE | `variant4Phase3` | game/ai/beh_a06_scripted_actor.cpp |  | FUN_80139A28 — variant-4 case-3 sub-machine (inner script cycle) |
| 0x80139C84 | LIVE | `sub801398E4` | game/ai/beh_a06_scripted_actor.cpp |  | FUN_80139C84 — variant-4 outer sub-machine (5 states) |
| 0x8013A330 | LIVE | `beh_lift_platform` | game/ai/beh_lift_platform.cpp |  |  |
| 0x8013A730 | LIVE | `Spawn::spawnTypedChild` | game/world/spawn.cpp |  | TYPED-CHILD SPAWN wrappers (A00 overlay, |
| 0x8013A730 | LIVE | `Spawn::spawnLiftPlatformChild` | game/world/spawn.cpp |  |  |
| 0x8013A900 | LIVE | `beh_child_trig_motion` | game/ai/beh_child_trig_motion.cpp |  |  |
| 0x8013AA14 | LIVE | `beh_a06_scripted_actor` | game/ai/beh_a06_scripted_actor.cpp |  |  |
| 0x8013AC34 | LIVE | `Spawn::spawnTypedChild` | game/world/spawn.cpp |  | TYPED-CHILD SPAWN wrappers (A00 overlay, |
| 0x8013AC34 | LIVE | `Spawn::spawnChildTrigChild` | game/world/spawn.cpp |  |  |
| 0x8013ADBC | LIVE | `beh_box_rearm_sub` | game/ai/beh_box_rearm_sub.cpp |  |  |
| 0x8013AEF0 | LIVE | `beh_a06_spawn_follow_obj_8013AEF0` | game/ai/beh_a06_script_fades.cpp |  | FUN_8013AEF0 — spawn a follow-obj and hook it |
| 0x8013AFD8 | LIVE | `beh_a06_sound_cmd_wait_8013AFD8` | game/ai/beh_a06_script_fades.cpp | 0x800708B4 | FUN_8013AFD8 — kick a sound-command sequence and wait for scratchpa… |
| 0x8013B074 | LIVE | `beh_a06_spawn_subobj_8013B074` | game/ai/beh_a06_script_fades.cpp | 0x8006CBA8 | FUN_8013B074 — spawn a subobj + set field/anim params |
| 0x8013B178 | LIVE | `beh_a06_fade_ramp_8013B178` | game/ai/beh_a06_script_fades.cpp |  |  |
| 0x8013B274 | LIVE | `beh_a06_music_cue_8013B274` | game/ai/beh_a06_script_fades.cpp |  |  |
| 0x8013B29C | LIVE | `beh_a06_timer_gate_8013B29C` | game/ai/beh_a06_script_fades.cpp |  | FUN_8013B29C — 2-state (init + counted gate) primitive |
| 0x8013B2E4 | LIVE | `beh_flagbit_timer_machine` | game/ai/beh_flagbit_timer_machine.cpp |  |  |
| 0x8013B70C | LIVE | `drawInit` | game/ai/beh_seaside_prox_substate.cpp | 0x8013B534 |  |
| 0x8013B868 | LIVE | `subA` | game/ai/beh_seaside_prox_substate.cpp | 0x8006CBA8 0x8006E1C0 0x8006E1E4 |  |
| 0x8013BA44 | LIVE | `UnlitModelEmitter::gt3` (A06) | game/render/unlit_model_emitter.cpp |  | SOP GT3 with lit depth code and hide bit 2 (kA06). |
| 0x8013BAB0 | LIVE | `subB` | game/ai/beh_seaside_prox_substate.cpp | 0x8004766C 0x80048750 |  |
| 0x8013BCC8 | LIVE | `subC` | game/ai/beh_seaside_prox_substate.cpp | 0x80027144 0x8003116C 0x8006E1C0 0x8006E1E4 0x8009A450 |  |
| 0x8013BD40 | LIVE | `UnlitModelEmitter::gt4` (A06) | game/render/unlit_model_emitter.cpp |  | SOP GT4 with lit depth code and hide bit 2 (kA06). |
| 0x8013C0BC | LIVE | `modeArm` | game/ai/beh_seaside_prox_substate.cpp | 0x80081218 |  |
| 0x8013C0D8 | LIVE | `LitModelEmitter::gt3` (A06) | game/render/lit_model_emitter.cpp |  | A08's lit GT3 body plus hide bit 2. |
| 0x8013C1DC | LIVE | `beh_seaside_prox_substate` | game/ai/beh_seaside_prox_substate.cpp |  | 's own prologue is `addiu sp,sp,-0x20` (disas-verified). modeArm/subC'… |
| 0x8013C3F4 | LIVE | `beh_area_threshold_ptr_swap` | game/ai/beh_area_threshold_ptr_swap.cpp |  |  |
| 0x8013C538 | LIVE | `beh_scatter_record_dither` | game/ai/beh_scatter_record_dither.cpp |  |  |
| 0x8013C5B4 | LIVE | `LitModelEmitter::gt4` (A06) | game/render/lit_model_emitter.cpp |  | A08's lit GT4 body plus hide bit 2. |
| 0x8013C9C0 | LIVE | `beh_scatter_ramp_machine` | game/ai/beh_scatter_ramp_machine.cpp |  |  |
| 0x8013CDD4 | LIVE | `WidescreenMarginQuad::emit` | game/render/widescreen_margin_quad.cpp |  |  |
| 0x8013CF00 | LIVE | `UnlitModelEmitter::gt3` (A06) | game/render/unlit_model_emitter.cpp |  | A06 drawer unlit GT3 with U scroll (kA06Scroll). |
| 0x8013D1E4 | LIVE | `UnlitModelEmitter::gt4` (A06) | game/render/unlit_model_emitter.cpp |  | A06 drawer unlit GT4 with U scroll (kA06Scroll). |
| 0x8013FB88 | LIVE | `OverlayGroundGt3Gt4::gt3` | game/render/overlay_ground_gt3gt4.cpp |  | ground/scene POLY_GT3 emit. Record = 36 bytes, SAME field layout as th… |
| 0x8013FE58 | LIVE | `OverlayGroundGt3Gt4::gt4` | game/render/overlay_ground_gt3gt4.cpp |  | ground/scene POLY_GT4 emit. Record = 44 bytes: {+0 rgb0(rgb1=rgb0<<4)\|… |
| 0x801401B8 | LIVE | `OverlayGroundGt3Gt4::entityLoop` | game/render/overlay_ground_gt3gt4.cpp |  | the ground-entity render list walker. list=a0: +6 (u8) entry count, +1… |
| 0x8014047C | LIVE | `ActorZonedAttacker::gateCheck` | game/ai/actor_zoned_attacker.cpp |  | ActorZonedAttacker::gateCheck(c) — FUN_8014047c(node) -> bool v0. A ti… |
| 0x80140544 | LIVE | `ActorZonedAttacker::typeInit` | game/ai/actor_zoned_attacker.cpp |  | ActorZonedAttacker::typeInit(c) — FUN_80140544(node). One-shot per-typ… |
| 0x801409C0 | LIVE | `ActorZonedAttacker::pickAttackByRange` | game/ai/actor_zoned_attacker.cpp |  | ActorZonedAttacker::pickAttackByRange(c) — FUN_801409c0(node[, unused … |
| 0x80140FBC | LIVE | `OverlayGt3Gt4::gt3` (A08) | game/render/overlay_gt3gt4.cpp |  | A00's GT3 body plus the UV scroll at 0x80145A6E. |
| 0x801411D8 | LIVE | `OverlayGt3Gt4::gt4` (A08) | game/render/overlay_gt3gt4.cpp |  | A00's GT4 body plus the UV scroll. |
| 0x80143A00 | LIVE | `ActorZonedAttacker::defaultSubStateMachine` | game/ai/actor_zoned_attacker.cpp |  | ActorZonedAttacker::defaultSubStateMachine(c) — FUN_80143a00(node). Th… |
| 0x80144928 | LIVE | `ActorZonedAttacker::approachAndFace` | game/ai/actor_zoned_attacker.cpp |  | ActorZonedAttacker::approachAndFace(c) — FUN_80144928(node) -> v0. A s… |
| 0x80144B50 | LIVE | `ActorZonedAttacker::idleTick` | game/ai/actor_zoned_attacker.cpp |  | ActorZonedAttacker::idleTick(c) — FUN_80144b50(node). The "idle" state… |
| 0x80145230 | LIVE | `beh_id_compare_motion_dispatch` | game/ai/beh_id_compare_motion_dispatch.cpp | 0x800781E0 0x8014047C 0x80140544 0x801409C0 0x80143A00 0x80144928 … |  |
| 0x801458E0 | LIVE | `AttackOrbitSubstate::orbitTargetMotion` | game/ai/attack_orbit_substate.cpp |  | node[3]==0x81 sub-behavior: 6-phase acquire/orbit machine, see header … |
| 0x80145AF0 | LIVE | `AttackOrbitSubstate::aimAtTargetAnchor` | game/ai/attack_orbit_substate.cpp |  | node[3]==0x80 sub-behavior: aim-point recompute + one-shot attack-wind… |
| 0x80145C78 | LIVE | `ActorZonedAttacker::zoneClassify` | game/ai/actor_zoned_attacker.cpp |  | classifies (u8 at record+0x2A, s16 at record+0x36) into a {0,1,2} zone… |
| 0x80146478 | LIVE | `OverlayGt3Gt4::submitBlock` | game/render/overlay_gt3gt4.cpp | 0x801465EC 0x801467BC |  |
| 0x801465EC | LIVE | `OverlayGt3Gt4::gt3` | game/render/overlay_gt3gt4.cpp |  | POLY_GT3 (gouraud-textured triangle) emit, GTE-driven, guest-writing. |
| 0x801467BC | LIVE | `OverlayGt3Gt4::gt4` | game/render/overlay_gt3gt4.cpp |  | POLY_GT4 (gouraud-textured quad) emit, GTE-driven, guest-writing. |
| 0x80182000 | LIVE | `preload_build_vram` | game/core/assets/asset.cpp | 0x80075448 | cel/sprite VRAM build, synchronous. FUN_800753ac is itself an async CD… |

## Stock library bindings (`TombaRuntime::registerOverrides`)

psxport stock classes and `PlatformHle::bindStock` own these; the guest body never runs.

| addr | owner |
|------|-------|
| 0x80085900 | `StockVSync` (guest VSync is fatal) |
| 0x8008AC34 | `StockLibcd::command` (CD_cw) |
| 0x8008A6EC | `StockLibcd::sync` (CdSync) |
| 0x80080F6C | `StockLibgpu` DrawSync |
| 0x800834A0 | `StockLibgpu` GPU timeout arm |
| 0x800834D4 | `StockLibgpu` GPU timeout check |
| 0x800846D0 | `StockLibgte` SetGeomOffset |
| 0x800846F0 | `StockLibgte` SetGeomScreen |
| 0x8008A96C | `cdReady` (CdReady) |
| 0x8008B2D8 | `PlatformHle::completeImmediately` (CdInit handshake) |
| 0x8008B4B8 | `PlatformHle::completeImmediately` (CdDataSync) |
| 0x8009CAEC | `PlatformHle::completeImmediately` (DecDCTinSync) |
| 0x8009CB80 | `PlatformHle::completeImmediately` (DecDCToutSync) |
| 0x80080880 | `TombaScheduler::yield` (ChangeThread; a native override) |
