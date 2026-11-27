# fusion-test, the driver of section 12's tests (12.C2, 12.C8; the C3 note). It links libfusion and the kit.
# fusion_test_driver holds all but main.cpp, so test_fusion_test (unit/fusion-test/) can test it in process.
add_library(fusion_test_driver STATIC options.cpp data.cpp e2e/case.cpp e2e/engine.cpp)
target_include_directories(fusion_test_driver PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(fusion_test_driver PUBLIC fusion::testkit)
# --fusion defaults to the build's staged fusion, which finds its home beside it (S1.20).
target_compile_definitions(fusion_test_driver PRIVATE FUSION_EXE="$<TARGET_FILE:fusion>")

add_executable(fusion-test main.cpp)
target_link_libraries(fusion-test PRIVATE fusion_test_driver)
add_dependencies(fusion-test fusion fusion_stage)

# CTest registration (C3.1): `e2e: <case>`, one test per e2e/cases/*/case.json, running fusion-test e2e --case
# <case>, labelled e2e and the case's tags; fusion-test's exit code 4, every case skipped, is CTest's skip.
# Each test unsets 11.C20's overrides, so the staged fusion finds its own home and tools unless the case sets
# them. An edited case.json configures again, so the labels follow its tags. The ref: tests come with C4.
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
