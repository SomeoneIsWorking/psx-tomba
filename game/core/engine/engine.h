// class Engine — the PC-native GAME/STAGE driver of Tomba! 2.
//
// PROPER OOP: one instance per Core (embedded as `Core::engine`). Callers use
// it as `eng(c).method()` — non-static instance methods, back-pointer to Core
// stored once at Core construction time (same pattern as `Core::screenFade`).
// No `extern "C"` shims.
//
// SCOPE: the GAME-stage state machine (sm[0x48] handlers) and its per-frame
// field driver — the top of the field spine that was previously exposed as the
// free functions `ov_game_stage_prologue` / `ov_game_frame` / `ov_field_frame`
// / `ov_field_frame_x` in engine.cpp. Those free functions are the
// implementation; this class is the public interface (called by the scheduler
// and by the state-machine handlers themselves once fully migrated).
//
// Migration is progressive: existing static helpers keep working while methods
// are added here. An Engine method may be a thin forwarder to a static ov_* in
// engine.cpp until that body is class-ified in place and the forwarder collapses.
#pragma once
#include "ai/actor_melee_engage.h"     // Engine owns the ActorMeleeEngage AI leaf (FUN_80112188)
#include "ai/attack_orbit_substate.h"  // Engine owns the A00-overlay AttackOrbitSubstate sub-behaviors
#include "ai/melee_proximity.h"        // Engine owns the MeleeProximity AI leaf (FUN_8001F9DC)
#include "ai/release_trigger_motion.h" // Engine owns the release-trigger sub-motion cluster
#include "area_fade_sequencer.h"
#include "array8_dispatch.h"      // Engine owns the Array8Dispatch fixed-array dispatcher
#include "audio/audio_dispatch.h" // Engine owns the AudioDispatch dispatch/settle cluster
#include "audio/music_coord.h"    // Engine owns the MusicCoord dialog↔music coordination
#include "audio/sequencer.h" // Engine owns the Sequencer libsnd tick wrapper (FUN_800909C0, wide-RE draft, unwired)
#include "audio/sfx.h"       // Engine owns the Sfx trigger subsystem (FUN_80074590)
#include "bg_scene_transition_sm.h"            // Engine owns the BG scene-transition fade manager
#include "core/assets/asset.h"                 // Engine owns the Asset loader subsystem
#include "core/frame/frame_cadence.h"          // Engine owns FrameCadence — the one frame-rate decision
#include "core/frame/frame_cut.h"              // Engine owns FrameCut — whether a sealed record is a cut
#include "demo.h"                              // Engine owns the Demo front-end MENU stage machine
#include "math/mathlib.h"                      // Engine owns the Bit game-flag bitmap subsystem
#include "object/animation.h"                  // Engine owns the Animation per-object VM stepper
#include "object/behavior_dispatch.h"          // Engine owns the per-object BehaviorDispatch subsystem
#include "object_list.h"                       // Engine owns the ObjectList entity-list walkers
#include "object_table.h"                      // Engine owns the ObjectTable 40-slot dispatcher
#include "overrides/native_override_catalog.h" // overlay image residency and scoped native bindings
#include "parallax_bg.h"                       // Engine owns the SOP parallax-BG state machine
#include "player/actor_tomba.h"                // Engine owns Tomba's per-frame logic + growth/movement
#include "player/collision.h"                  // Engine owns the Collision grid-family subsystem
#include "render/cull.h"                       // Engine owns the Cull visibility subsystem
#include "scene/field_transition.h"            // Engine owns the FieldTransition sub-scene/door/area FADE machine
#include "scene/mode_state_arm.h"              // Engine owns the ModeStateArm arm-primitive pair
#include "scene/scene_events.h"                // Engine owns the SceneEvents arm subsystem (FUN_80040B48)
#include "scene/script_interp.h"               // Engine owns the ScriptInterp cutscene-script dispatcher
#include "scene_transition.h"                  // Engine owns the SceneTransition subsystem instance
#include "sop.h"                               // Engine owns the Sop intro-cutscene field stage machine
#include "transition_state3.h"                 // Engine owns the TransitionState3 walker instance
#include "ui/font.h"                           // Engine owns the Font boot-time init subsystem
#include "world/area_slots.h"                  // Engine owns the AreaSlots slot-table state machine
#include "world/graphics_bind.h"               // Engine owns the GraphicsBind object render-bind subsystem
#include "world/placement.h"                   // Engine owns the Placement field-object driver
#include "world/pool.h"                        // Engine owns the Pool per-area init subsystem
#include "world/spawn.h"                       // Engine owns the Spawn entity-spawn/despawn subsystem
#include <optional>
class Core;

