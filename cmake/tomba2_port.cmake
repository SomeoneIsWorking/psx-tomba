# cmake/tomba2_port.cmake — compose the native Tomba! 2 owners around psxport's Lightrec runtime.

option(PSXPORT_BUILD_PORT "Build the Tomba!2 native port binary (tomba2_port)" ON)

# The framework static library (psxport) + its option(PSXPORT_BUILD_SMOKE) + the standalone smoke.
# Always included so `psxport` / `psxport_smoke` are buildable even when the game target is off.
include(${PSXPORT_DIR}/cmake/psxport.cmake)

if(NOT PSXPORT_BUILD_PORT)
  return()
endif()

if(NOT EXISTS "${PSXPORT_DIR}/runtime/cpu/dynarec_capabilities.h" OR
   NOT EXISTS "${PSXPORT_DIR}/runtime/cpu/native_dispatch.h")
  message(FATAL_ERROR
    "Tomba! 2's offline guest-source product was removed by the break-first migration, but "
    "PSXPORT_DIR=${PSXPORT_DIR} does not expose the required Lightrec runtime address API "
    "(runtime/cpu/dynarec_capabilities.h and runtime/cpu/native_dispatch.h).")
endif()

# ---- game source list (game/* only — the framework moved to cmake/psxport.cmake) --------------
set(GAME_SRC
  game/game_tomba2.cpp
  game/cd/libcd_dir_cache.cpp
  game/cd/libcd_native.cpp
  game/core/assets/asset.cpp
  game/core/debug/auto_drive.cpp
  game/core/debug/dev_warp.cpp
  game/core/frame/boot_cards.cpp
  game/core/frame/frame_cadence.cpp
  game/core/frame/frame_cut.cpp
  game/core/frame/frame_diagnostics.cpp
  game/core/frame/frame_driver.cpp
  game/core/frame/movie_policy.cpp
  game/core/hle/libapi_intr.cpp
  game/core/overrides/native_override_catalog.cpp
  game/core/entry/game_config.cpp
  game/core/entry/game_ctx.cpp
  game/core/entry/game_hooks.cpp
  game/core/entry/tomba_runtime.cpp
  game/core/entry/main.cpp          # process entry point: the multi-title host over the catalog
  game/core/entry/main_handoff.cpp
  game/core/entry/tomba_catalog.cpp
  game/core/entry/tomba_identity.cpp
  game/core/debug/dev_areas.cpp
  game/core/debug/repl_commands.cpp
  game/core/overrides/register_overrides.cpp
  game/core/assets/str.cpp
  game/math/mathlib.cpp
  game/math/rng.cpp
  game/math/mtx.cpp
  game/math/trig.cpp
  game/render/cull.cpp
  game/render/horizontal_visibility_cull.cpp
  game/player/collision.cpp
  game/player/actor_targeting.cpp       # FUN_8001FAE0 — acquire a target: reach, band, arc
    game/player/interact_scan.cpp
  game/world/spawn.cpp
  game/scene/scene_events.cpp
  game/scene/script_interp.cpp
  game/scene/script_interp_entry.cpp
  game/scene/script_opcode.cpp
  game/audio/sfx.cpp
  game/audio/audio_dispatch.cpp
  game/audio/sequencer.cpp
  game/audio/sequencer_channel_flags.cpp
  game/audio/sequencer_pitch_envelope.cpp
  game/audio/sequencer_voice_write.cpp
  game/audio/sequencer_tone_records.cpp
  game/audio/sequencer_voice_alloc.cpp
  game/audio/sequencer_voice_state.cpp
  game/world/area_slots.cpp
  game/scene/mode_state_arm.cpp
  game/world/placement.cpp
  game/world/graphics_bind.cpp
  game/world/render_record_pool.cpp
  game/world/pool.cpp
  game/world/collision_resolve.cpp
  game/object/actor_sm_reward.cpp
  game/object/cube_text_ledger.cpp
  game/ai/beh_scene_ui_trigger.cpp
  game/ai/beh_typed_init_scene_trigger.cpp
  game/ai/beh_pickup_collect_trigger.cpp
  game/ai/beh_substate_edge_orchestrator.cpp
  game/ai/substate_edge_native.cpp
  game/ai/assembly_companion.cpp         # FUN_80138A64 idle tick (camera hold + re-arm) + FUN_801389C8 rig pose
  game/ai/assembly_rider.cpp             # FUN_80118B10 rider perched on a seaside pump's arm-end (ride/hop/fling)
  game/ai/tilt_follower.cpp             # FUN_80125FE0 — pitch at half a sub-part's tilt
  game/ai/sway_schedule.cpp             # FUN_8012D27C — rocking rate winds down over the area's event sequence
  game/ai/rope_swing.cpp                # FUN_801281B8 — hanging rope: spring swing + per-segment bend
  game/ai/actor_object_contact.cpp      # FUN_8010E258 — actor-vs-object hit / proximity contact
  game/ai/actor_bump.cpp                # FUN_8010EA80 — actor bump: interact / push apart / recoil
  game/ai/placed_prop_sm.cpp
  game/ai/beh_jumptable_release_trigger.cpp
  game/ai/release_trigger_motion.cpp
  game/ai/beh_typed_table_seed_gate.cpp
  game/ai/beh_typed_jumptable_pair.cpp
  game/ai/beh_cull_substate_orchestrator.cpp
  game/ai/beh_id_compare_motion_dispatch.cpp
  game/ai/actor_zoned_attacker.cpp
  game/ai/actor_zoned_attacker_substate.cpp
  game/ai/actor_zoned_attacker_idle.cpp
  game/ai/actor_zoned_attacker_zone_classify.cpp
  game/ai/attack_orbit_substate.cpp
  game/ai/actor_melee_engage.cpp
  game/ai/beh_actor_tomba_proximity_combat.cpp
  game/ai/melee_proximity.cpp
  game/ai/beh_jumptable_flag_gate.cpp
  game/ai/beh_cull_tick_render.cpp
  game/ai/beh_sibling_angle_track.cpp
  game/ai/beh_visibility_gate_dispatch.cpp
  game/ai/beh_record_list_scanner.cpp
  game/ai/beh_area_event_dispatch.cpp
  game/ai/beh_pad_child_linker.cpp
  game/ai/beh_scatter_record_dither.cpp
  game/ai/beh_area_threshold_ptr_swap.cpp
  game/ai/beh_scatter_ramp_machine.cpp
  game/ai/beh_pure_inner_dispatch.cpp
  game/ai/beh_anim_trigger_gates.cpp
  game/ai/beh_box_seed_phase_gate.cpp
  game/ai/beh_typed_anim_spawn.cpp
  game/ai/beh_id_routed_dispatch.cpp
  game/ai/beh_pure_substate_dispatch.cpp
  game/ai/beh_linked_advance_branch.cpp
  game/ai/beh_typed_init_exit_poker.cpp
  game/ai/beh_child_trig_motion.cpp
  game/ai/beh_prng_velocity_machine.cpp
  game/ai/beh_quad_record_table_seed.cpp
  game/ai/beh_flagbit_timer_machine.cpp
  game/ai/beh_two_child_steer.cpp
  game/ai/beh_single_child_cull.cpp
  game/ai/beh_twin_record_steer.cpp
  game/ai/beh_multi_record_phase_machine.cpp
  game/ai/beh_sine_motion_sfx.cpp
  game/ai/beh_box_rearm_sub.cpp
  game/ai/beh_node3_router.cpp
  game/ai/beh_actor_move_sm.cpp
  game/ai/beh_variant_actor_sm.cpp
  game/ai/beh_lift_platform.cpp
  game/ai/beh_event_record_machine.cpp
  game/ai/beh_typed_variant_router.cpp
  game/ai/beh_camera_target_follow.cpp
  game/ai/beh_cube_text_spawn.cpp
  game/ai/beh_area_transition_machine.cpp
  game/ai/beh_rand_phase_cull.cpp
  game/ai/beh_pos_history_trail.cpp
  game/ai/beh_variant_overlay_lifecycle.cpp
  game/ai/beh_a06_multi_actor.cpp
  game/ai/beh_a06_scripted_actor.cpp
  game/ai/beh_a06_script_fades.cpp
  game/ai/beh_a08_scene_actor.cpp
  game/ai/beh_toy_spawn_family.cpp
  game/ai/beh_sop_intro_pilot.cpp
  game/ai/beh_sop_intro_lifted.cpp
  game/ai/beh_sop_intro_narration.cpp
  game/ai/sop_overlay_shadow.cpp
  game/ai/sop_intro_events.cpp
  game/ai/beh_seaside_prox_substate.cpp
  game/ai/area_seaside_perframe.cpp
  game/ai/beh_substate_edge_leaves.cpp
  game/player/actor_tomba.cpp
  game/player/actor_interaction.cpp
  game/scene/area_fade_sequencer.cpp
  game/scene/bg_scene_transition_sm.cpp
  game/scene/parallax_bg.cpp
  game/scene/scene_transition.cpp
  game/scene/transition_state3.cpp
  game/object/object_list.cpp
  game/object/array8_dispatch.cpp
  game/world/object_table.cpp
  game/object/animation.cpp
  game/input/pad_edge_fence.cpp
  game/ui/ui_sprite_compose.cpp
  game/ui/ui_sprite.cpp
  game/ui/panel_fill.cpp
  game/ui/dialog_backdrop.cpp
  game/ui/dialog_text_stream.cpp
  game/items/inventory.cpp
  game/ui/save_menu.cpp
  game/audio/music_coord.cpp
  game/scene/startup.cpp
  game/ui/font.cpp
  game/ui/panel.cpp
  game/ui/options_page.cpp
  game/scene/level_load.cpp
  game/scene/start_bin_stage.cpp
  game/object/behavior_dispatch.cpp
  game/render/node_xform.cpp
  game/render/render_frame.cpp
  game/render/wide_window.cpp
  game/core/engine/engine.cpp
  game/core/engine/engine_task_machine.cpp
  game/core/engine/engine_state_dispatch.cpp
  game/core/engine/engine_field_run.cpp
  game/core/engine/engine_scene_frame.cpp
  game/core/engine/engine_object_leaves.cpp
  game/core/engine/engine_frame_ticks.cpp
  game/scene/field_transition.cpp
  game/scene/sop.cpp
  game/scene/demo.cpp
  game/scene/card_load_machine.cpp
  game/camera/cutscene_camera.cpp
  game/camera/camera_look_builder.cpp
  game/math/gte_math.cpp
  game/math/wide_re_gte_transform3.cpp
  game/render/wide_re_libgpu_leaves.cpp
  game/render/wide_re_gpu_dma_queue.cpp
  game/render/wide_re_gpu_loadimage_streamer.cpp
  game/render/wide_re_gpu_putdrawenv.cpp
  game/render/libgpu_draw_env.cpp        # libgpu SetDrawEnv (0x80081FB0) — DRAWENV -> DR_ENV packet
  game/render/guest_ordering_table.cpp
  game/render/screen_fade.cpp
  game/render/margin_render.cpp
  game/render/quad_rtpt_submit.cpp
  game/render/model_packet.cpp
  game/render/lit_model_emitter.cpp
  game/render/unlit_model_emitter.cpp
  game/render/sway_model_emitter.cpp
  game/render/sop_ground.cpp
  game/render/rain_streaks.cpp
  game/render/letterbox_bars.cpp
  game/audio/native_audio.c
  game/audio/native_music.cpp
  game/audio/music_list.cpp
  game/render/overlay_gt3gt4.cpp
  game/render/overlay_ground_gt3gt4.cpp
  game/render/tile_grid_layer.cpp
  game/render/widescreen_margin_quad.cpp
  game/render/obj_model_view.cpp
  game/render/hud_gauge_emitter.cpp
  game/render/fx_sprite_swarm.cpp        # FUN_800281EC — per-particle member of the sprite family
  game/render/fx_sprite_anchored.cpp     # FUN_80027CB4, FUN_80027E5C — single-anchor members
  game/render/fx_sprite_publish.cpp      # the sprite family's shared anchor projection
  game/render/perobj_dispatch.cpp
  game/render/perobj_billboard.cpp
  game/render/subpart_walk_shared.cpp
  game/render/subpart_walk.cpp
  game/render/compose_tint_gate.cpp
  game/render/effect_mod.cpp
  game/render/text_label.cpp
  game/render/render_walk_dispatch.cpp
  game/render/overlay_type_dispatch.cpp
  game/render/objlist_walk.cpp)

