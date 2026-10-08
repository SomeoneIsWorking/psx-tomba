// class BehaviorDispatch — the engine's per-object BEHAVIOR-HANDLER dispatcher. See header.
//
// The native object-behavior table: guest handler address → its PC-native reimplementation + a short
// name. This is the SINGLE registry of which per-object behaviors the engine owns natively (readable
// C) vs. still run as the guest instruction path substrate. Introspectable via `nativeName()` so the `ents` REPL
// diagnostic can flag, per live object, whether its logic is owned. Add a row when you own a new
// behavior; `dispatchNative` + the ents flag both read this table.
//
// Was three free functions in engine_tomba2.cpp — dispatch_obj_method / dispatch_native_behavior /
// behavior_native_name. Promoted here as `class BehaviorDispatch` on Engine so callers reach the
// dispatcher through the object graph (`eng(c).behaviors.dispatchObj(obj, h)`), matching the
// Collision / Bit / Spawn class-instance pattern.
#include "behavior_dispatch.h"
#include "cfg.h"
#include "core.h"
#include "core/engine/engine.h" // class Engine (for Core::engine)
#include "core/entry/game_ctx.h"
#include "game.h"
#include "guest_call.h"
#include <cstdint>
#include <cstdio>
#include <cstring>

// Behavior entry-point declarations. Each `beh_*` lives in its own game/ai/beh_*.cpp file as an
// external-linkage function (the verify wrapper — no `_run` forwarder layer anymore). Kept as free-
// function pointers in the table below; class-ifying each individual behavior is a separate axis.
void beh_scene_ui_trigger(Core *c);           // 0x800739AC
void beh_typed_init_scene_trigger(Core *c);   // 0x80073CD8
void beh_pickup_collect_trigger(Core *c);     // 0x800741DC
void beh_substate_edge_orchestrator(Core *c); // 0x8012EB54 (overlay)
void beh_jumptable_release_trigger(Core *c);  // 0x80124E74 (overlay)
void beh_typed_table_seed_gate(Core *c);      // 0x80133C14 (overlay)
void beh_typed_jumptable_pair(Core *c);       // 0x80138FC8 (overlay)
void beh_cull_substate_orchestrator(Core *c); // 0x8013259C (overlay)
void beh_id_compare_motion_dispatch(Core *c); // 0x80145230 (overlay)
void beh_jumptable_flag_gate(Core *c);        // 0x8012D4EC (overlay)
void beh_cull_tick_render(Core *c);           // 0x8012D404 (overlay)
void beh_sibling_angle_track(Core *c);        // 0x801395C0 (overlay)
void beh_visibility_gate_dispatch(Core *c);   // 0x8004C238 (resident)
void beh_record_list_scanner(Core *c);        // 0x8004CE14 (resident)
void beh_area_event_dispatch(Core *c);        // 0x80071A3C (resident)
void beh_pad_child_linker(Core *c);           // 0x8006F2D0 (resident)
void beh_scatter_record_dither(Core *c);      // 0x8013C538 (overlay)
void beh_area_threshold_ptr_swap(Core *c);    // 0x8013C3F4 (overlay)
void beh_scatter_ramp_machine(Core *c);       // 0x8013C9C0 (overlay)
void beh_pure_inner_dispatch(Core *c);        // 0x80136D9C (overlay)
void beh_anim_trigger_gates(Core *c);         // 0x80129C00 (overlay)
void beh_box_seed_phase_gate(Core *c);        // 0x8012A0B8 (overlay)
void beh_typed_anim_spawn(Core *c);           // 0x8012DA04 (overlay)
void beh_id_routed_dispatch(Core *c);         // 0x80121978 (overlay)
void beh_pure_substate_dispatch(Core *c);     // 0x80125E0C (overlay)
void beh_linked_advance_branch(Core *c);      // 0x80128760 (overlay)
void beh_typed_init_exit_poker(Core *c);      // 0x80118240 (overlay)
void beh_child_trig_motion(Core *c);          // 0x8013A900 (overlay)
void beh_prng_velocity_machine(Core *c);      // 0x80117658 (overlay)
void beh_quad_record_table_seed(Core *c);     // 0x80135D64 (overlay)
void beh_flagbit_timer_machine(Core *c);      // 0x8013B2E4 (overlay)
void beh_two_child_steer(Core *c);            // 0x80131D08 (overlay)
void beh_single_child_cull(Core *c);          // 0x80132400 (overlay)
void beh_twin_record_steer(Core *c);          // 0x80133D6C (overlay)
void beh_multi_record_phase_machine(Core *c); // 0x80134FD8 (overlay)
void beh_sine_motion_sfx(Core *c);            // 0x80136158 (overlay)
void beh_box_rearm_sub(Core *c);              // 0x8013ADBC (overlay)
void beh_node3_router(Core *c);               // 0x8011CBD0 (overlay)
void beh_actor_move_sm(Core *c);              // 0x8011D988 (overlay)
void beh_variant_actor_sm(Core *c);           // 0x8011D578 (overlay)
void beh_lift_platform(Core *c);              // 0x8013A330 (overlay)
void beh_event_record_machine(Core *c);       // 0x80136954 (overlay)
void beh_typed_variant_router(Core *c);       // 0x8011C164 (overlay)
void beh_camera_target_follow(Core *c);       // 0x80059ED8 (resident)
void beh_cube_text_spawn(Core *c);            // 0x8003AD48 (resident)
void beh_area_transition_machine(Core *c);    // 0x80127798 (overlay)
void beh_rand_phase_cull(Core *c);            // 0x8002918C (resident)
void beh_pos_history_trail(Core *c);          // 0x80029B40 (resident)
void beh_variant_overlay_lifecycle(Core *c);  // 0x8007DC38 (resident)
void beh_a06_multi_actor(Core *c);            // 0x801189E8 (A06 overlay — cutscene fade director)
void beh_script_interp_step(Core *c);         // 0x80041098 (resident) — ScriptInterp::step wrapper
void beh_a06_scripted_actor(Core *c);         // 0x8013AA14 (A06 overlay) — cutscene scripted actor,
                                              //   inlines the 0x80139C84 / 0x80139A28 chain natively
                                              //   so ScriptInterp::step actually fires (was dark)
