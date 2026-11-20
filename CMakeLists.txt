# FuSION 2.0: the backend's build (section 11 of the architecture).
cmake_minimum_required(VERSION 3.28)
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" FUSION_VERSION LIMIT_COUNT 1)
project(FuSION VERSION ${FUSION_VERSION} LANGUAGES CXX)

option(FUSION_BUILD_TESTS "Build the backend's tests" ON)
option(FUSION_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
# 11.C3 fetches the pinned Z3 by default. Until experiment X11 has run, the build links the
# system Z3 that FuSION 1 links (decision 0039).
set(FUSION_Z3_PROVIDER "SYSTEM" CACHE STRING "Where Z3 comes from: PINNED or SYSTEM (11.C3)")
set_property(CACHE FUSION_Z3_PROVIDER PROPERTY STRINGS PINNED SYSTEM)

set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(FusionVersions)
include(FusionFlags)
include(FusionInstall)
include(FusionDependencies)

if(FUSION_BUILD_TESTS)
  enable_testing()
endif()
add_subdirectory(backend)