add_executable(tomba2_port ${GAME_SRC})

# The catalog identity: the boot stub and MAIN.EXE facts live in one manifest and reach the code only here.
file(READ "${CMAKE_SOURCE_DIR}/config/tomba2-images.json" TOMBA2_IMAGES_JSON)
string(JSON TOMBA2_TITLE_LABEL GET "${TOMBA2_IMAGES_JSON}" title)
string(JSON TOMBA2_BOOT_NAME GET "${TOMBA2_IMAGES_JSON}" boot)
set(TOMBA2_IDENTITY_DEFINITIONS
  "TOMBA2_TITLE_LABEL=\"${TOMBA2_TITLE_LABEL}\""
  "TOMBA2_BOOT_NAME=\"${TOMBA2_BOOT_NAME}\""
  "TOMBA2_MAIN_NAME=\"MAIN.EXE\"")
foreach(identity IN ITEMS "BOOT;${TOMBA2_BOOT_NAME}" "MAIN;MAIN.EXE")
  list(POP_FRONT identity prefix image)
  string(JSON size GET "${TOMBA2_IMAGES_JSON}" images "${image}" size)
  string(JSON sha256 GET "${TOMBA2_IMAGES_JSON}" images "${image}" sha256)
  list(APPEND TOMBA2_IDENTITY_DEFINITIONS "TOMBA2_${prefix}_SIZE=${size}u" "TOMBA2_${prefix}_SHA256=\"${sha256}\"")
  foreach(fact IN ITEMS entry gp text_address text_size stack_address stack_offset)
    string(JSON value GET "${TOMBA2_IMAGES_JSON}" images "${image}" header "${fact}")
    string(TOUPPER "${fact}" fact_upper)
    list(APPEND TOMBA2_IDENTITY_DEFINITIONS "TOMBA2_${prefix}_${fact_upper}=${value}u")
  endforeach()