class Engine {
public:
  // Back-pointer set once by Core's constructor (same pattern as
  // ScreenFade::core).
  Core *core = nullptr;

  // The ONE owner of "how many display fields does one logic frame span", and of the guest's
  // own dwell counter that expresses it. Tomba! 2's frame-rate lever is a guest byte the engine's
  // main loop compares against a vblank-ticked counter
  // (`do {} while (DAT_800e809c < DAT_1f800235)` at 0x80050CC8..0x80050CF4), and the guest writes
  // that byte exactly once, at boot, from a literal compiled into the instruction word. This
  // class holds the decision and publishes it through the guest's own field, so the number the
  // per-frame work is driven by and the number the engine reads are the same number.
  // Full recovery, instruction words included: game/core/frame_cadence.h.
  tomba::FrameCadence cadence_;
  tomba::FrameCadence &cadence() {
    return cadence_;
  }
  const tomba::FrameCadence &cadence() const {
    return cadence_;
  }
  tomba::FrameCut frameCut_;
  tomba::FrameCut &frameCut() {
    return frameCut_;
  }
  const tomba::FrameCut &frameCut() const {
    return frameCut_;
  }

  // One-shot debug PLAYER teleport (REPL / debug-server `tp X Y Z`).
  // CutsceneCamera is instantiated per-call so its own members can't persist
  // across the set/consume boundary; live on Engine (per-Core) instead. Was
  // file-scope s_tp_pending / s_tp_x/y/z; cross-Core shared meant an
  // `@a tp X Y Z` teleported BOTH SBS cores (deglobalize 2026-07-03).
  //
  // IT WRITES TOMBA'S MASTER POSITION, NOT THE CAMERA — the follow camera then
  // moves because it tracks him. The name is historical; `devTeleport` below is
  // what it does.
  //
  // CONSUMED BY Engine::devTeleportApply, called from Engine::frameUpdate — the
  // ONE update/audio body TombaFrameDriver calls on every exec path — NOT from a
  // camera method. It used to be consumed inside CutsceneCamera::trackXZ, which
  // only runs in the follow-camera mode: MEASURED 2026-08-06, `tp` fired in
  // area 0 (1 `[tp] Tomba ->` line) and NEVER FIRED in areas 13/14/20 (0 lines
  // each) while the REPL still printed "tp camera -> (x,y,z)" — a dev teleport
  // that silently did nothing over most of the game and reported success. The
  // frame step always runs, so consumption no longer depends on which camera
  // mode the area is in.
  bool mCamTpPending = false;
  int32_t mCamTpX = 0, mCamTpY = 0, mCamTpZ = 0;
  // Arm / disarm the one-shot teleport (`tp X Y Z` / bare `tp`).
  void camTeleport(int x, int y, int z) {
    mCamTpX = x;
    mCamTpY = y;
    mCamTpZ = z;
    mCamTpPending = true;
  }
  void camTeleportOff() {
    mCamTpPending = false;
  }
  // Apply an armed teleport to Tomba's master position. No-op when nothing is
  // armed. Called once per frame from Engine::frameUpdate, before that frame's
  // state update.
  void devTeleportApply();

  // DEV WARP area index (game/core/dev_areas.cpp) — backs the RmlUi warp
  // selector and the REPL `warp` range guard. Static: no Engine state, and the
  // framework reaches them through GameHooks so it never learns a Tomba guest
  // address or an area name. devAreaName returns "" for an area whose name has
  // no source yet (see docs/areas.md — an index is a fact, a name is a claim).
  static int devAreaCount();
  static const char *devAreaName(int area);
  static bool devWarpAllowed(Core *c); // a warp is only legal from the field (GAME stage)

  // `debug stage` change-detector for the RUNNING dispatcher's
  // sm[0x4a]/sm[0x4c] log line (was a function-local static pair in the stage
  // dispatchers — per-Core so SBS's two cores log independently).
  uint16_t mLast4a = 0xffff, mLast4c = 0xffff;

