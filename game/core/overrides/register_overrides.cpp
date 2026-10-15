// Collect Tomba! 2's subsystem-owned native declarations for Runtime.
//
// Subsystem registerOverrides() calls declare handlers in native_override_catalog. Repeating an
// identical declaration is idempotent; conflicting owners are refused rather than overwritten.
// Runtime separately binds resident declarations to each Core's explicit resident image token
// after loading, including the final boot reload. PSXPort's per-Core dispatcher then selects a native
// handler or executes the guest body through Lightrec. Declaring an overlay handler does not activate it.
#include "overrides/register_overrides.h"
#include "actor_bump.h"           // class ActorBump — bump response (0x8010EA80)
#include "actor_object_contact.h" // class ActorObjectContact — hit / proximity contact (0x8010E258)
#include "actor_targeting.h"      // class ActorTargeting — acquire a target (0x8001FAE0)
#include "assembly_companion.h"   // class AssemblyCompanion — assembly companion idle tick (0x80138A64)
#include "core.h"
#include "engine/engine.h"
#include "entry/session.h"
#include "game.h"
#include "mtx.h" // class Mtx — libgte matrix leaves (MR_init identity, 0x80051794)
#include "overrides/native_override_catalog.h"
#include "placed_prop_sm.h"       // class PlacedPropSm — placed scene-prop behaviour (0x80040558)
#include "rope_swing.h"           // class RopeSwing — hanging rope swing + segment bend (0x801281B8)
#include "sway_schedule.h"        // class SwaySchedule — rocking-rate schedule + sway tick (0x8012D27C)
#include "tilt_follower.h"        // class TiltFollower — half-tilt follower leaf (0x80125FE0)
                                  //   + its per-visible-frame rig pose (0x801389C8)
#include "actor_melee_engage.h"   // class ActorMeleeEngage — A00-overlay melee-engage/reposition/arm leaf
#include "actor_sm_reward.h"      // class ActorReward — reward/tally window actor SM family
#include "actor_tomba.h"          // class ActorTomba — Tomba's postInteractWalk sub-handler leaves
#include "actor_zoned_attacker.h" // class ActorZonedAttacker — 0x8014xxxx zoned-attacker sub-behavior cluster
#include "assembly_rider.h"       // class AssemblyRider — pump arm-end rider per-frame tick (0x80118B10)
#include "collision_resolve.h"    // class CollisionResolve — actor-vs-object cylinder resolve (0x80023D48)
#include "cube_text_ledger.h"     // class CubeTextLedger — cube-text popup ledger (deactivate/spawn)
#include "cutscene_camera.h" // class CutsceneCamera — resetFollowAccum/pushMode/restoreMode/snapToMasterOffsetY200/orbitTick
#include "demo.h"            // class Demo — DEMO main-menu title cursor sub-machine (registerOverrides)
#include "fx_sprite_anchored.h"    // class FxSpriteAnchored — single-anchor sprite emitter (0x80027CB4)
#include "fx_sprite_swarm.h"       // class FxSpriteSwarm — per-particle sprite emitter (0x800281EC)
#include "graphics_bind.h"         // class GraphicsBind — object render-bind subsystem (recordArrayInit)
#include "gte_transform3.h"        // class GteTransform3 — GTE 3-vertex rotate+pack (0x80084250)
#include "hle/libapi_intr.h"       // class LibapiIntr — kernel interrupt-mask primitives (0x80085C9C)
#include "hud_gauge_emitter.h"     // class HudGaugeEmitter — self-contained HUD gauge emitter (0x8004FD30/0x8004FB4C)
#include "letterbox_bars.h"        // class LetterboxBars — cutscene letterbox (0x80026864)
#include "libgpu_draw_env.h"       // class LibgpuDrawEnv — libgpu SetDrawEnv (0x80081FB0)
#include "lit_model_emitter.h"     // class LitModelEmitter — A01/A05/A07/A08 point-lit GT3/GT4 emitters
#include "math/trig.h"             // class Trig — rsin/ratan2/angleCmp override wiring
#include "melee_proximity.h"       // class MeleeProximity — melee-proximity/approach-anchor leaf
#include "node_xform.h"            // class NodeXform — per-object child-transform-propagate family
#include "obj_model_view.h"        // class ObjModelView — shared effect-mesh model-view compose leaf (0x800318A0)
#include "overlay_ground_gt3gt4.h" // class OverlayGroundGt3Gt4 — A00-overlay GROUND/SCENE GT3/GT4 cluster
#include "overlay_gt3gt4.h"        // class OverlayGt3Gt4 — A00 and A08 model GT3/GT4 packet emitters
#include "quad_rtpt_submit.h"
#include "rain_streaks.h" // class RainStreaks — A08 rain streaks (0x80116904)       // class QuadRtptSubmit — 0x8003xxxx rope/flame quad rotate+RTPT submit
#include "render/drop_shadow.h"
#include "render/packed_mesh.h"
#include "scene/scene_events.h" // class SceneEvents — scene-event ARM (FUN_80040B48 sole owner)
#include "screen_fade.h"        // class ScreenFade — full-screen fade leaf (0x8007E9C8)
#include "sop_ground.h"         // class SopGround — SOP intro ground blocks (0x80109FE0)
#include "sop_intro_events.h"   // RegisterSopIntroEventOverrides — SOP intro-cutscene sub-tick/sub-motion/timer cluster
#include "substate_edge_native.h" // class SubstateEdgeLeaves — authored child-oscillator loop (0x801316CC)
#include "sway_model_emitter.h"   // class SwayModelEmitter — A01 sway, scroll and cue emitters
#include "tile_grid_layer.h" // class TileGridLayer — A00-overlay field scroll-wrap + tile-grid sprite emitter (0x8011534C/0x80115598)
#include "ui/ui_sprite.h"        // class UiSprite — 2D sprite emitters (0x8007E1B8/0x8007E6DC)
#include "unlit_model_emitter.h" // class UnlitModelEmitter — resident and SOP/A0B/A0C GT3/GT4 emitters
#include "widescreen_margin_quad.h" // class WidescreenMarginQuad — A00-overlay widescreen-margin OT.GT4 emitter (0x8013CDD4)