endforeach()
target_compile_definitions(tomba2_port PRIVATE ${TOMBA2_IDENTITY_DEFINITIONS})
# The framework's shader header is generated by the psxport library's custom target; the game exe
# (via gpu_vk.cpp in libpsxport) transitively needs it present before its own compile ordering.
add_dependencies(tomba2_port gen_gpu_shaders)

# C++17 for this target (engine), matching the framework library.
set_target_properties(tomba2_port PROPERTIES
  CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON
  ENABLE_EXPORTS ON                                   # -rdynamic: watchdog backtrace symbol names
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# GAME include dirs. The framework include dirs (RT, generated, vendored backends, SDL/freetype) are
# inherited PUBLICly from the psxport link below — only the game/* subfolders are added here.
target_include_directories(tomba2_port PRIVATE
  game  game/ai  game/audio  game/camera  game/cd  game/core  game/input  game/items  game/math  game/object  game/player  game/render  game/scene  game/ui
  game/world)

target_compile_options(tomba2_port PRIVATE -w -O2 -g
  ${SDL3_CFLAGS_OTHER} ${FREETYPE_CFLAGS_OTHER})

# The framework library carries all system/vendored link deps + compile defs as PUBLIC, so linking it
# is all the game exe needs.
target_link_libraries(tomba2_port PRIVATE psxport)

if(BUILD_TESTING)
  foreach(TOMBA2_HELP_ARGUMENT IN ITEMS -h --help)
    string(REPLACE "-" "" TOMBA2_HELP_SUFFIX "${TOMBA2_HELP_ARGUMENT}")
    add_test(
      NAME "tomba2_direct_help_${TOMBA2_HELP_SUFFIX}"
      COMMAND "$<TARGET_FILE:tomba2_port>" "${TOMBA2_HELP_ARGUMENT}")
    set_tests_properties(
      "tomba2_direct_help_${TOMBA2_HELP_SUFFIX}"
      PROPERTIES PASS_REGULAR_EXPRESSION "Usage: tomba2_port")
  endforeach()
endif()
