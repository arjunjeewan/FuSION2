# core's own build steps, included by backend/CMakeLists.txt's fusion_add_module (target fusion_core,
# folder ${dir}). Build information (11.C16, 11.C19): the VERSION file, the commit and whether the
# backend's sources have uncommitted changes, stamped at configure time into a generated source.
# Section 11's layout puts the template in cmake/build_info.cpp.in; it lives here because modules
# never edit cmake/.

file(STRINGS "${PROJECT_SOURCE_DIR}/VERSION" FUSION_BUILD_VERSION LIMIT_COUNT 1)
string(STRIP "${FUSION_BUILD_VERSION}" FUSION_BUILD_VERSION)

set(FUSION_BUILD_COMMIT "unknown")
set(FUSION_BUILD_DIRTY "true")          # a build of unknown provenance counts as uncommitted
set(FUSION_BUILD_EPOCH "0")
find_package(Git QUIET)
if(GIT_FOUND)
  execute_process(COMMAND "${GIT_EXECUTABLE}" rev-parse --verify HEAD
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}" RESULT_VARIABLE _fusion_rc
    OUTPUT_VARIABLE _fusion_out ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
  if(_fusion_rc EQUAL 0 AND _fusion_out MATCHES "^[0-9a-f]+$")
    set(FUSION_BUILD_COMMIT "${_fusion_out}")
    # Uncommitted changes to anything the backend is built from, untracked files included; build
    # trees and docs are outside these paths. GIT_OPTIONAL_LOCKS=0 keeps git from rewriting the index.
    execute_process(COMMAND "${CMAKE_COMMAND}" -E env GIT_OPTIONAL_LOCKS=0
        "${GIT_EXECUTABLE}" status --porcelain --untracked-files=normal --
        CMakeLists.txt CMakePresets.json VERSION cmake backend
      WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}" RESULT_VARIABLE _fusion_rc
      OUTPUT_VARIABLE _fusion_status ERROR_QUIET)
    if(_fusion_rc EQUAL 0 AND _fusion_status STREQUAL "")
      set(FUSION_BUILD_DIRTY "false")
    endif()
    execute_process(COMMAND "${GIT_EXECUTABLE}" log -1 --format=%ct HEAD
      WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}" RESULT_VARIABLE _fusion_rc
      OUTPUT_VARIABLE _fusion_out ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(_fusion_rc EQUAL 0 AND _fusion_out MATCHES "^[0-9]+$")
      set(FUSION_BUILD_EPOCH "${_fusion_out}")
    endif()
    # A commit, a checkout or a staged change re-runs this configure step, so the stamp follows
    # them. An edit that is neither staged nor committed is seen at the next configure.
    execute_process(COMMAND "${GIT_EXECUTABLE}" rev-parse --absolute-git-dir
      WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}" RESULT_VARIABLE _fusion_rc
      OUTPUT_VARIABLE _fusion_git_dir ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
    execute_process(COMMAND "${GIT_EXECUTABLE}" symbolic-ref -q HEAD
      WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
      OUTPUT_VARIABLE _fusion_ref ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(_fusion_rc EQUAL 0)
      foreach(_fusion_file HEAD index packed-refs ${_fusion_ref})
        if(EXISTS "${_fusion_git_dir}/${_fusion_file}")
          set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_fusion_git_dir}/${_fusion_file}")
        endif()
      endforeach()
    endif()
  endif()
endif()

set(FUSION_BUILD_TYPE "${CMAKE_BUILD_TYPE}")
set(FUSION_BUILD_COMPILER "${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
string(TOLOWER "${CMAKE_SYSTEM_NAME}" _fusion_os)
if(_fusion_os STREQUAL "darwin")
  set(_fusion_os "macos")
endif()
string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" _fusion_arch)
if(_fusion_arch MATCHES "^(x86_64|amd64)$")
  set(_fusion_arch "x86_64")
elseif(_fusion_arch MATCHES "^(aarch64|arm64)$")
  set(_fusion_arch "arm64")
endif()
set(FUSION_BUILD_TARGET "${_fusion_os}-${_fusion_arch}")

configure_file("${dir}/build_info.cpp.in" "${CMAKE_CURRENT_BINARY_DIR}/fusion_core_build_info.cpp" @ONLY)
target_sources(fusion_core PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/fusion_core_build_info.cpp")

# SHA-256 is hot even in Debug builds: an uncommitted build's build_id() hashes the executable in
# every process (11.C16), which takes about 1.7 s for a 35 MB Debug test binary at -O0 and 0.2 s at -O2.
set_source_files_properties("${dir}/src/sha256.cpp" PROPERTIES COMPILE_OPTIONS "$<$<CXX_COMPILER_ID:GNU,Clang>:-O2>")
