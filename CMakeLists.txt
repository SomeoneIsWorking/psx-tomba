# The Tomba PC product, built on the shared psxport Lightrec runtime: one tomba2_port binary whose
# catalog holds Tomba! 1 and Tomba! 2. The repository-level game/** tree belongs only to Tomba! 2.
# Tomba! 1 is a separate engine under titles/tomba1/** and may consume psxport, but it must never
# compile or include Tomba! 2 game code; only the catalog names both.
cmake_minimum_required(VERSION 3.21)
project(tomba2engine CXX C)

find_package(Python3 REQUIRED COMPONENTS Interpreter)

# PSXPORT_DIR — WHERE THE FRAMEWORK SOURCE COMES FROM. Defaults to external/psxport, which is NOT a
# git submodule any more: it is a relative symlink to the workspace's live framework checkout (local —
# so a framework edit is live in every port at once), or a clone of psxport main where there is no
# sibling checkout (fresh machine, CI, or anyone cloning this repo alone).
# psxport's tools/psxport_fetch.py establishes whichever applies; run.sh runs it when the link is missing. The PATH is
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
                      "run ./run.sh once to fetch it, or point PSXPORT_DIR at a psxport checkout.")
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

# Tomba! 1's runtime library, which the product's catalog links.
include(titles/tomba1/cmake/runtime.cmake)

# The game binary (tomba2_port): title-native game/** linked to the shared runtime. Guest code is
# read from authenticated user assets and translated at runtime; no emitted guest source is built.
include(cmake/port.cmake)