  // ── Scene subsystem instances owned by Engine
  // ───────────────────────────────────── Callers reach them as
  // `eng(c).sceneTransition.method(args)`.
  SceneTransition sceneTransition;         // area-mask trigger + sub-scene swap handshake
  TransitionState3 transitionState3;       // mid-transition entity walker (guest FUN_8007B04C)
  ObjectList objectList;                   // per-frame entity-list walkers (guest FUN_8007A904 /
                                           // FUN_80069B28)
  Array8Dispatch array8Dispatch;           // 8-slot fixed array dispatcher   (guest FUN_80026368)
  ObjectTable objectTable;                 // 40-slot fixed object table       (guest FUN_80026C88)
  Demo demo;                               // front-end DEMO / MENU stage machine (docs/engine_re.md)
  Sop sop;                                 // SOP intro-cutscene FIELD stage machine (guest 0x80109450)
  FieldTransition fieldTransition;         // sm[0x4a]==5 sub-scene/door/area FADE transition
                                           // machine (guest FUN_80108A60 + workers)
  BgSceneTransitionSm bgSceneTransitionSm; // BG scene-transition fade manager
                                           // (guest FUN_8002655C)
  AreaFadeSequencer areaFadeSequencer;     // A0L per-node fade sequencer (guest FUN_8010957C)
  ParallaxBg parallaxBg;                   // SOP parallax-BG state machine (guest FUN_8010BFFC)
  Pool pool;                               // per-area object-pool + control-block init (world subsystem)
  Placement placement;                     // field object-placement driver (guest FUN_80072A78/DDC)
  GraphicsBind graphicsBind;               // per-object render-bind subsystem (guest
                                           // FUN_8007AAE8 et al.)
  Font font;                               // boot-time font / text init subsystem (guest FUN_80075130)
  Animation animation;                     // per-object animation-VM stepper       (guest FUN_80076D68)
  Asset asset;                             // asset loader — LZ + texgroup + VRAM upload + boot preload
  // START, DEMO, and GAME reuse the stage-code slot independently of MODE.
  std::optional<psx::cpu::ImageIdentity> activeStageOverlay;
  // The fixed MODE slot holds one A00..A0L code image at a time. Keep its image token per Core so
  // replacing an area retires the old address-qualified native registrations before binding the new one.
  std::optional<psx::cpu::ImageIdentity> activeModeOverlay;
  // OPN/CRD code shares 0x8018A000 with raw area and texture staging data.
  std::optional<psx::cpu::ImageIdentity> activeAreaOverlay;
  MusicCoord musicCoord;                     // dialog ↔ ingame-music coordination (instant-CD-safe PC mod)
  Collision collision;                       // collision-grid family (list-scan + grid
                                             // setup/query/resolve/step)
  Bit bit;                                   // game progress-flag bitmap bit-test (FUN_8004D7EC / D868)
  Spawn spawn;                               // entity spawn/despawn dispatcher (FUN_8007A980 / A624 / 3116C)
  BehaviorDispatch behaviors;                // per-object handler dispatch registry (50 native behaviors)
  Cull cull;                                 // per-object visibility cull / margin re-include
  SceneEvents sceneEvents;                   // field-wide scene-event arm primitive (FUN_80040B48)
  Sfx sfx;                                   // sound-FX trigger dispatcher (FUN_80074590)
  AudioDispatch audioDispatch;               // field-audio dispatch/settle cluster
                                             // (FUN_800750D8 et al.)
  tomba::audio::Sequencer sequencer;         // libsnd per-VBlank tick wrapper and channel leaves
  AreaSlots areaSlots;                       // area-slot table state machine (FUN_80075A80 / FUN_80074AF0)
  ModeStateArm modeStateArm;                 // mode-state arm primitive pair (FUN_8005082C /
                                             // FUN_800508A8)
  ScriptInterp script;                       // cutscene bytecode dispatcher (FUN_80041098 et al.)
  tomba::player::ActorTomba actorTomba;      // Tomba's per-frame logic + growth/movement over G block
  AttackOrbitSubstate attackOrbit;           // A00 overlay: node[3]==0x80/0x81
                                             // sub-behaviors (FUN_80145AF0/801458E0)
  ReleaseTriggerMotion releaseTriggerMotion; // release-trigger sub-motion
                                             // cluster (FUN_80123E9C family)
  ActorMeleeEngage actorMeleeEngage;         // A00-overlay melee-engage/reposition/arm
                                             // leaf (FUN_80112188)
  MeleeProximity meleeProximity;             // melee-proximity/approach-anchor leaf (FUN_8001F9DC)