// Script-driven fade fns (op 0x03E fnptrs from A06 cutscene scripts) — beh_a06_script_fades.cpp.
void beh_a06_fade_flash_ramp_80139728(Core *c);  // 0x80139728
void beh_a06_spawn_follow_obj_8013AEF0(Core *c); // 0x8013AEF0
void beh_a06_sound_cmd_wait_8013AFD8(Core *c);   // 0x8013AFD8
void beh_a06_spawn_subobj_8013B074(Core *c);     // 0x8013B074
void beh_a06_fade_ramp_8013B178(Core *c);        // 0x8013B178
void beh_a06_music_cue_8013B274(Core *c);        // 0x8013B274
void beh_a06_timer_gate_8013B29C(Core *c);       // 0x8013B29C
void beh_a08_scene_actor(Core *c);               // 0x801280D0 (A08 overlay) — outer per-object
                                                 //   scene actor + inlined 0x80127C58 cutscene
                                                 //   director. Closes the 3rd un-owned fade
                                                 //   caller for bug #27 (A08 FUN_80127C58 fade)
// SOP intro-cutscene per-object handlers — the 3 scene actors Sop::fieldMode spawns at sm[0x50]==0
// LOAD (game/scene/sop.cpp:505-514, table @0x8010C98C). Ghidra decomp scratch/decomp/sop_scene_actors.c.
void beh_sop_intro_pilot(Core *c);       // 0x8010ACFC (SOP overlay) — script-driven master-G actor
void beh_sop_intro_lifted(Core *c);      // 0x8010B798 (SOP overlay) — Y-lifted secondary actor
void beh_sop_intro_narration(Core *c);   // 0x8010B990 (SOP overlay) — narration-beat spawner
void beh_seaside_prox_substate(Core *c); // 0x8013C1DC (A00 overlay) — closes the seaside
                                         //   placement-installed handler set (last unowned entry)
void beh_sop_overlay_shadow(Core *c);    // 0x8010AB38 (SOP overlay) — the per-actor drop-shadow
                                         //   quad FUN_8010AE30 spawns for the pilot/lifted actors

