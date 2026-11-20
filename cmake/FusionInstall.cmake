# The installed home and the staging tree (section 11, "Finding the backend's own files (S1.20)").
#
# The build writes <build>/stage, laid out exactly like an installation:
#   bin/fusion       the fusion executable, built here (backend/CMakeLists.txt), beside its home (S1.20)
#   share/fusion/    the home: the source tree's share/fusion/, with a generated fusion-data.json
# <build>/stage/bin/fusion finds <build>/stage/share/fusion the way an installed fusion finds
# <prefix>/share/fusion, and `cmake --install` copies the staging tree to the prefix. The build links
# the system Z3 (decision 0039), so nothing is staged under lib/; the fonts, the bundled tools and the
# licence notices arrive with packaging.
#
# The top-level CMakeLists.txt includes this file, and backend/tests/CMakeLists.txt calls
# fusion_add_home_tests(). The build step that stages the home and the home's tests run this same file
# as a script: cmake -D FUSION_INSTALL_STEP=<stage|test_version|test_batch|test_fake_home> ... -P.

function(fusion_json_string out text)
  string(REPLACE "\\" "\\\\" text "${text}")
  string(REPLACE "\"" "\\\"" text "${text}")
  set(${out} "\"${text}\"" PARENT_SCOPE)
endfunction()

if(NOT DEFINED FUSION_INSTALL_STEP)
  # ---------------------------------------------------------------- configure: staging and install

  set(FUSION_STAGE_DIR "${PROJECT_BINARY_DIR}/stage")

  # `cmake --install` copies the staging tree and nothing else. pugixml and fast_float, compiled into
  # fusion, would install their headers, libraries and CMake packages, so their install rules are
  # switched off here, before FusionDependencies.cmake fetches them (the include order in CMakeLists.txt).
  set(PUGIXML_INSTALL OFF CACHE BOOL "" FORCE)
  set(FASTFLOAT_INSTALL OFF CACHE BOOL "" FORCE)

  # fusion-data.json's fusion_version and git_commit, read as core's module.cmake reads them for
  # build_info(), so that the home's version equals the binary's (S1.20, step 3).
  file(STRINGS "${PROJECT_SOURCE_DIR}/VERSION" _fusion_home_version LIMIT_COUNT 1)
  string(STRIP "${_fusion_home_version}" _fusion_home_version)
  set(_fusion_home_commit "unknown")
  find_package(Git QUIET)
  if(GIT_FOUND)
    execute_process(COMMAND "${GIT_EXECUTABLE}" rev-parse --verify HEAD
      WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}" RESULT_VARIABLE _fusion_rc
      OUTPUT_VARIABLE _fusion_out ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(_fusion_rc EQUAL 0 AND _fusion_out MATCHES "^[0-9a-f]+$")
      set(_fusion_home_commit "${_fusion_out}")
    endif()
  endif()

  file(GLOB_RECURSE _fusion_home_files CONFIGURE_DEPENDS LIST_DIRECTORIES false
    "${PROJECT_SOURCE_DIR}/share/fusion/*")
  add_custom_command(OUTPUT "${FUSION_STAGE_DIR}/share/fusion/fusion-data.json"
    COMMAND "${CMAKE_COMMAND}" -D FUSION_INSTALL_STEP=stage
      -D "FUSION_HOME_SOURCE=${PROJECT_SOURCE_DIR}/share/fusion"
      -D "FUSION_HOME_STAGE=${FUSION_STAGE_DIR}/share/fusion"
      -D "FUSION_HOME_VERSION=${_fusion_home_version}" -D "FUSION_HOME_COMMIT=${_fusion_home_commit}"
      -P "${CMAKE_CURRENT_LIST_FILE}"
    DEPENDS ${_fusion_home_files} "${CMAKE_CURRENT_LIST_FILE}"
    COMMENT "Staging the home, stage/share/fusion, with its fusion-data.json"
    VERBATIM)
  add_custom_target(fusion_stage ALL DEPENDS "${FUSION_STAGE_DIR}/share/fusion/fusion-data.json")

  install(DIRECTORY "${FUSION_STAGE_DIR}/" DESTINATION "." USE_SOURCE_PERMISSIONS)

  # End-to-end tests of the staged tree (12's T1, label e2e), registered from backend/tests: the staged
  # home, a run on the shipped maps, and one fake home per failure of section 11's error table.
  function(fusion_add_home_tests)
    set(script "${CMAKE_CURRENT_FUNCTION_LIST_FILE}")
    set(common -D "FUSION_STAGE_DIR=${FUSION_STAGE_DIR}" -D "FUSION_EXE=$<TARGET_FILE:fusion>"
      -D "FUSION_SOURCE_DIR=${PROJECT_SOURCE_DIR}")
    set(names "")
    macro(fusion_home_test name step folder)
      add_test(NAME "home: ${name}"
        COMMAND "${CMAKE_COMMAND}" -D FUSION_INSTALL_STEP=${step} ${common} ${ARGN}
          -D "FUSION_TEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/${folder}" -P "${script}")
      list(APPEND names "home: ${name}")
    endmacro()
    fusion_home_test("fusion --version --json from the build tree finds and verifies the staged home"
      test_version home_version)
    fusion_home_test("a named batch with no start line runs with the shipped maps" test_batch home_batch)
    fusion_home_test("a home without fusion-data.json stops fusion before the batch (install.home_missing)"
      test_fake_home home_missing -D FUSION_FAKE=home_missing)
    fusion_home_test("a home of another version stops fusion (install.version_mismatch)"
      test_fake_home home_version_mismatch -D FUSION_FAKE=version_mismatch)
    fusion_home_test("a missing shipped map stops the run (install.file_missing)"
      test_fake_home home_file_missing -D FUSION_FAKE=file_missing)
    fusion_home_test("a damaged shipped map stops the run (install.file_damaged)"
      test_fake_home home_file_damaged -D FUSION_FAKE=file_damaged)
    set_tests_properties(${names} PROPERTIES LABELS "e2e;home" ENVIRONMENT_MODIFICATION "FUSION_HOME=unset:")
  endfunction()