  // ── GAME-stage entry points (called by the scheduler each frame)
  // ──────────────────────────── stagePrologue: one-time prologue that runs
  // when the GAME task enters — task-slot setup, first
  //   field-mode load, etc. (was `ov_game_stage_prologue`).
  // frame: one frame of the GAME stage — the sm[0x48] dispatcher + tail.
  // Returns 0 if the current
  //   sm[0x48] state isn't owned natively yet (scheduler falls back to
  //   substrate); non-zero if handled here (was `ov_game_frame`).
  void stagePrologue();
  int frame();
  // stageBodyFaithful: the whole overlay guest 0x8010637C arc as a native task body
  // on a PcScheduler fiber (faithful-execution model, same shape as
  // Demo::stageBodyFaithful): stagePrologue (already guest-frame-faithful),
  // then the substate loop — frame() for natively-owned sm[0x48] states, the
  // guest handler dispatched for unowned ones, substrate tail yield each
  // iteration. native_sync keeps the per-tick stagePrologue()/frame() pair via
  // runGameStanza.
  void stageBodyFaithful();
  // stageMain: OLD guest-loop entry (prologue + guest-continuation into 0x801063F4).
  // Kept as reference / fallback (native per-frame path calls stagePrologue +
  // frame directly). Was ov_game_stage_main.
  void stageMain();

  // submode0 / submode1: the two sm[0x4a] state handlers under the GAME running
  // sub-mode (sm[0x48]==2). submode0 = SOP intro; submode1 = the walkable field
  // area machine (0x801088D8). Called by frame() when the corresponding
  // sm[0x4a] is owned natively. Formerly `ov_game_submode0` /
  // `ov_game_submode1` free functions in engine.cpp.
  void submode0();
  void submode1();
  // submode1Case0Native: case 0 of the walkable-field area machine — the owned
  // area-DATA load. It completes through the same seam FUN_80044BD4 uses (flag-2 RNG stamp)
  // and then runs the load synchronously, so no wait frame or loading-screen service exists
  // after host work returns.
  void submode1Case0Native();

  // sm[0x48] state handlers (0=area INIT, 1=area RESUME-INIT, 2=RUNNING
  // dispatcher) and the sm[0x4c] area LOAD/TRANSITION machine. Formerly
  // `ov_game_s48_0..2` / `ov_game_s4c` / `ov_game_s48_2_frame` free statics in
  // engine.cpp. All operate on the sm task-state block at *0x1F800138 that
  // Engine already owns.
  void stageAreaInit();
  void stageResumeInit();
  void stageRunning();
  // areaLoadState(): guest FUN_80106478 — the 9-state (sm[0x4c]==0..8) area
  // LOAD/TRANSITION body reached via the guest's own case-label addresses
  // (0x801064c4/106510/106580/1065b8/1066b8/106830/106930/10694c/1069b4). It is
  // a plain switch(sm[0x4c]) — Ghidra recovered it as ONE function, not 9
  // separate ones — so calling this method fresh reaches the same case body the
  // guest-continuation used to jump into directly (the redirect skipped only the
  // leading areaSlots.updateTail() call and the switch dispatch itself, both
  // idempotent / already-current here). Owns control flow + every
  // sm/DAT_800bf84a/etc state write; the pause/save-menu text-render leaves
  // (FUN_8007E8DC/8007ED5C/8007EE74/8007EF60), the quit-confirm dialog
  // sub-machine (FUN_8007BF20, its own DAT_800bf84a-keyed SM), and the
  // audio-tick leaf (FUN_8001CF2C) stay substrate (font/pad-hold leaves with no
  // PC-native equivalent yet). Ghidra decomp scratch/decomp/game_all_list.c
  // (FUN_80106478) + scratch/decomp/area_load_leaves.c (the sub-leaves, RE'd to
  // classify/rule them out as spawns). NEGATIVE RESULT (walkable-Tomba spawn
  // hunt): none of states 0-8 spawn anything — this machine is entirely
  // pause/save/quit menu UI + area audio-fade sequencing, driven by pad-edge
  // bits at 0x800E7E68. Rules out the last un-RE'd sibling of the sm[0x4c] area
  // machine.
  void areaLoadState();