namespace {
struct NativeBeh {
  uint32_t addr;
  void (*fn)(Core *);
  const char *name;
};
constexpr NativeBeh kTable[] = {
    {0x8004CE14u, beh_record_list_scanner, "record_list_scanner"},
    {0x8006F2D0u, beh_pad_child_linker, "pad_child_linker"},
    {0x80071A3Cu, beh_area_event_dispatch, "area_event_dispatch"},
    {0x800739ACu, beh_scene_ui_trigger, "scene_ui_trigger"},
    {0x80073CD8u, beh_typed_init_scene_trigger, "typed_init_scene_trigger"},
    {0x800741DCu, beh_pickup_collect_trigger, "pickup_collect_trigger"},
    {0x8012EB54u, beh_substate_edge_orchestrator, "substate_edge_orchestrator"},
    {0x80124E74u, beh_jumptable_release_trigger, "jumptable_release_trigger"},
    {0x80133C14u, beh_typed_table_seed_gate, "typed_table_seed_gate"},
    {0x80138FC8u, beh_typed_jumptable_pair, "typed_jumptable_pair"},
    {0x8013259Cu, beh_cull_substate_orchestrator, "cull_substate_orchestrator"},
    {0x80145230u, beh_id_compare_motion_dispatch, "id_compare_motion_dispatch"},
    {0x8012D4ECu, beh_jumptable_flag_gate, "jumptable_flag_gate"},
    {0x8012D404u, beh_cull_tick_render, "cull_tick_render"},
    {0x801395C0u, beh_sibling_angle_track, "sibling_angle_track"},
    {0x8013C538u, beh_scatter_record_dither, "scatter_record_dither"},
    {0x8013C3F4u, beh_area_threshold_ptr_swap, "area_threshold_ptr_swap"},
    {0x8013C9C0u, beh_scatter_ramp_machine, "scatter_ramp_machine"},
    {0x80136D9Cu, beh_pure_inner_dispatch, "pure_inner_dispatch"},
    {0x80129C00u, beh_anim_trigger_gates, "anim_trigger_gates"},
    {0x8012A0B8u, beh_box_seed_phase_gate, "box_seed_phase_gate"},
    {0x8012DA04u, beh_typed_anim_spawn, "typed_anim_spawn"},
    {0x80121978u, beh_id_routed_dispatch, "id_routed_dispatch"},
    {0x80125E0Cu, beh_pure_substate_dispatch, "pure_substate_dispatch"},
    {0x80128760u, beh_linked_advance_branch, "linked_advance_branch"},
    {0x80118240u, beh_typed_init_exit_poker, "typed_init_exit_poker"},
    {0x8013A900u, beh_child_trig_motion, "child_trig_motion"},
    {0x80117658u, beh_prng_velocity_machine, "prng_velocity_machine"},
    {0x80135D64u, beh_quad_record_table_seed, "quad_record_table_seed"},
    {0x8013B2E4u, beh_flagbit_timer_machine, "flagbit_timer_machine"},
    {0x80131D08u, beh_two_child_steer, "two_child_steer"},
    {0x80132400u, beh_single_child_cull, "single_child_cull"},
    {0x80133D6Cu, beh_twin_record_steer, "twin_record_steer"},
    {0x80134FD8u, beh_multi_record_phase_machine, "multi_record_phase_machine"},
    {0x80136158u, beh_sine_motion_sfx, "sine_motion_sfx"},
    {0x8013ADBCu, beh_box_rearm_sub, "box_rearm_sub"},
    {0x8011CBD0u, beh_node3_router, "node3_router"},
    {0x8011D988u, beh_actor_move_sm, "actor_move_sm"},
    {0x8011D578u, beh_variant_actor_sm, "variant_actor_sm"},
    {0x8013A330u, beh_lift_platform, "lift_platform"},
    {0x80136954u, beh_event_record_machine, "event_record_machine"},
    {0x8011C164u, beh_typed_variant_router, "typed_variant_router"},
    {0x80059ED8u, beh_camera_target_follow, "camera_target_follow"},
    {0x8003AD48u, beh_cube_text_spawn, "cube_text_spawn"},
    {0x80127798u, beh_area_transition_machine, "area_transition_machine"},
    {0x8004C238u, beh_visibility_gate_dispatch, "visibility_gate_dispatch"},   // resident
    {0x8002918Cu, beh_rand_phase_cull, "rand_phase_cull"},                     // resident
    {0x80029B40u, beh_pos_history_trail, "pos_history_trail"},                 // resident
    {0x8007DC38u, beh_variant_overlay_lifecycle, "variant_overlay_lifecycle"}, // resident  // resident
    {0x801189E8u, beh_a06_multi_actor, "a06_multi_actor"},                     // A06 overlay
    {0x80041098u, beh_script_interp_step, "script_interp_step"}, // resident — cutscene script dispatch loop
    {0x8013AA14u,
     beh_a06_scripted_actor,
     "a06_scripted_actor"}, // A06 overlay — the caller-chain root that reaches ScriptInterp
    // Script-driven fade fnptrs (called through op 0x03E's native path via ScriptInterp::callFnptr):
    {0x80139728u, beh_a06_fade_flash_ramp_80139728, "a06_fade_flash_ramp"},   // 8-state gray flash + music trigger
    {0x8013AEF0u, beh_a06_spawn_follow_obj_8013AEF0, "a06_spawn_follow_obj"}, // follow-obj spawner + hook
    {0x8013AFD8u, beh_a06_sound_cmd_wait_8013AFD8, "a06_sound_cmd_wait"},     // sound-cmd queue + wait
    {0x8013B074u, beh_a06_spawn_subobj_8013B074, "a06_spawn_subobj"},         // subobj spawn + field seed
    {0x8013B178u, beh_a06_fade_ramp_8013B178, "a06_fade_ramp"},               // 3-state simple ramp fade
    {0x8013B274u, beh_a06_music_cue_8013B274, "a06_music_cue"},               // one-shot music/SFX cue
    {0x8013B29Cu, beh_a06_timer_gate_8013B29C, "a06_timer_gate"},             // 60-frame counted gate
    {0x801280D0u, beh_a08_scene_actor, "a08_scene_actor"},         // A08 overlay — closes the 3rd un-owned fade caller
    {0x8010ACFCu, beh_sop_intro_pilot, "sop_intro_pilot"},         // SOP overlay — script-driven master-G intro actor
    {0x8010B798u, beh_sop_intro_lifted, "sop_intro_lifted"},       // SOP overlay — Y-lifted secondary intro actor
    {0x8010B990u, beh_sop_intro_narration, "sop_intro_narration"}, // SOP overlay — narration-beat spawner
    {0x8013C1DCu, beh_seaside_prox_substate, "seaside_prox_substate"}, // A00 overlay — last seaside placement handler
    {0x8010AB38u, beh_sop_overlay_shadow, "sop_overlay_shadow"},       // SOP overlay — per-actor drop-shadow quad tick
};
} // namespace

