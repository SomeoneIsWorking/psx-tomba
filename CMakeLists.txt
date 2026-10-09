# Tomba PC ports, built on the shared psxport Lightrec runtime. The repository-level game/** tree
# belongs only to Tomba! 2. Tomba! 1 is a separate engine under titles/tomba1/** and may consume
# psxport, but it must never compile or include Tomba! 2 game code.
cmake_minimum_required(VERSION 3.21)
project(tomba2engine CXX C)

include(CTest)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

# PSXPORT_DIR — WHERE THE FRAMEWORK SOURCE COMES FROM. Defaults to external/psxport, which is NOT a
# git submodule any more: it is a relative symlink to the workspace's live framework checkout (local —
# so a framework edit is live in every port at once), or a clone of psxport main where there is no
# sibling checkout (fresh machine, CI, or anyone cloning this repo alone).
# tools/psxport_fetch.py --auto establishes whichever applies, and run.sh calls it. The PATH is
# unchanged, so every reference to external/psxport/... still works.
#
# Point it at another framework checkout to build this game against in-progress framework work:
#
#   cmake -S . -B build -DPSXPORT_DIR=$HOME/repo/psx/psxport
#
# It is a CACHE path, so it survives reconfigures, and `run.sh` passes it through from the
# environment. Everything else in this file and in cmake/ derives from it — never re-spell
# `external/psxport` at a call site, or the two halves of a build can come from different checkouts.
set(PSXPORT_DIR "${CMAKE_SOURCE_DIR}/external/psxport" CACHE PATH
    "psxport framework source directory (default: the external/psxport submodule)")
if(NOT EXISTS "${PSXPORT_DIR}/cmake/psxport.cmake")
  message(FATAL_ERROR "PSXPORT_DIR=${PSXPORT_DIR} has no cmake/psxport.cmake — "
                      "run `python3 tools/psxport_fetch.py --auto`, or point PSXPORT_DIR at a psxport checkout.")
endif()
message(STATUS "psxport framework: ${PSXPORT_DIR}")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(ZLIB REQUIRED)
find_package(zstd CONFIG QUIET)
if(NOT TARGET zstd::libzstd_shared)
  find_library(ZSTD_LIBRARY zstd REQUIRED)
  add_library(zstd::libzstd_shared SHARED IMPORTED)
  set_target_properties(zstd::libzstd_shared PROPERTIES IMPORTED_LOCATION "${ZSTD_LIBRARY}")
endif()
find_package(Threads REQUIRED)

# libchdr (CHD access) comes from the Beetle PSX submodule NESTED under the psxport submodule.
add_subdirectory(${PSXPORT_DIR}/vendor/beetle-psx/deps/libchdr ${CMAKE_BINARY_DIR}/libchdr_build EXCLUDE_FROM_ALL)

# The framework static library (psxport) — builds from external/psxport via PSXPORT_ROOT (self-locating).
include(${PSXPORT_DIR}/cmake/psxport.cmake)

# The game binary (tomba2_port): title-native game/** linked to the shared runtime. Guest code is
# read from authenticated user assets and translated at runtime; no emitted guest source is built.
include(cmake/tomba2_port.cmake)

# Tomba! 1 remains an isolated runtime product while its representative-gameplay evidence is open.
include(titles/tomba1/cmake/tomba1_port.cmake)