  // fieldFrame / fieldFrameX: the FIELD per-frame update body (guest
  // 0x80108B0C) and its mid-transition twin (0x80108BE4, sm[0x4a]==5
  // running-during-fade variant). Called by the sm[0x4c] handlers and the
  // transition drivers. Formerly ov_field_frame / ov_field_frame_x.
  void fieldFrame();
  void fieldFrameX();

  // fieldRun / fieldRunX: the sm[0x4c]==2 field RUNNING sub-machine on sm[0x4e]
  // (guest FUN_80106B98) and its mid-transition twin (0x801070B4, sm[0x4c]==3).
  // Formerly ov_field_run / ov_field_run_x. Called by Engine::submode1 /
  // fieldFrameX.
  void fieldRun();
  void fieldRunX();

  // submitPage810c: the sm[task+0x6b]==1 page-1 (pause-menu dim) fade branch of
  // the master submit dispatcher at guest 0x8010810C. Owns just the dim-fade
  // shape (subtractive #808080 held on page-1) + a substrate dispatch to the
  // still-unowned menu draw at 0x801084F8; other pages fall through to
  // substrate 0x8010810C. Was ov_game_submit_810c in engine.cpp.
  void submitPage810c();

  // frameUpdate: TombaFrameDriver's update/audio phase. Runs the still-PSX per-frame update leaf,
  // then owns the per-vblank audio (sequencer tick + SPU field advance) and returns. Presentation,
  // pacing, scheduler, and render submission remain TombaFrameDriver responsibilities. Was the free
  // function `ov_frame_update` in game_tomba2.cpp.
  void frameUpdate();

  // padEdgeFence: FUN_800788AC — the per-frame INPUT-EDGE FENCE
  // (docs/engine_re.md "Per-frame
  //   fence FUN_800788ac"). Called exactly once per logic frame;
  //   `frameUpdate()`'s `typed runtime address dispatch(c, 0x800788ACu)` routes here via the
  //   override (pad_edge_fence_install, §9-verified against gen 2026-07-16).
  //   Full RE in game/input/pad_edge_fence.cpp.
  void padEdgeFence();
  void padFenceTail();           // FUN_8005229C — padEdgeFence's tail; subsystem
                                 // UNRESOLVED, see impl
  void allocRecordForSelector(); // FUN_8008913C — record[0]/record[1] of the
                                 // 0x80102500 array

  // drawOTag (libgpu FUN_80081560): walks the guest OT into the GPU device. Called by TombaFrameDriver.
  void drawOTag(uint32_t otHead);
  // While a bug report takes this frame, hand its ordering table to the report's PSX render.
  void captureBugReportReference(uint32_t otHead);

  // startBinStage / startBinStageFaithful: the two stage-0 START.BIN task bodies, one per
  // execution model. Both run StartBinStage (game/scene/start_bin_stage.h); the native body
  // returns and Engine performs the stage swap to DEMO, the faithful body never returns (its
  // guest stage-swap arm parks the fiber until the stanza cancels it).
  void startBinStage();
  void startBinStageFaithful();

  // task0Bootstrap: the boot-init entry that (a) resolves \BIN\START.BIN
  // natively via disc_find_file, (b) records its {LBA,size} into 0x800be1e0,
  // and (c) enters stage 0 via startStage(0). Called once from
  // native_boot.cpp's game_init (was `native_task0_bootstrap(Core*)`).
  void task0Bootstrap();

  // startStage(stage): FUN_80052078 — switch task 0 to the given stage. Loads
  // the stage overlay through tomba::stage::loadOverlay, sets task state=3, hits the three
  // BIOS EnterCS/B0F-reset/ExitCS leaves, then yields the scheduler. Public
  // wrapper (was the free fn `demo_start_stage(Core*, uint32_t)`) plus the
  // previous file-scope `native_start_stage` helper. Also the tail of
  // task0Bootstrap + stage0Advance's final step.
  void startStage(uint32_t stage);