void BehaviorDispatch::dispatchObj(uint32_t obj, uint32_t handler) {
  Core *c = this->core;
  c->r[4] = obj; // $a0 — the behaviors read the object here
  // Gameplay prefers the native behavior table. An unowned handler crosses the single PSXPort
  // guest boundary below; there is no player-selectable substrate mode.
  // PSXPORT_DEBUG=uitrig traces the scene-UI trigger at the dispatch site, so verification observes
  // the same event shape on both legs.
  if (handler == 0x800739ACu) {
    const uint32_t o = c->r[4];
    cfg_logf("uitrig",
             "obj=%08X st=%u sub=%u f2b=%u pad=%04X lock=%02X",
             o,
             c->mem_r8(o + 4),
             c->mem_r8(o + 5),
             c->mem_r8(o + 0x2b),
             c->mem_r16(0x800E7E68u),
             c->mem_r8(0x800E7E80u));
  }
  if (traceDispatch(obj, handler)) {
    return;
  }
  if (!dispatchNative(handler)) {
    psx::cpu::dispatchGuestToReturn0(*c, handler, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
  }
}

// traceDispatch — the per-invocation node-delta log (see header). Same shape on both legs.
bool BehaviorDispatch::traceDispatch(uint32_t obj, uint32_t handler) {
  Core *c = this->core;
  if (mTraceAddr == 0) {
    const char *e = cfg_str("PSXPORT_BEH_TRACE");
    mTraceAddr = (e && *e) ? ((uint32_t)strtoul(e, nullptr, 16) & 0x1FFFFFFFu) | 0x80000000u : 0xFFFFFFFFu;
  }
  if (handler != mTraceAddr) {
    return false;
  }

  uint8_t before[kTraceNodeBytes];
  for (uint32_t i = 0; i < kTraceNodeBytes; i++) {
    before[i] = c->mem_r8(obj + i);
  }
  if (!dispatchNative(handler)) {
    psx::cpu::dispatchGuestToReturn0(*c, handler, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
  }

  // One line per invocation, listing every node byte the handler changed. An invocation that changes
  // nothing still prints, so the two legs' logs stay line-aligned and `diff` reports the FIRST
  // invocation that differs rather than a shifted stream.
  char delta[512];
  int n = 0;
  for (uint32_t i = 0; i < kTraceNodeBytes && n < (int)sizeof delta - 24; i++) {
    const uint8_t now = c->mem_r8(obj + i);
    if (now != before[i]) {
      n += snprintf(delta + n, sizeof delta - n, " %02X:%02X->%02X", i, before[i], now);
    }
  }
  delta[n] = '\0';
  cfg_logf("behtrace", "f%u %08X obj=%08X v0=%08X%s", c->game->timing.logicFrame, handler, obj, c->r[2], delta);
  return true;
}

bool BehaviorDispatch::dispatchNative(uint32_t handler) {
  Core *c = this->core;
  // PSXPORT_BEH_SUBSTRATE=<hex,hex> — force named handlers back to the substrate without a rebuild.
  // This exists because a single bad native handler is otherwise very hard to isolate: it does not
  // crash, it corrupts game state (the 2026-07-21 save-sign softlock was one handler out of 65, found
  // by bisecting this list). Diagnostic only; the default is empty and changes nothing.
  static const char *s_forced = nullptr;
  static bool s_init = false;
  if (!s_init) {
    s_init = true;
    s_forced = cfg_str("PSXPORT_BEH_SUBSTRATE");
  }
  if (s_forced && *s_forced) {
    char hex[16];
    snprintf(hex, sizeof hex, "%08X", handler);
    if (strcasestr(s_forced, hex)) {
      return false;
    }
  }
  for (const NativeBeh &b : kTable) {
    if (b.addr == handler) {
      noteNativeHit(handler, b.name);
      b.fn(c);
      return true;
    }
  }
  return false;
}

// noteNativeHit — the `behcov` coverage probe (see header). Counts per handler; logs the first hit
// and every decade after it, so one replay run yields "which handlers did this scenario reach, and
// roughly how hard" in a greppable form.
void BehaviorDispatch::noteNativeHit(uint32_t handler, const char *name) {
  // Latch the channel flag once: this runs on EVERY native behaviour dispatch (thousands per frame),
  // and cfg_dbg does a channel-table lookup per call. Same latching convention as VerifyHarness::on.
  if (mCovOn < 0) {
    mCovOn = cfg_dbg("behcov") ? 1 : 0;
  }
  if (!mCovOn) {
    return;
  }
  Cov *slot = nullptr;
  for (int i = 0; i < mNCov; i++) {
    if (mCov[i].addr == handler) {
      slot = &mCov[i];
      break;
    }
  }
  if (!slot) {
    if (mNCov >= kMaxCov) {
      return;
    }
    slot = &mCov[mNCov++];
    slot->addr = handler;
    slot->hits = 0;
  }
  const uint64_t n = ++slot->hits;
  bool milestone = n == 1;
  for (uint64_t decade = 10; decade <= 1000000 && !milestone; decade *= 10) {
    milestone = (n == decade);
  }
  if (milestone) {
    cfg_logf("behcov", "%08X %-28s hits=%llu", handler, name, (unsigned long long)n);
  }
}

const char *BehaviorDispatch::nativeName(uint32_t handler) const {
  for (const NativeBeh &b : kTable) {
    if (b.addr == handler) {
      return b.name;
    }
  }
  return nullptr;
}
