# The backend's tests (section 12): one Catch2 executable per module, test_<module>, from unit/<module>/.
# Tags become CTest labels, so `ctest -L unit` selects a tier.
include(Catch)

# The test kit (section 12, "The test kit"; the C2 note). fusion_testkit_base holds the kit's own data
# model, the FuSION 1 readers, the checker and the oracles, and links only core, pugixml and nlohmann/json,
# so an oracle that includes network, merge, corridor or solve does not compile (C2.2; 12.C14).
# fusion_testkit adds the adapters from module types, ScriptedOracle and the recording StepContext.
set(kit "${CMAKE_CURRENT_SOURCE_DIR}/testkit")
add_library(fusion_testkit_base STATIC
  "${kit}/src/util.cpp" "${kit}/src/canon.cpp" "${kit}/src/data.cpp" "${kit}/src/scratch.cpp"
  "${kit}/src/generate.cpp" "${kit}/src/fusion1.cpp" "${kit}/src/check.cpp" "${kit}/src/oracles.cpp"
  "${kit}/src/merge_oracle.cpp" "${kit}/src/e2e.cpp" "${kit}/src/run_facts.cpp")
add_library(fusion::testkit_base ALIAS fusion_testkit_base)
target_include_directories(fusion_testkit_base PUBLIC "${kit}/include" PRIVATE "${kit}/src")
target_link_libraries(fusion_testkit_base PUBLIC fusion::core PRIVATE pugixml::pugixml nlohmann_json::nlohmann_json)
target_compile_definitions(fusion_testkit_base PRIVATE FUSION_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

add_library(fusion_testkit STATIC "${kit}/src/adapt.cpp" "${kit}/src/scripted_oracle.cpp" "${kit}/src/step_context.cpp")
add_library(fusion::testkit ALIAS fusion_testkit)
target_include_directories(fusion_testkit PRIVATE "${kit}/src")
target_link_libraries(fusion_testkit PUBLIC fusion_testkit_base fusion::libfusion)

# The suites that use the kit (C2.1), and the kit's own self-tests (test_testkit).
set(kit_users merge corridor solve testkit)
foreach(module core network expression merge corridor solve results enrichment run batch testkit)
  file(GLOB_RECURSE sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/unit/${module}/*.cpp")
  if(sources)
    add_executable(test_${module} ${sources})
    if(module STREQUAL "testkit")
      target_link_libraries(test_${module} PRIVATE Catch2::Catch2WithMain)
      # The kit's self-tests run fusion itself on a toy batch (run_fusion, read_run, compare_tree).
      target_compile_definitions(test_${module} PRIVATE FUSION_EXE="$<TARGET_FILE:fusion>")
      add_dependencies(test_${module} fusion fusion_stage)
    else()
      target_link_libraries(test_${module} PRIVATE fusion::${module} Catch2::Catch2WithMain)
    endif()
    if(module IN_LIST kit_users)
      target_link_libraries(test_${module} PRIVATE fusion::testkit)
    endif()
    target_compile_definitions(test_${module} PRIVATE
      FUSION_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    catch_discover_tests(test_${module} ADD_TAGS_AS_LABELS DISCOVERY_MODE PRE_TEST)
  endif()
endforeach()
fusion_add_home_tests()   # the staged home and the shipped maps, end to end (cmake/FusionInstall.cmake)

# The driver, fusion-test, with the end-to-end cases' CTest tests (tools/fusion-test/; the C3 note), and its
# own tests, test_fusion_test: options, exit codes, data on a toy root and the engine on toy cases.
add_subdirectory(tools/fusion-test)
file(GLOB_RECURSE sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/unit/fusion-test/*.cpp")
add_executable(test_fusion_test ${sources})
target_link_libraries(test_fusion_test PRIVATE fusion_test_driver Catch2::Catch2WithMain)
target_compile_definitions(test_fusion_test PRIVATE FUSION_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
  FUSION_EXE="$<TARGET_FILE:fusion>" FUSION_TEST_EXE="$<TARGET_FILE:fusion-test>")
add_dependencies(test_fusion_test fusion fusion_stage fusion-test)
catch_discover_tests(test_fusion_test ADD_TAGS_AS_LABELS DISCOVERY_MODE PRE_TEST)
