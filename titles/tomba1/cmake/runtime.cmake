# Tomba! 1's runtime library, linked into the Tomba product (tomba::Catalog), its tests and its
# identity check.

get_filename_component(TOMBA1_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(TOMBA1_EXECUTABLE
    "${CMAKE_SOURCE_DIR}/scratch/bin/tomba1/SCUS_942.36"
    CACHE FILEPATH "Identity-checked Tomba! 1 USA executable")

if(NOT TARGET psxport)
  message(FATAL_ERROR "Tomba! 1 scaffold requires the shared psxport library target")
endif()
if(NOT TARGET oracle_trace)
  add_subdirectory(
    "${PSXPORT_DIR}/tools/oracle"
    "${CMAKE_BINARY_DIR}/tomba1_oracle_build"
    EXCLUDE_FROM_ALL)
endif()

add_custom_target(tomba1_scaffold DEPENDS psxport)

add_library(
  tomba1_runtime STATIC
  "${TOMBA1_ROOT}/game/core/boot/cd_native_startup.cpp"
  "${TOMBA1_ROOT}/game/core/state/context.cpp"
  "${TOMBA1_ROOT}/game/core/frame/frame_driver.cpp"
  "${TOMBA1_ROOT}/game/core/task/guest_task_slots.cpp"
  "${TOMBA1_ROOT}/game/core/boot/native_boot.cpp"
  "${TOMBA1_ROOT}/game/core/frame/stream_field_turn.cpp"
  "${TOMBA1_ROOT}/game/core/boot/sync_native.cpp"
  "${TOMBA1_ROOT}/game/core/entry/runtime.cpp"
  "${TOMBA1_ROOT}/game/core/entry/identity.cpp")
set_target_properties(tomba1_runtime PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
# The catalog includes only tomba1/game/core/entry/ (tomba1::Runtime, the identity) through titles/;
# every other concept directory is private to the library and to Tomba! 1's own tests.
set(TOMBA1_PRIVATE_INCLUDE_DIRS
    "${TOMBA1_ROOT}/game/core/state"
    "${TOMBA1_ROOT}/game/core/frame"
    "${TOMBA1_ROOT}/game/core/boot"
    "${TOMBA1_ROOT}/game/core/task")
target_include_directories(tomba1_runtime PUBLIC "${TOMBA1_ROOT}/.."
                                          PRIVATE ${TOMBA1_PRIVATE_INCLUDE_DIRS})
target_link_libraries(tomba1_runtime PUBLIC psxport)

# The catalog identity: the executable facts live in executable.json and reach the code only here.
file(READ "${TOMBA1_ROOT}/executable.json" TOMBA1_MANIFEST)
string(JSON TOMBA1_TITLE GET "${TOMBA1_MANIFEST}" title)
string(JSON TOMBA1_EXECUTABLE_NAME GET "${TOMBA1_MANIFEST}" disc_executable)
string(JSON TOMBA1_FILE_SIZE GET "${TOMBA1_MANIFEST}" file_size)
string(JSON TOMBA1_SHA256 GET "${TOMBA1_MANIFEST}" sha256)
set(TOMBA1_IDENTITY_DEFINITIONS
  "TOMBA1_TITLE=\"${TOMBA1_TITLE}\""
  "TOMBA1_EXECUTABLE_NAME=\"${TOMBA1_EXECUTABLE_NAME}\""
  "TOMBA1_FILE_SIZE=${TOMBA1_FILE_SIZE}u"
  "TOMBA1_SHA256=\"${TOMBA1_SHA256}\"")
foreach(fact IN ITEMS entry global_pointer text_address text_size stack_address stack_size)
  string(JSON value GET "${TOMBA1_MANIFEST}" ps_exe "${fact}")
  string(TOUPPER "${fact}" fact_upper)
  list(APPEND TOMBA1_IDENTITY_DEFINITIONS "TOMBA1_${fact_upper}=${value}u")
endforeach()
set_source_files_properties(
  "${TOMBA1_ROOT}/game/core/entry/identity.cpp"
  PROPERTIES COMPILE_DEFINITIONS "${TOMBA1_IDENTITY_DEFINITIONS}")

if(BUILD_TESTING)
  add_executable(
    tomba1_stream_field_turn_test
    "${TOMBA1_ROOT}/tests/test_stream_field_turn.cpp")
  set_target_properties(
    tomba1_stream_field_turn_test PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
  target_link_libraries(tomba1_stream_field_turn_test PRIVATE tomba1_runtime psxport_test_support)
  target_include_directories(tomba1_stream_field_turn_test PRIVATE ${TOMBA1_PRIVATE_INCLUDE_DIRS})
  add_test(NAME tomba1_stream_field_turn COMMAND tomba1_stream_field_turn_test)

  # The bounded multi-field resume, and the guest spin it exists to refuse (docs/issues/0007). Both
  # halves run real guest code on the real dynarec over real display-field budgets, so the test needs
  # no provisioned disc and the boot decompress that motivated the bound is not the only thing that
  # can exercise it. The refusal half is a DEATH TEST: the product ends the process, so the child is
  # forked and its signal, field count, and report are what the parent asserts.
  add_executable(
    tomba1_guest_task_budget_resume_test
    "${TOMBA1_ROOT}/tests/test_guest_task_budget_resume.cpp")
  set_target_properties(
    tomba1_guest_task_budget_resume_test PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
  target_link_libraries(tomba1_guest_task_budget_resume_test PRIVATE tomba1_runtime psxport_test_support)
  target_include_directories(tomba1_guest_task_budget_resume_test PRIVATE ${TOMBA1_PRIVATE_INCLUDE_DIRS})
  add_test(NAME tomba1_guest_task_budget_resume COMMAND tomba1_guest_task_budget_resume_test)
endif()

add_custom_target(
  tomba1_identity_check
  COMMAND
    "${Python3_EXECUTABLE}" "${TOMBA1_ROOT}/tools/verify_executable.py"
    --check "${TOMBA1_EXECUTABLE}"
  USES_TERMINAL
  VERBATIM)

add_custom_target(
  tomba1_verify_scaffold
  DEPENDS
    tomba1_scaffold
    tomba1_identity_check)