elseif(FUSION_INSTALL_STEP STREQUAL "stage")
  # ---------------------------------------------------------------- build step: stage the home

  # The stage holds the source tree's share/fusion and nothing else, so a file removed there leaves it.
  file(REMOVE_RECURSE "${FUSION_HOME_STAGE}")
  file(COPY "${FUSION_HOME_SOURCE}/" DESTINATION "${FUSION_HOME_STAGE}")

  # Each data file's source, date and licence, from the table at the top of data/SOURCES.md (S8.31).
  set(rows "")
  if(EXISTS "${FUSION_HOME_STAGE}/data/SOURCES.md")
    file(STRINGS "${FUSION_HOME_STAGE}/data/SOURCES.md" rows REGEX "^\\| `[^`]+` \\| ")
  endif()
  foreach(row IN LISTS rows)
    if(row MATCHES
       "^\\| `([^`]+)` \\| ([^|]*[^| ]) +\\| ([0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]) +\\| ([^|]*[^| ]) +\\|")
      set("source/data/${CMAKE_MATCH_1}" "${CMAKE_MATCH_2}, ${CMAKE_MATCH_4}, see data/SOURCES.md")
      set("date/data/${CMAKE_MATCH_1}" "${CMAKE_MATCH_3}")
    endif()
  endforeach()

  # Every staged file with its bytes and SHA-256, which fusion --version --json checks (section 11).
  file(GLOB_RECURSE files LIST_DIRECTORIES false RELATIVE "${FUSION_HOME_STAGE}" "${FUSION_HOME_STAGE}/*")
  list(SORT files)
  set(entries "")
  foreach(rel IN LISTS files)
    file(SIZE "${FUSION_HOME_STAGE}/${rel}" bytes)
    file(SHA256 "${FUSION_HOME_STAGE}/${rel}" sha256)
    fusion_json_string(name "${rel}")
    set(entry "\"bytes\": ${bytes}, \"sha256\": \"${sha256}\"")
    if(rel MATCHES "^data/" AND NOT rel STREQUAL "data/SOURCES.md")
      if(NOT DEFINED "date/${rel}")
        message(FATAL_ERROR "data/SOURCES.md has no row for ${rel}: each shipped data file needs its source, date and licence (S8.31)")
      endif()
      fusion_json_string(source "${source/${rel}}")
      string(APPEND entry ", \"source\": ${source}, \"date\": \"${date/${rel}}\"")
    endif()
    list(APPEND entries "    ${name}: {${entry}}")
  endforeach()
  list(JOIN entries ",\n" body)
  fusion_json_string(version "${FUSION_HOME_VERSION}")
  fusion_json_string(commit "${FUSION_HOME_COMMIT}")
  file(WRITE "${FUSION_HOME_STAGE}/fusion-data.json"
    "{\n  \"fusion_version\": ${version},\n  \"git_commit\": ${commit},\n  \"files\": {\n${body}\n  },\n  \"tools\": {}\n}\n")