  // ── ov_field_frame direct children (progressive class-ification)
  // ────────────────────────── areaModeDispatch: the 22-way area-mode
  // jump-table dispatcher at guest 0x8001CAC0. Reads the
  //   area RENDER-MODE byte at 0x800BF870 and dispatches to the overlay handler
  //   that owns that mode (each mode = an entry in the resident table at
  //   0x80010000; 10 of 22 slots are the "no-op default" stub 0x8001CB98, the
  //   other 12 jal one specific overlay leaf then return). Replaces `d0(c,
  //   0x8001cac0u)` in the field-frame body.
  void areaModeDispatch();

  // sceneEventFifo: the field EVENT/COMMAND-QUEUE state machine at guest
  // 0x80025588 (struct
  //   @0x800ed058). 3-state top switch on base[2]: state 0 arms + falls through
  //   to state 1 (active body drains a small FIFO); state >=2 no-op. Was `d0(c,
  //   0x80025588)`.
  void sceneEventFifo();
  // sceneRenderListBuilder: 2-phase scene/render-list builder driver at guest
  // 0x8004FE84 (struct
  //   @0x800bf548). Phase 0 arms it (snapshot list ptr 0x800ecf64 into
  //   base+0x2b0..0x2b8, advance phase to 1); phase 1 dispatches a sub-state
  //   handler (base[1] 0..3 -> distinct overlays); flag @0x800bf822 bit 0
  //   latched from (base[1]!=0 || base[0x0a]!=0). Was `d0(c, 0x8004fe84)`.
  void sceneRenderListBuilder();

  // sceneStateStep: the per-frame area SCENE-INIT / SCENE-RUN state machine at
  // guest 0x80050DE4.
  //   Two overlay-handler tables of 22 entries each (indexed by the same
  //   render-mode byte 0x800BF870 that areaModeDispatch uses), plus a
  //   scene-phase byte at 0x800F2418:
  //     phase == 0 -> INIT: call the INIT handler for the current area mode,
  //     set phase=1. phase == 1 -> RUN:  call the RUN handler for the current
  //     area mode (per-frame update). phase < 0 or >= 2 -> no-op.
  //   Both handler tables extracted verbatim from MAIN.EXE .text (@0x80015A40
  //   init, @0x80015A98 run, 21 overlay leaves + 1 default no-op each).
  //   Handlers take a0 = 0x800F2418 (the scene-state base). Replaces `d0(c,
  //   0x80050de4u)` in the field-frame body.
  void sceneStateStep();

  // modePerFrameDispatch: the second per-frame render-mode dispatcher at guest
  // 0x80022A80 (ov_field_frame
  //   calls it right after the object dispatcher). Keyed by the same
  //   render-mode byte 0x800BF870; reads a 24-entry function-pointer table at
  //   0x8009D1D4 (MAIN.EXE .rodata, one entry per area mode, all pointing at
  //   overlay leaves in the 0x8010xxxx..0x80117xxx range that the currently-
  //   loaded overlay owns). Mode 3 (A00 fisherman village) is explicitly
  //   SKIPPED before the table read — the only special case. Replaces `d0(c,
  //   0x80022a80u)` in the field-frame body.
  void modePerFrameDispatch();

  // Small per-object leaves shared across many behavior handlers. Ghidra decomp
  // scratch/decomp/batch_leaves.c — see each method's body for the full RE.
  // Kept as Engine methods (not a new class) since they're each self-contained
  // and only-recently-owned.

  // (FUN_80040CDC is ScriptInterp::init, not an anim leaf — a dead mis-named
  // duplicate
  //  Engine::animEnvInit was removed here; see engine.cpp +
  //  docs/findings/scene.md.)

  // animTick(obj): guest FUN_8004190C. Ticks the animation VM (substrate
  // FUN_80076D68) and
  //   stashes its returned byte into obj+0x79. Returns 1 (matches guest instruction path v0).
  uint32_t animTick(uint32_t obj);

  // announcerCue(id, flag): guest FUN_8004ED94. Enqueues an announcer/UI cue by
  // (id, flag) via
  //   the substrate FUN_8004FA38 leaf — table lookup at DAT_800BF7FC then
  //   DAT_800BF800 base.
  void announcerCue(uint32_t id, uint8_t flag);

  // (FUN_800518FC is NodeXform::buildWithOffset, not an Engine leaf — a dead
  // duplicate
  //  Engine::objMatrixCompose was removed here; see engine.cpp +
  //  docs/findings/render.md.)

