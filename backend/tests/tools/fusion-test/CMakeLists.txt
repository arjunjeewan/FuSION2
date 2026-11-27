# fusion-test, the driver of section 12's tests (12.C2, 12.C8; the C3 note). It links libfusion and the kit.
# fusion_test_driver holds all but main.cpp, so test_fusion_test (unit/fusion-test/) can test it in process.
add_library(fusion_test_driver STATIC options.cpp data.cpp e2e/case.cpp e2e/engine.cpp
  ref/case.cpp ref/expect.cpp ref/facts.cpp ref/chained.cpp ref/pinned.cpp ref/verify.cpp ref/rules.cpp
  ref/report.cpp ref/run.cpp)
target_include_directories(fusion_test_driver PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(fusion_test_driver PUBLIC fusion::testkit PRIVATE pugixml::pugixml)
# --fusion defaults to the build's staged fusion, which finds its home beside it (S1.20).
target_compile_definitions(fusion_test_driver PRIVATE FUSION_EXE="$<TARGET_FILE:fusion>")

add_executable(fusion-test main.cpp)
target_link_libraries(fusion-test PRIVATE fusion_test_driver)
add_dependencies(fusion-test fusion fusion_stage)

# CTest registration (C3.1): `e2e: <case>`, one test per e2e/cases/*/case.json, running fusion-test e2e --case
# <case>, labelled e2e and the case's tags; fusion-test's exit code 4, every case skipped, is CTest's skip.
# Each test unsets 11.C20's overrides, so the staged fusion finds its own home and tools unless the case sets
# them. An edited case.json configures again, so the labels follow its tags.
cmake_path(SET e2e_cases NORMALIZE "${CMAKE_CURRENT_SOURCE_DIR}/../../e2e/cases")
file(GLOB e2e_case_files CONFIGURE_DEPENDS LIST_DIRECTORIES false "${e2e_cases}/*/case.json")
foreach(case_file IN LISTS e2e_case_files)
  cmake_path(GET case_file PARENT_PATH case_dir)
  cmake_path(GET case_dir FILENAME case_id)
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${case_file}")
  file(READ "${case_file}" case_json)
  set(labels e2e)
  # A case.json that is not JSON gets the label e2e alone; its test then fails with exit code 2.
  string(JSON tags_type ERROR_VARIABLE no_tags TYPE "${case_json}" tags)
  if(NOT no_tags AND tags_type STREQUAL "ARRAY")
    string(JSON count LENGTH "${case_json}" tags)
    if(count GREATER 0)
      math(EXPR last "${count} - 1")
      foreach(i RANGE ${last})
        string(JSON tag GET "${case_json}" tags ${i})
        list(APPEND labels "${tag}")
      endforeach()
    endif()
  endif()
  list(REMOVE_DUPLICATES labels)
  add_test(NAME "e2e: ${case_id}" COMMAND fusion-test e2e --case "${case_id}")
  set_tests_properties("e2e: ${case_id}" PROPERTIES LABELS "${labels}" SKIP_RETURN_CODE 4
    ENVIRONMENT_MODIFICATION "FUSION_HOME=unset:;FUSION_GNUPLOT=unset:;FUSION_DOT=unset:")
endforeach()

# The reference runner's CTest tests (C3.1, C4): "ref: <id>" for each T1 case of cases.json, run with its default
# modes into a report folder of its own in the build tree, labelled ref and t1; and "ref: t2", the whole fast
# tier with one report on 8 workers, labelled ref, t2 and kegg, which the dev test preset leaves out by its t2
# label. T2 is booked with the user (12.C1), so booked.cmake runs it only when FUSION_T2_BOOKED=1, which the
# t2 test preset sets; otherwise it exits 4, skipped (prompt log entry 379). T3 is not registered: it runs
# only as a booked fusion-test ref --tier t3.
cmake_path(SET ref_cases NORMALIZE "${CMAKE_CURRENT_SOURCE_DIR}/../../reference/cases.json")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${ref_cases}")
file(READ "${ref_cases}" ref_json)
string(JSON ref_count LENGTH "${ref_json}" cases)
math(EXPR ref_last "${ref_count} - 1")
foreach(i RANGE ${ref_last})
  string(JSON ref_tier GET "${ref_json}" cases ${i} tier)
  if(NOT ref_tier STREQUAL "T1")
    continue()
  endif()
  string(JSON ref_id GET "${ref_json}" cases ${i} id)
  add_test(NAME "ref: ${ref_id}"
           COMMAND fusion-test ref --case "${ref_id}" --out "${CMAKE_CURRENT_BINARY_DIR}/ref-reports/${ref_id}")
  set_tests_properties("ref: ${ref_id}" PROPERTIES LABELS "ref;t1" SKIP_RETURN_CODE 4
    ENVIRONMENT_MODIFICATION "FUSION_HOME=unset:;FUSION_GNUPLOT=unset:;FUSION_DOT=unset:")
endforeach()
add_test(NAME "ref: t2" COMMAND "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_SOURCE_DIR}/booked.cmake" --
         "$<TARGET_FILE:fusion-test>" ref --tier t2 --workers 8)
set_tests_properties("ref: t2" PROPERTIES LABELS "ref;t2;kegg" SKIP_RETURN_CODE 4
  SKIP_REGULAR_EXPRESSION "T2 is booked with the user"
  ENVIRONMENT_MODIFICATION "FUSION_HOME=unset:;FUSION_GNUPLOT=unset:;FUSION_DOT=unset:")