if(BUILD_TESTING)
  # Real-title evidence is local-only: CI has no licensed overlay images.
  add_executable(test_authentic_overlay_collision
      tests/test_authentic_overlay_collision.cpp
      tests/authenticated_image.cpp
      game/core/overrides/native_override_catalog.cpp)
  target_include_directories(test_authentic_overlay_collision PRIVATE game game/core tests)
  target_link_libraries(test_authentic_overlay_collision PRIVATE psxport)

  add_executable(test_authentic_resident_override
      tests/test_authentic_resident_override.cpp
      tests/authenticated_image.cpp
      game/core/overrides/native_override_catalog.cpp
      game/core/assets/str.cpp)
  target_include_directories(test_authentic_resident_override PRIVATE game game/core tests)
  target_link_libraries(test_authentic_resident_override PRIVATE psxport)

  add_executable(test_long_area_handler
      tests/test_long_area_handler.cpp
      tests/authenticated_image.cpp)
  target_include_directories(test_long_area_handler PRIVATE game game/core tests)
  target_link_libraries(test_long_area_handler PRIVATE psxport)

  add_executable(test_authentic_screen_fade
      tests/test_authentic_screen_fade.cpp
      tests/authenticated_image.cpp
      game/render/screen_fade.cpp
      game/render/guest_ordering_table.cpp
      game/render/wide_window.cpp
      game/core/overrides/native_override_catalog.cpp)
  target_include_directories(test_authentic_screen_fade PRIVATE game game/core tests)
  target_link_libraries(test_authentic_screen_fade PRIVATE psxport)

  add_executable(test_authentic_model_emitters
      tests/test_authentic_model_emitters.cpp
      tests/authenticated_image.cpp
      game/render/model_packet.cpp
      game/render/list_job.cpp
      game/render/list_state_producer.cpp
      game/render/lit_model_emitter.cpp
      game/render/unlit_model_emitter.cpp
      game/render/sway_model_emitter.cpp
      game/render/overlay_gt3gt4.cpp
      game/render/overlay_ground_gt3gt4.cpp
      game/render/guest_ordering_table.cpp
      game/render/horizontal_visibility_cull.cpp
      game/render/wide_window.cpp
      game/math/trig.cpp
      game/core/overrides/native_override_catalog.cpp
      game/scene/level_load.cpp)
  target_include_directories(test_authentic_model_emitters PRIVATE game game/core game/math game/render game/scene tests)
  target_link_libraries(test_authentic_model_emitters PRIVATE psxport)

  add_executable(test_native_override_catalog
      tests/test_native_override_catalog.cpp
      game/core/overrides/native_override_catalog.cpp
      game/scene/level_load.cpp)
  target_include_directories(test_native_override_catalog PRIVATE game game/core game/scene)
  target_link_libraries(test_native_override_catalog PRIVATE psxport)
  add_test(NAME tomba_native_override_catalog COMMAND test_native_override_catalog)

  # DEATH TEST for the issue-0015 abort. `bindResident` now refuses, by aborting, on any resident-form
  # declaration outside the resident text range — the defect that silently kept 42 native owners out
  # of the product. This covers the decision itself, which no build-time scan can make, and an untested
  # fatal path is one nobody dares change. The test forks and asserts the child died by SIGABRT,
  # because ctest does not invert WILL_FAIL for a signalled child.
  add_executable(test_native_override_unreachable_abort
      tests/test_native_override_unreachable_abort.cpp
      game/core/overrides/native_override_catalog.cpp
      game/scene/level_load.cpp)
  target_include_directories(test_native_override_unreachable_abort PRIVATE game game/core game/scene)
  target_link_libraries(test_native_override_unreachable_abort PRIVATE psxport)
  add_test(NAME tomba_native_override_unreachable_abort
           COMMAND test_native_override_unreachable_abort)

  add_executable(test_title_host_owners
      tests/test_title_host_owners.cpp
      game/core/entry/tomba_identity.cpp
      game/core/frame/movie_policy.cpp)
  target_include_directories(test_title_host_owners PRIVATE game game/core)
  target_compile_definitions(test_title_host_owners PRIVATE ${TOMBA2_IDENTITY_DEFINITIONS})
  target_link_libraries(test_title_host_owners PRIVATE psxport)
  add_test(NAME tomba_title_host_owners COMMAND test_title_host_owners)
  foreach(mode IN ITEMS skip forced headless)
    add_test(NAME tomba_movie_policy_${mode} COMMAND test_title_host_owners ${mode})
  endforeach()

  add_executable(test_gpu_timeout_dispatch
      tests/test_gpu_timeout_dispatch.cpp game/core/entry/game_config.cpp)
  target_include_directories(test_gpu_timeout_dispatch PRIVATE game game/core)
  target_link_libraries(test_gpu_timeout_dispatch PRIVATE psxport)
  add_test(NAME tomba_gpu_timeout_dispatch COMMAND test_gpu_timeout_dispatch)

  add_executable(test_libapi_vblank_mirror
      tests/test_libapi_vblank_mirror.cpp
      game/core/hle/libapi_intr.cpp
      game/core/overrides/native_override_catalog.cpp)
  target_include_directories(test_libapi_vblank_mirror PRIVATE game game/core)
  target_link_libraries(test_libapi_vblank_mirror PRIVATE psxport)
  add_test(NAME tomba_libapi_vblank_mirror COMMAND test_libapi_vblank_mirror)

  # The frame-cadence owner (game/core/frame/frame_cadence.h). The gate's own `sltu`, the incrementer's
  # read-modify-write, the u16 wrap, and the accounting denominators.
  add_executable(test_frame_cadence
      tests/test_frame_cadence.cpp
      game/core/frame/frame_cadence.cpp)
  target_include_directories(test_frame_cadence PRIVATE game game/core)
  target_link_libraries(test_frame_cadence PRIVATE psxport)
  add_test(NAME tomba_frame_cadence COMMAND test_frame_cadence)

  add_executable(test_parallax_scroll tests/test_parallax_scroll.cpp)
  target_include_directories(test_parallax_scroll PRIVATE game game/scene "${PSXPORT_DIR}/tests")
  add_test(NAME tomba_parallax_scroll COMMAND test_parallax_scroll)

  # The horizontal visibility cull owner (game/render/horizontal_visibility_cull.h). 4:3 identity is a
  # SWEEP over the predicate's whole domain, not a sample, and the site->slack table is pinned rather
  # than restated.
  add_executable(test_horizontal_visibility_cull tests/test_horizontal_visibility_cull.cpp)
  target_include_directories(test_horizontal_visibility_cull PRIVATE game/render)
  add_test(NAME tomba_horizontal_visibility_cull COMMAND test_horizontal_visibility_cull)

  # Cull keeps FUN_8007712C's and FUN_8002B278's own far limits; wider ones overran the packet pool.
  add_executable(test_cull_far_limits
      tests/test_cull_far_limits.cpp
      game/render/cull.cpp
      game/render/margin_render.cpp
      game/math/gte_math.cpp
      game/math/trig.cpp
      game/core/overrides/native_override_catalog.cpp)
  target_include_directories(test_cull_far_limits PRIVATE
      game game/ai game/audio game/camera game/cd game/core game/input game/items game/math game/object
      game/player game/render game/scene game/ui game/world)
  target_link_libraries(test_cull_far_limits PRIVATE psxport)
  add_test(NAME tomba_cull_far_limits COMMAND test_cull_far_limits)

  # The camera driver's mode table: the eighteen driver modes, the render-mode prologue that two of
  # them share, the per-render-mode height floors, and the ten shake states. This table replaced TWO
  # 18-arm switches — one native, one guest-faithful — that could have drifted apart silently. The
  # six distinct (mode, render mode) return constants are the load-bearing part: a body that pushes a
  # frame writes them into guest stack bytes, so collapsing them to one per mode is a behaviour change.
  add_executable(test_camera_mode_table tests/test_camera_mode_table.cpp)
  target_include_directories(test_camera_mode_table PRIVATE game game/camera)
  add_test(NAME tomba_camera_mode_table COMMAND test_camera_mode_table)

  # The cutscene script interpreter's three vocabulary owners: ScriptObject (the driven object's
  # block), the bytecode vocabulary (opcode ids, entry flags, handler returns, cursor moves), and
  # the fixed guest globals. Each makes a decision the raw accessors left to the reader — a field's
  # width and sign, an opcode's id mask, a name this repository has NOT read out of the image — and
  # each of those can be wrong in a way that still runs, which is what these assertions are for.
  add_executable(test_script_interp_vocabulary
      tests/test_script_interp_vocabulary.cpp
      game/scene/script_opcode.cpp)
  target_include_directories(test_script_interp_vocabulary PRIVATE game)
  target_link_libraries(test_script_interp_vocabulary PRIVATE psxport)
  add_test(NAME tomba_script_interp_vocabulary COMMAND test_script_interp_vocabulary)

  add_executable(test_producers
      tests/test_producers.cpp
      game/core/overrides/native_override_catalog.cpp
      game/scene/level_load.cpp
      game/render/tile_grid_layer.cpp
      game/render/libgpu_draw_mode.cpp
      game/render/list_job.cpp
      game/render/list_state_producer.cpp
      game/render/wide_window.cpp
      game/render/sop_ground.cpp
      game/render/rain_streaks.cpp
      game/render/guest_ordering_table.cpp
      game/ui/font.cpp
      game/ui/glyph_state.cpp)
  target_include_directories(test_producers PRIVATE game game/core game/math game/render game/scene tests)
  target_link_libraries(test_producers PRIVATE psxport)
  add_test(NAME tomba_producers COMMAND test_producers)

  add_executable(test_ordering_table tests/test_ordering_table.cpp game/render/guest_ordering_table.cpp)
  target_include_directories(test_ordering_table PRIVATE game game/core game/render tests)
  target_link_libraries(test_ordering_table PRIVATE psxport)
  add_test(NAME tomba_ordering_table COMMAND test_ordering_table)

  add_executable(test_letterbox_bars
      tests/test_letterbox_bars.cpp
      game/render/letterbox_bars.cpp
      game/render/guest_ordering_table.cpp
      game/render/wide_window.cpp
      game/ui/dialog_backdrop.cpp
      game/core/overrides/native_override_catalog.cpp)
  target_include_directories(test_letterbox_bars PRIVATE game game/core game/render tests)
  target_link_libraries(test_letterbox_bars PRIVATE psxport)
  add_test(NAME tomba_letterbox_bars COMMAND test_letterbox_bars)

  add_executable(test_screen_fade
      tests/test_screen_fade.cpp
      game/render/screen_fade.cpp
      game/render/guest_ordering_table.cpp
      game/render/wide_window.cpp
      game/core/overrides/native_override_catalog.cpp
      game/scene/level_load.cpp)
  target_include_directories(test_screen_fade PRIVATE game game/core game/render game/scene tests)
  target_link_libraries(test_screen_fade PRIVATE psxport)
  add_test(NAME tomba_screen_fade COMMAND test_screen_fade)

  add_executable(test_model_emitters
      tests/test_model_emitters.cpp
      game/render/model_packet.cpp
      game/render/list_job.cpp
      game/render/list_state_producer.cpp
      game/render/lit_model_emitter.cpp
      game/render/unlit_model_emitter.cpp
      game/render/sway_model_emitter.cpp
      game/render/overlay_gt3gt4.cpp
      game/render/overlay_ground_gt3gt4.cpp
      game/render/guest_ordering_table.cpp
      game/render/horizontal_visibility_cull.cpp
      game/render/wide_window.cpp
      game/math/trig.cpp
      game/core/overrides/native_override_catalog.cpp
      game/scene/level_load.cpp)
  target_include_directories(test_model_emitters PRIVATE game game/core game/math game/render game/scene tests)
  target_link_libraries(test_model_emitters PRIVATE psxport)
  add_test(NAME tomba_model_emitters COMMAND test_model_emitters)

  add_executable(test_render_record_pool tests/test_render_record_pool.cpp game/world/render_record_pool.cpp)
  target_include_directories(test_render_record_pool PRIVATE game game/core game/world tests)
  target_link_libraries(test_render_record_pool PRIVATE psxport)
  add_test(NAME tomba_render_record_pool COMMAND test_render_record_pool)

  add_executable(test_fx_sprite_publish
      tests/test_fx_sprite_publish.cpp
      game/render/fx_sprite_publish.cpp
      game/render/guest_ordering_table.cpp)
  target_include_directories(test_fx_sprite_publish PRIVATE game game/core game/math game/render tests)
  target_link_libraries(test_fx_sprite_publish PRIVATE psxport)
  add_test(NAME tomba_fx_sprite_publish COMMAND test_fx_sprite_publish)

  # Shrink-only legacy caps, all ratcheted down on 2026-09-28.
  #
  # `game/scene/script_interp.cpp` LEFT this list: it was 1317 and the cutscene script interpreter's
  # three vocabulary owners (game/scene/script_object.h, script_opcode.h, script_globals.h) took it to
  # 1196, which is under the 1200 default and needs no cap. A file under the default does not belong in
  # the legacy list — leaving it would be the same "keeps a cap it no longer needs" the gate prevents.
  #
  # `game/core/engine/engine.cpp` LEFT this list too: it was 3251, the largest single file in the repository.
  # `Engine` is still ONE class and game/core/engine/engine.h is still its single declaration — the 47 method
  # bodies moved to six sibling translation units split by the level of the guest's own state machine
  # they serve, with no method body changed. engine.cpp is now the banner and the includes, and every
  # new file is under the 1200 default.
  # `game/audio/sequencer.cpp` and `game/ai/actor_zoned_attacker.cpp` LEFT this list too: the
  # sequencer's 2768 lines split into seven translation units by leaf responsibility (dispatch, channel
  # flags, pitch/envelope ramps, voice writes, tone records, voice allocation, SPU voice state), and the
  # zoned attacker's 1496 into three (entry points, sub-state machine, idle). Both classes stay one
  # owner with one declaration header; only the translation units moved, and each group declares its own
  # overrides from the file that owns those bodies.
  # `game/camera/cutscene_camera.cpp` LEFT this list too: it was 1459, and the camera's look-point and
  # heading construction — the yaw/distance fold, the scripted angle builders, the distance/zoom solver
  # and the rotation builder, which `cutscene_camera.h` had already named "a cohesive future unit" —
  # moved to `game/camera/camera_look_builder.{h,cpp}` as `tomba::camera::LookAngleBuilder`. The shared
  # guest-memory view and the retained guest maths vocabulary each got their own header, so all three
  # owners read and call the guest one way. cutscene_camera.cpp is now under the 1200 default.
  # `game/player/actor_tomba.cpp` LEFT this list too: it was 1452, and "what touching an item means"
  # moved to `game/player/actor_interaction.{h,cpp}` as `tomba::player::ActorInteraction` — the six
  # per-item-kind leaves the two walks dispatch into, one method per guest item kind. The G-block lens
  # and the address vocabulary every Tomba owner shares moved to `game/player/tomba_state.h`, and
  # ActorTomba itself is now `tomba::player::ActorTomba`. actor_tomba.cpp is under the 1200 default.
  # `game/scene/demo.h` LEFT this list too. The caps are legacy EXCEPTIONS to the 1200-line default
  # (check_caps only compares line counts), so an 87-line header under one bought nothing but an
  # obligation: the gate fails a capped file that shrinks, demanding a ratchet on every edit. Demo
  # is one cohesive owner (the DEMO / front-end MENU substate machine) and belongs at the default.
  set(TOMBA_CPP_POLICY_CAPS)
  set(TOMBA_CPP_POLICY_ARGS)
  foreach(cap IN LISTS TOMBA_CPP_POLICY_CAPS)
    list(APPEND TOMBA_CPP_POLICY_ARGS --cap "${cap}")
  endforeach()

  add_test(
    NAME tomba_cpp_policy
    COMMAND
      "${Python3_EXECUTABLE}" "${PSXPORT_DIR}/tools/check_cpp_style.py"
      --root "${CMAKE_SOURCE_DIR}"
      --compile-commands "${CMAKE_BINARY_DIR}"
      ${TOMBA_CPP_POLICY_ARGS})
endif()