// Free-function beh_* wide-RE clusters (verified+wired) — same "class-ifying is a separate axis"
// acceptance behavior_dispatch.cpp's own table already uses for this family.
namespace tomba {

void RegisterBehToySpawnFamilyOverrides(
    native::NativeOverrideCatalog
        &natives); // game/ai/beh_toy_spawn_family.cpp (0x80127420/801274BC/80127720/8012763C/80127510)
void RegisterEngineAnimLeafOverrides(
    native::NativeOverrideCatalog &natives); // game/core/engine.cpp (0x8004190C animTick / 0x80054D14 walkStart)
void RegisterBehActorTombaProximityCombatOverride(
    native::NativeOverrideCatalog &natives); // game/ai/beh_actor_tomba_proximity_combat.cpp (0x800527C8)

void interact_scan_install(
    native::NativeOverrideCatalog &natives); // game/player/interact_scan.cpp — guest FUN_80024794
void dialog_backdrop_install(
    native::NativeOverrideCatalog &natives); // game/ui/dialog_backdrop.cpp — guest FUN_8007FCC8 (box backdrop)
void compose_tint_gate_install(
    native::NativeOverrideCatalog &natives); // game/render/compose_tint_gate.cpp — guest FUN_8003EF9C
void subpart_walk_install(native::NativeOverrideCatalog &natives); // game/render/subpart_walk.cpp — guest FUN_8003F174
void shared_transform_walk_install(
    native::NativeOverrideCatalog &natives); // game/render/subpart_walk_shared.cpp — guest FUN_8003F07C
void startup_overrides_install(native::NativeOverrideCatalog &natives); // game/scene/startup.cpp
void id_routed_leaves_install(native::NativeOverrideCatalog &natives);  // game/ai/beh_id_routed_dispatch.cpp

void register_engine_overrides(Game &game, native::NativeOverrideCatalog &natives) {
  interact_scan_install(natives);         // interaction scanner: promotes an in-range object to ACTIVATED
  dialog_backdrop_install(natives);       // message-box backdrop rect — LIVE on the dialog path
  UiSprite::registerOverrides(natives);   // 2D sprite emitters and their entry points
  compose_tint_gate_install(natives);     // per-type render gate (render frontier)
  subpart_walk_install(natives);          // sub-part walker (render frontier — last of the per-type list)
  shared_transform_walk_install(natives); // its rigid-node sibling
  Core *c = &game.core;
  // The legacy C++-fiber scheduler bindings were intentionally removed. Yield-capable guest work
  // must resume from stored CPU context through typed executor exits; it cannot retain a generated
  // original body or suspend a native C++ stack.
  mathOf(c).registerOverrides(natives);         // GTE matMul/applyMatlv/applyMatrixLV/rotmat/rotX/Y/Z (0x80084110 etc.)
  eng(c).animation.registerOverrides(natives);  // loadFrame/advanceLinkChain/attach/applyFrame (0x80076904 etc.)
  eng(c).areaSlots.registerOverrides(natives);  // primeCountdown/updateCell (0x80074A38/0x8007496C)
  eng(c).musicCoord.registerOverrides(natives); // setGain2 (0x80075D24)
  ActorReward::registerOverrides(natives);      // reward/tally window actor SM family
  tomba::ai::ActorZonedAttacker::registerOverrides(natives); // 0x8014xxxx zoned-attacker sub-behavior cluster
  eng(c).spawn.registerTypedChildOverrides(natives);         // A00-overlay typed-child spawners
  eng(c).releaseTriggerMotion.registerOverrides(natives);    // release-trigger sub-motion cluster
  OverlayGt3Gt4::registerOverrides(natives);                 // A00 and A08 plain GT3/GT4 packet emitters
  OverlayGroundGt3Gt4::registerOverrides(
      natives); // A00-overlay GROUND/SCENE GT3/GT4 + entity loop (0x8013FB88/8013FE58/801401B8)
  TileGridLayer::registerOverrides(
      natives); // A00-overlay field scroll-wrap + tile-grid sprite emitter (0x8011534C/0x80115598)
  WidescreenMarginQuad::registerOverrides(natives); // A00-overlay widescreen-margin OT.GT4 quad emitter (0x8013CDD4)
  ObjModelView::registerOverrides(natives);         // shared effect-mesh model-view compose into GTE CR0-7 (0x800318A0)
  render::PackedMesh::registerOverrides(natives);   // packed-mesh writer (0x80027768)
  render::DropShadow::registerOverrides(natives);   // actor drop shadow (0x8002AB5C)
  HudGaugeEmitter::registerOverrides(natives);      // self-contained HUD gauge emitter (0x8004FD30/0x8004FB4C)
  QuadRtptSubmit::registerOverrides(natives);       // rope/flame quad rotate+RTPT submit (0x8003B054/8003B320)
  tomba::render::LitModelEmitter::registerOverrides(natives);   // A01/A05/A07/A08 point-lit GT3/GT4 emitters
  tomba::render::UnlitModelEmitter::registerOverrides(natives); // resident and SOP/A0B/A0C GT3/GT4 emitters
  tomba::render::SwayModelEmitter::registerOverrides(natives);  // A01 sway, scroll and cue GT3/GT4 emitters
  SopGround::registerOverrides(natives);                        // SOP intro ground blocks (0x80109FE0)
  RainStreaks::registerOverrides(natives);                      // A08 rain streaks (0x80116904)
  tomba::render::LetterboxBars::registerOverrides(natives);     // cutscene letterbox bars (0x80026864)
  ScreenFade::registerOverrides(natives);                       // full-screen fade (0x8007E9C8)
  NodeXform::registerOverrides(
      natives); // seedBlock/propagateRotmat/propagateAxis/buildAxis/copyMatrixBlock/buildFromChild/worldPosFromLocal/worldPosFromComposed
                // (0x800517BC/80051300/80051464/80051C8C/80051B34/80051614/80051D90/80051D20)
  GraphicsBind::registerOverrides(natives); // recordArrayInit (0x800519E0)
  eng(c).cull.registerOverrides(
      natives); // cullWrapper family (0x8007778C/800777FC/80077ACC/800779D0/80077A4C/800778E4)
  eng(c).collision.registerOverrides(
      natives); // field-collision leaf cluster (0x80045810/80048034/80048134/80048360/80049760)
  eng(c).bgSceneTransitionSm.registerOverrides(natives); // BG scene-transition opcode leaves (0x80042758/80042884)
  eng(c).audioDispatch.registerOverrides(natives);       // field-audio BGM start/override leaves (0x80075024/80075070)
  eng(c).sfx.registerOverrides(natives);                 // SFX trigger wrapper (0x80074810)
  SceneEvents::registerOverrides(natives); // scene-event ARM FUN_80040B48 (sole owner; deduped from cube_text_ledger)
  CubeTextLedger::registerOverrides(natives); // cube-text popup ledger deactivate/spawn (0x80040C00/80040AA4)
  tomba::player::ActorTomba::registerOverrides(
      natives);                                 // postInteractWalk sub-handlers (0x80020364/800205CC/800235A0/80022C78)
  ActorMeleeEngage::registerOverrides(natives); // A00-overlay melee-engage/reposition/arm leaf (0x80112188)
  MeleeProximity::registerOverrides(natives);   // melee-proximity/approach-anchor leaf (0x8001F9DC)
  tomba::camera::CutsceneCamera::registerOverrides(
      natives); // resetFollowAccum/pushMode/restoreMode/snapToMasterOffsetY200/orbitTick
                // (0x8006E8F8/8006E1C0/8006E1E4/8006EA00/8006EF38)
  RegisterBehToySpawnFamilyOverrides(
      natives);                             // toy/child spawner leaves (0x80127420/801274BC/80127720/8012763C/80127510)
  RegisterEngineAnimLeafOverrides(natives); // Engine::animTick/walkStart fallthrough native-ize (0x8004190C/80054D14)
  Trig::registerOverrides(natives); // Trig::rsin/ratan2/angleCmp fallthrough native-ize (0x80083E80/80085690/80077768)
  Mtx::registerOverrides(
      natives); // MR_init identity-matrix leaf (0x80051794) — native existed since long before it was wired
  startup_overrides_install(natives);             // Engine::allocRecordForSelector (0x8008913C)
  id_routed_leaves_install(natives);              // id-routed state-1 common tail (0x80122BF4)
  PlacedPropSm::registerOverrides(natives);       // placed scene-prop behaviour handler (0x80040558)
  ActorTargeting::registerOverrides(natives);     // acquire a target: reach, band, arc (0x8001FAE0)
  ActorBump::registerOverrides(natives);          // actor bump: interact / push apart / recoil (0x8010EA80)
  ActorObjectContact::registerOverrides(natives); // actor-vs-object hit / proximity contact (0x8010E258)
  RopeSwing::registerOverrides(natives);          // hanging rope: spring swing + per-segment bend (0x801281B8)
  TiltFollower::registerOverrides(natives);       // pitch at half the owner sub-part's tilt (0x80125FE0)
  SwaySchedule::registerOverrides(natives);       // rocking rate winds down over the area-0 event
                                                  // sequence, then the per-type sway (0x8012D27C)
  AssemblyCompanion::registerOverrides(natives);  // rig pose (0x801389C8, A00-scoped) of the companion a
                                                  // field assembly spawns; idle tick (0x80138A64) stays
                                                  // resident — its owning image is undecided (issue 0015)
  AssemblyRider::registerOverrides(natives);      // per-frame tick of the rider perched on a seaside
                                                  // pump's arm-end (0x80118B10, area 0)
  SubstateEdgeLeaves::registerOverrides(natives); // authored A00 child-oscillator loop (0x801316CC)
  CollisionResolve::registerOverrides(natives);   // actor-vs-object cylinder collision resolve (0x80023D48)
  tomba::LibapiIntr::registerOverrides(
      natives); // libapi SetIntrMask (0x80085C9C) — I_MASK swap through libapi's hw-pointer table
  LibgpuDrawEnv::registerOverrides(natives); // libgpu SetDrawEnv (0x80081FB0) — DRAWENV -> DR_ENV packet compiler,
                                             // 2x/frame from PutDrawEnv/DrawOTagEnv
  GteTransform3::registerOverrides(
      natives); // GTE 3-vertex rotate+pack (0x80084250) — wide-RE draft, re-verified before wiring
  FxSpriteSwarm::registerOverrides(
      natives); // FUN_800281EC — sprite-cluster stamped once per particle (torch/roof-flame embers)
  FxSpriteAnchored::registerOverrides(natives); // FUN_80027CB4 — sprite-cluster stamped once at the node's own anchor,
                                                // uniform depth scale (hut-roof flames)
  RegisterBehActorTombaProximityCombatOverride(natives); // enemy-vs-Tomba proximity-combat FSM (0x800527C8)
  eng(c).sequencer.registerOverrides(natives);           // libsnd SsSeqCalled cluster (0x80090BD0 etc.)
  eng(c).script.registerOverrides(natives);              // cutscene-script opcodes 05/06/34/36/31
                                                         // (0x80042090/800420AC/80042E10/80043108/80041468)
  RegisterSopIntroEventOverrides(natives);               // SOP intro-cutscene sub-tick/sub-motion/timer cluster
                                           // (0x8010AF60/8010B078/8010B11C/8010B2D4/8010B44C/8010BEAC/8010B588).
                                           // 0x8010B11C/8010B2D4/8010B588 are SOP-scoped by the IMAGE
                                           // (docs/issues/0015); the other four stay in the resident form
                                           // because more than one authenticated image holds a DIFFERENT
                                           // body at each of those addresses.
  Demo::registerOverrides(natives); // main-menu title cursor sub-machine (0x80106AC4) — the r16/r17
  // register-liveness gap that blocked this wire (docs/findings/ai.md "Demo::s3SubMachine r16
  // register-liveness SBS divergence") is FIXED (2026-07-10): s3SubMachine's own port was missing the
  // `r17 = 0x1F800000` scratch-register prep overlay guest 0x80106AC4:333 does right before calling
  // 0x80106824 — that instruction's only purpose is a post-call re-read of *0x1F800138, but 0x80106824
  // spills the INCOMING r17 to its own guest stack (sp+36) before restoring it, so the value must
  // match for byte-exact SBS. Root cause was never r16 (that was already correct) — see the finding.
}

} // namespace tomba
