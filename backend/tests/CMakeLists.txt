# The backend's tests (section 12): one Catch2 executable per module, test_<module>, from unit/<module>/.
# Tags become CTest labels, so `ctest -L unit` selects a tier.
include(Catch)
foreach(module core network expression merge corridor solve results enrichment run batch)
  file(GLOB_RECURSE sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/unit/${module}/*.cpp")
  if(sources)
    add_executable(test_${module} ${sources})
    target_link_libraries(test_${module} PRIVATE fusion::${module} Catch2::Catch2WithMain)
    target_compile_definitions(test_${module} PRIVATE
      FUSION_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    catch_discover_tests(test_${module} ADD_TAGS_AS_LABELS DISCOVERY_MODE PRE_TEST)
  endif()
endforeach()