else()
  # ---------------------------------------------------------------- tests of the staged tree

  file(REAL_PATH "${FUSION_STAGE_DIR}/share/fusion" home)
  set(case "${FUSION_SOURCE_DIR}/backend/tests/reference/synthetic/toy-hsa13-wni-tsv-b10")

  # Fails unless the JSON value at the path given after the two arguments is the expected text
  # (string(JSON) gives booleans as ON and OFF).
  function(fusion_expect_json json expected)
    string(JSON got ERROR_VARIABLE problem GET "${json}" ${ARGN})
    if(problem)
      message(FATAL_ERROR "${ARGN}: ${problem}")
    elseif(NOT got STREQUAL expected)
      message(FATAL_ERROR "${ARGN}: expected '${expected}', got '${got}'")
    endif()
  endfunction()

  # Runs fusion in the test's folder with FUSION_HOME set to `fake` and the arguments after the three
  # given, with labels.txt as its stdin; fails unless it exits with `status` and, when `code` is not
  # empty, names that code on stderr.
  function(fusion_expect_exit fake status code)
    execute_process(COMMAND "${CMAKE_COMMAND}" -E env "FUSION_HOME=${fake}" "${FUSION_EXE}" ${ARGN}
      WORKING_DIRECTORY "${FUSION_TEST_DIR}" INPUT_FILE "${FUSION_TEST_DIR}/labels.txt"
      RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
    if(NOT rc EQUAL status)
      message(FATAL_ERROR "fusion ${ARGN} (FUSION_HOME '${fake}') exited with ${rc}, not ${status}:\n${err}")
    endif()
    string(FIND "${err}" "[${code}]" at)
    if(code AND at EQUAL -1)
      message(FATAL_ERROR "fusion ${ARGN} (FUSION_HOME '${fake}') did not report ${code}:\n${err}")
    endif()
  endfunction()

  file(REMOVE_RECURSE "${FUSION_TEST_DIR}")
  file(MAKE_DIRECTORY "${FUSION_TEST_DIR}")
  # The default pipeline's form (section 01): named arguments and no start line, so the maps are the
  # shipped ones (row 2B.4). FuSION 1's toy case toy-hsa13-wni-tsv-b10 gives the inputs and the frontiers.
  file(COPY "${case}/inputs/with_node_included.tsv" "${case}/inputs/report_example_with_pathways.xml"
    DESTINATION "${FUSION_TEST_DIR}")
  file(WRITE "${FUSION_TEST_DIR}/toy.batch"
    ";; FuSION 1's toy case toy-hsa13-wni-tsv-b10 as a named batch with no start line\n"
    "cudf values=with_node_included.tsv up=0.9 down=-0.9\n"
    "rgx file=report_example_with_pathways.xml split_threshold=1\n"
    "fb_rch source=hsa1 target=hsa22 bound=10 edges_to_target=any\n"
    "pathz3 fsc=hsa13 node_min=0 node_max=7 edge_min=0 edge_max=7 timeout=150s\n"
    "pex\n"
    "exit\n")
  file(WRITE "${FUSION_TEST_DIR}/labels.txt" "labels\n")

  if(FUSION_INSTALL_STEP STREQUAL "test_version")
    # The staged fusion, with FUSION_HOME unset, finds its home beside bin/ and checks every file
    # that fusion-data.json lists (S1.20; section 11's --version).
    file(READ "${home}/fusion-data.json" manifest)
    execute_process(COMMAND "${FUSION_EXE}" --version --json
      RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
    if(NOT rc EQUAL 0)
      message(FATAL_ERROR "fusion --version --json exited with ${rc}:\n${err}\n${out}")
    endif()
    fusion_expect_json("${out}" "${home}" install home)
    fusion_expect_json("${out}" "executable" install source)
    string(JSON problem ERROR_VARIABLE none GET "${out}" install error)
    if(NOT none)
      message(FATAL_ERROR "the staged home fails the startup check: ${problem}")
    endif()
    fusion_expect_json("${out}" "${home}/fusion-data.json" data manifest)
    fusion_expect_json("${out}" "ON" data ok)
    string(JSON want GET "${manifest}" fusion_version)
    fusion_expect_json("${out}" "${want}" versions fusion version)
    string(JSON want GET "${manifest}" git_commit)
    fusion_expect_json("${out}" "${want}" versions fusion git_commit)
    foreach(name hsa_to_gene_symbol_map.txt hsa_path_to_path_name_map.txt kegg_pathways.txt SOURCES.md)
      fusion_expect_json("${out}" "ON" data files "data/${name}" ok)
    endforeach()
    string(JSON count LENGTH "${manifest}" files)
    math(EXPR last "${count} - 1")
    foreach(i RANGE ${last})
      string(JSON rel MEMBER "${manifest}" files ${i})
      string(JSON sha256 GET "${manifest}" files "${rel}" sha256)
      fusion_expect_json("${out}" "ON" data files "${rel}" ok)
      fusion_expect_json("${out}" "${sha256}" data files "${rel}" sha256)
    endforeach()

    # A copy of the home with one data file changed: the check names that file and fails (exit 1).
    file(COPY "${home}/" DESTINATION "${FUSION_TEST_DIR}/home")
    file(APPEND "${FUSION_TEST_DIR}/home/data/kegg_pathways.txt" "changed\n")
    execute_process(COMMAND "${CMAKE_COMMAND}" -E env "FUSION_HOME=${FUSION_TEST_DIR}/home" "${FUSION_EXE}" --version --json
      RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
    if(NOT rc EQUAL 1)
      message(FATAL_ERROR "fusion --version --json on a damaged home exited with ${rc}, not 1:\n${err}")
    endif()
    fusion_expect_json("${out}" "${FUSION_TEST_DIR}/home" install home)
    fusion_expect_json("${out}" "OFF" data ok)
    fusion_expect_json("${out}" "OFF" data files "data/kegg_pathways.txt" ok)
    fusion_expect_json("${out}" "ON" data files "data/hsa_to_gene_symbol_map.txt" ok)

  elseif(FUSION_INSTALL_STEP STREQUAL "test_batch")
    execute_process(COMMAND "${FUSION_EXE}" -b toy.batch --run-dir run WORKING_DIRECTORY "${FUSION_TEST_DIR}"
      RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
    if(NOT rc EQUAL 0)
      message(FATAL_ERROR "fusion -b toy.batch exited with ${rc}:\n${err}")
    endif()
    set(run "${FUSION_TEST_DIR}/run")
    file(READ "${run}/run.json" record)
    fusion_expect_json("${record}" "${home}" install home)
    fusion_expect_json("${record}" "executable" install source)
    # Both frontiers equal FuSION 1's: the in case (FuSION 1's w, with hsa13) and the out case (wo).
    set(cases in out)
    set(fusion1_cases w wo)
    foreach(name f1 IN ZIP_LISTS cases fusion1_cases)
      execute_process(COMMAND "${CMAKE_COMMAND}" -E compare_files "${run}/pathz3/${name}.dat"
        "${case}/f1/run/hsa1_to_hsa22_${f1}_hsa13_with_node_included.tsv_b10.dat" RESULT_VARIABLE differ)
      if(NOT differ EQUAL 0)
        message(FATAL_ERROR "the ${name} case's frontier, run/pathz3/${name}.dat, differs from FuSION 1's")
      endif()
    endforeach()
    # The source gene is labelled with its symbol from the shipped map.
    file(STRINGS "${home}/data/hsa_to_gene_symbol_map.txt" line REGEX "^hsa1\t")
    string(REGEX REPLACE "^hsa1\t" "" symbol "${line}")
    file(READ "${run}/pathz3/results.json" results)
    string(JSON count LENGTH "${results}" nodes)
    set(label "")
    if(count GREATER 0)
      math(EXPR last "${count} - 1")
      foreach(i RANGE ${last})
        string(JSON key GET "${results}" nodes ${i} key)
        if(key STREQUAL "hsa1")
          string(JSON label GET "${results}" nodes ${i} label)
        endif()
      endforeach()
    endif()
    if(symbol STREQUAL "" OR NOT label STREQUAL symbol)
      message(FATAL_ERROR "hsa1 is labelled '${label}' in results.json; the shipped map names it '${symbol}'")
    endif()

  elseif(FUSION_INSTALL_STEP STREQUAL "test_fake_home")
    # Section 11's test hooks: FUSION_HOME names a fake home, one per failure, and fusion stops with
    # exit status 1 and the failure's code.
    set(fake "${FUSION_TEST_DIR}/fake")
    if(FUSION_FAKE STREQUAL "home_missing")
      # No fusion-data.json: the commands that read a batch stop before reading it, so no run starts and
      # a missing batch file is not the usage error, 2; --version reports the failure and exits 1; -h
      # and --commands need no home.
      file(MAKE_DIRECTORY "${fake}")
      fusion_expect_exit("${fake}" 1 install.home_missing -b toy.batch)
      fusion_expect_exit("${fake}" 1 install.home_missing -b never-read.batch)
      fusion_expect_exit("${fake}" 1 install.home_missing -b toy.batch --check)
      fusion_expect_exit("${fake}" 1 install.home_missing --query)
      fusion_expect_exit("${fake}" 0 "" -h)
      fusion_expect_exit("${fake}" 0 "" --commands)
      execute_process(COMMAND "${CMAKE_COMMAND}" -E env "FUSION_HOME=${fake}" "${FUSION_EXE}" --version --json
        RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
      if(NOT rc EQUAL 1)
        message(FATAL_ERROR "fusion --version --json with no fusion-data.json exited with ${rc}, not 1:\n${err}")
      endif()
      fusion_expect_json("${out}" "install.home_missing" install error code)
      string(JSON message GET "${out}" install error message)
      string(FIND "${message}" "${fake}" at)
      if(at EQUAL -1)
        message(FATAL_ERROR "the message does not name the home it tried: ${message}")
      endif()
      file(GLOB runs LIST_DIRECTORIES true "${FUSION_TEST_DIR}/toy_*")
      if(runs)
        message(FATAL_ERROR "a run started without a home: ${runs}")
      endif()
    else()
      file(COPY "${home}/" DESTINATION "${fake}")
      if(FUSION_FAKE STREQUAL "version_mismatch")
        file(READ "${fake}/fusion-data.json" manifest)
        string(REGEX REPLACE "\"fusion_version\": \"[^\"]*\"" "\"fusion_version\": \"0.0.0-other\"" manifest "${manifest}")
        file(WRITE "${fake}/fusion-data.json" "${manifest}")
        set(code install.version_mismatch)
      elseif(FUSION_FAKE STREQUAL "file_missing")
        file(REMOVE "${fake}/data/hsa_path_to_path_name_map.txt")
        set(code install.file_missing)
      elseif(FUSION_FAKE STREQUAL "file_damaged")
        file(APPEND "${fake}/data/hsa_to_gene_symbol_map.txt" "hsa0\tchanged\n")
        set(code install.file_damaged)
      else()
        message(FATAL_ERROR "FUSION_FAKE is ${FUSION_FAKE}")
      endif()
      fusion_expect_exit("${fake}" 1 ${code} -b toy.batch)
    endif()

  else()
    message(FATAL_ERROR "FUSION_INSTALL_STEP is ${FUSION_INSTALL_STEP}")
  endif()
endif()
