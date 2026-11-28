# The C++ dependencies: fetched at their pinned versions and checked by SHA-256 (11.C4), and Z3 (11.C3).
include(FetchContent)
set(FETCHCONTENT_TRY_FIND_PACKAGE_MODE NEVER)

FetchContent_Declare(pugixml URL ${FUSION_PUGIXML_URL} URL_HASH SHA256=${FUSION_PUGIXML_SHA256}
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_Declare(nlohmann_json URL ${FUSION_JSON_URL} URL_HASH SHA256=${FUSION_JSON_SHA256}
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_Declare(fast_float URL ${FUSION_FAST_FLOAT_URL} URL_HASH SHA256=${FUSION_FAST_FLOAT_SHA256}
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
set(FASTFLOAT_TEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(pugixml nlohmann_json fast_float)

if(FUSION_BUILD_TESTS)
  FetchContent_Declare(Catch2 URL ${FUSION_CATCH2_URL} URL_HASH SHA256=${FUSION_CATCH2_SHA256}
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
  FetchContent_MakeAvailable(Catch2)
  list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
endif()

if(FUSION_Z3_PROVIDER STREQUAL "SYSTEM")
  if(APPLE)   # Homebrew's prefix, which CMake does not search by itself on Apple silicon (decision 0041)
    if(DEFINED ENV{HOMEBREW_PREFIX})
      list(APPEND CMAKE_PREFIX_PATH "$ENV{HOMEBREW_PREFIX}")
    endif()
    list(APPEND CMAKE_PREFIX_PATH /opt/homebrew /usr/local)
  endif()
  find_path(FUSION_Z3_INCLUDE_DIR z3++.h REQUIRED)
  find_library(FUSION_Z3_LIBRARY z3 REQUIRED)
  file(STRINGS "${FUSION_Z3_INCLUDE_DIR}/z3_version.h" _z3_version_lines REGEX "#define Z3_(MAJOR|MINOR|BUILD)_")
  foreach(_part MAJOR_VERSION MINOR_VERSION BUILD_NUMBER)
    string(REGEX MATCH "Z3_${_part} +([0-9]+)" _match "${_z3_version_lines}")
    list(APPEND _z3_found "${CMAKE_MATCH_1}")
  endforeach()
  list(JOIN _z3_found "." FUSION_Z3_FOUND_VERSION)
  if(NOT FUSION_Z3_FOUND_VERSION STREQUAL FUSION_Z3_VERSION)
    if(APPLE)   # a macOS build uses the system's Z3, Homebrew's, as FuSION 1 did on Macs (decision 0041)
      message(STATUS "Z3 ${FUSION_Z3_FOUND_VERSION} on macOS; Linux builds pin ${FUSION_Z3_VERSION}, so witnesses and solve times can differ (decision 0041)")
    else()
      message(FATAL_ERROR "The system Z3 is ${FUSION_Z3_FOUND_VERSION}; the build expects ${FUSION_Z3_VERSION} (11.C3, decision 0039)")
    endif()
  endif()
  add_library(z3::libz3 UNKNOWN IMPORTED)
  set_target_properties(z3::libz3 PROPERTIES
    IMPORTED_LOCATION "${FUSION_Z3_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${FUSION_Z3_INCLUDE_DIR}")
  message(STATUS "Z3 ${FUSION_Z3_FOUND_VERSION} from the system: ${FUSION_Z3_LIBRARY}")
else()
  message(FATAL_ERROR "FUSION_Z3_PROVIDER=PINNED fetches Z3 5.3.3 (11.C3), which waits for experiment X11; configure with -DFUSION_Z3_PROVIDER=SYSTEM (decision 0039)")
endif()
