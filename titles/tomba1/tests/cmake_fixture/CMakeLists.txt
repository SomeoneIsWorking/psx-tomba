cmake_minimum_required(VERSION 3.24)
project(tomba1_cmake_fixture LANGUAGES NONE)

include(CTest)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

# The production fragment requires this framework-owned target. A dependency-only fixture is enough
# to validate the title fragment because the framework itself is verified in its own repository.
add_custom_target(psxport)
include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/tomba1_port.cmake")