  // walkStart(obj, mode, subMode): guest FUN_80054D14. Transitions the object
  // into anim `mode`.
  //   Returns 0 if already in that mode; else 1 (and delegates to FUN_80077C40
  //   / FUN_80077CFC).
  uint32_t walkStart(uint32_t obj, uint32_t mode, int16_t subMode);

  // (playerGrowthStep moved to ActorTomba::growthStep — see
  // game/player/actor_tomba.h; callers
  //  reach it as `eng(c).actorTomba.growthStep(mode)` since obj is always G.)

  // uploadModeSprites(): guest FUN_80067DA8. Uploads 5 sprite patterns (each a
  // 16×1 BGR555 strip)
  //   to VRAM at fixed (X=0x1F0, Y=0x1E2/0x1E5/0x1C9/0x1D0/0x1B3) rects. The 5
  //   source pointers are selected by mode byte DAT_800BF88D (0/1/2 → three
  //   distinct pattern sets in MAIN.EXE .rodata at 0x800A4800..0x800A49C0). Any
  //   other mode value returns without touching VRAM. Called from
  //   `beh_sop_intro_pilot`'s state-0 init; the guest passes a0 = master G but
  //   the function ignores it, so this method takes no args. Ghidra decomp
  //   scratch/decomp/fun_80067da8.c
  //   + disas.py of FUN_80081218 (= libgpu-style `LoadImage(RECT*, data)`; kept
  //   as substrate leaf).
  void uploadModeSprites();

  // gStateMutate(G, op): guest FUN_80058304. A 15-case dispatcher that flips
  // flag bits on the
  //   master G block's byte pair (G+0x174, G+0xD) and fires an SFX/announcer
  //   cue via FUN_8004ED94(id, 0x41) for most cases (and Sfx::trigger for cases
  //   8-9's alternate cue path). Cases 8/9/0xD/0xE also stash G+0x174 into
  //   DAT_800BF881 on the "already-set" fast path and otherwise pump
  //   obj[5]=<id-cue>, obj[6]=2 into a target obj (currently G). Case 0xC
  //   (called from Engine::fieldRun state 2) is the "clear G flags" body —
  //   zeros G+0x174 / G+0xD, then normalizes G+0x17E (clears the 0x8000 sign
  //   bit if set; force-sets to 0x10 when the 0x200 flag is set, plus clears
  //   G+0x6F and mirrors 0x800BF89E / 0x800BF88F). Every case tail stamps
  //   DAT_800BF881 = G+0x174 (post-mutation snapshot). Ghidra decomp
  //   scratch/decomp/fieldrun_s2_init.c. `op` is the u8 selector (a1 in the
  //   guest instruction path).
  void gStateMutate(uint32_t G, uint8_t op);

  // postRenderTick: small 3-state machine on byte 0x800BF842 at guest
  // 0x80077D8C, called after the
  //   per-frame render submit. `b42 & 0x7F` selects: 1 = trigger FX 41 then set
  //   b42=0x87; 2 = trigger FX 42 then clear b42; otherwise = decrement b42.
  //   Trigger call = FUN_80074590(id, 2, -65) — a sound/vibration fx queue
  //   leaf, still substrate. Replaces `d0(c, 0x80077d8cu)` in ov_field_frame.
  void postRenderTick();
  // guest frame + jal-site ras

  // frameStartTick: the first call in ov_field_frame's gameplay block (guest
  // 0x80059D28). A per-frame
  //   prologue that (a) decrements the frame counter at 0x800BF819 and masks
  //   two 12-bit heading fields when it fires, (b) zeroes a bank of
  //   frame-scoped flags at G+0x177..0x17B and 0x1F80027A, (c) increments the
  //   per-frame stamp at 0x1F800247, (d) if 0x800BF841 == 0 dispatches a
  //   mode-keyed per-frame handler (modes 2/3/7/20 -> overlay-specific, else
  //   0x8005950C) with G as a0 and clears 0x1F800230, (e) seeds the master
  //   position + heading (G+0x2E/0x32/0x36/0x56/0x58) into scratchpad
  //   0x1F800160..0x1F80016A for the projection to read, (f) latches
  //   0x800BF81E=1 when 0x800BF9C3&0x80, (g) ticks the sub-counter at G+0x180
  //   when 0x1F800137 (pause flag) is 0, (h) advances the LFSR rand at
  //   0x8009A450 every frame. Replaces `d0(c, 0x80059d28u)` in ov_field_frame.
  void frameStartTick();

  // ── Boot-time INIT (called from native_boot.cpp before the scheduler starts)
  // ────────────────── The engine's own PC-native init prefix (was the free
  // functions `eng_init_*` in startup.cpp). Each method reimplements the
  // corresponding guest init leaf: frame/display/camera state, the entity pool
  // control block, the allocator + dispatch table, mode ctrl, input, and the
  // orchestrator that sequences them. Called via eng(c).initX() from
  // native_boot.
  void initFrameState(); // FUN_80050A0C — vblank + double-buffer pacing state.
                         // The frame-rate decision itself is NOT a literal here: it belongs to
                         // cadence() below, which publishes it into the guest's own quota byte
                         // (0x1F800235) the way FUN_80050A0C's single `sb` does.
  void initDisplay();    // FUN_800509B4 — GTE projection CRs + display H
  void initCamera();     // FUN_80050A80 — camera scratchpad matrices + state
  // FUN_80051794 (identity 3x3 rot + zero translation) is owned by
  // `Mtx::identity` (game/math/mtx.h) — call `mtxOf(c).identity(p)` directly
  // (deduped 2026-07-08: this used to carry a redundant copy).
  void initEntityPool(); // FUN_8007B328 — entity-pool control block + fixed-pt
                         // scales
  void initAlloc(uint32_t s1,
                 uint32_t s2); // FUN_80088B00 — allocator + 6-entry dispatch table
  void initInput();            // FUN_80087A60 → 80086970 — input subsystem
  void initSubsystems();       // FUN_800520E0 — orchestrator (entity pool + alloc +
                               // mode + input)

  // seedDirectionMasks(flipped): guest FUN_8007B2C0. Seeds the 4 fixed-point
  // direction-mask words
  //   at 0x1F800170/172/174/176 (the same words initEntityPool's
  //   FUN_8007B2C0(0) call seeds at boot). flipped==0 ->
  //   0x8000/0x4000/0x2000/0x1000; else the reverse order
  //   0x1000/0x2000/0x4000/0x8000 (a mirrored-facing swap). Ghidra decomp
  //   scratch/decomp/fun_8007b2c0.c.
  void seedDirectionMasks(bool flipped);
  // reloadEntityPool(): guest FUN_8007B3F4. Re-copies the staged per-area
  // entity-pool control
  //   bytes (0x800BFE4C/4D/4E/4F + 0x800BF8A3/88A/88B) onto the live control
  //   header initEntityPool seeded at boot (0x800FB166/167/161/162/163/164/165
  //   respectively), then reseeds the direction masks via
  //   seedDirectionMasks(staged byte 0x800BFE4C). Called by
  //   Engine::areaLoadState's state 7 confirm-branch (quit-confirm accepted ->
  //   resume). Ghidra decomp scratch/decomp/area_load_leaves.c.
  void reloadEntityPool();

  // setAreaStartPos(): guest FUN_80078824. Loads the player's spawn position +
  // facing for the
  //   current (area,sub-area) from the per-area start-pos table
  //   0x800A54A8[area] -> 8-byte record [sub]: writes X/Y/Z (<<16 fixed) to
  //   0x800BF890/894/898 and the facing byte (masked 0x7F) to 0x800BFE38. Leaf,
  //   no frame. Called from the area machine's spawn state.
  void setAreaStartPos();

  // activeModeCtx(): guest FUN_80086604. Returns the active mode/draw-env
  // context pointer held at
  //   0x800ABE20. Leaf accessor, no frame.
  uint32_t activeModeCtx();

  // installModeHandlers(): guest FUN_80086738. Installs the 2-entry mode
  // handler table at
  //   0x80102444 ([0]=0x800867CC, [1]=Engine::runModeEnter=0x80086764) and
  //   zeroes the guard slot 0x80102440 + trailing slot 0x8010244C. Leaf, no
  //   frame. Called from initAlloc's post-init.
  void installModeHandlers();

  // runModeEnter(): guest FUN_80086764. If both bit0 flags in the mode ctx
  // (*0x800ABE98)[+0]/[+4]
  //   are set, dispatches the mode-enter handler at 0x800ABE60 (when non-null)
  //   and returns 1; else returns 0. Ready-FRAME (sp-24, ra@16). One of the
  //   handlers installModeHandlers registers.
  uint32_t runModeEnter();
};
