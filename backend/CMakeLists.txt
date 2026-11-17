# The backend: one static library per module (11.C6), joined in fusion::libfusion, and the fusion executable.
# Each module's public headers are in modules/<name>/include/fusion/<name>/, its sources in modules/<name>/src/.
# The links below are section 11's targets table; an include that breaks the layering fails to compile.

function(fusion_add_module name)
  cmake_parse_arguments(M "" "" "PUBLIC_LINKS;PRIVATE_LINKS" ${ARGN})
  set(dir "${CMAKE_CURRENT_SOURCE_DIR}/modules/${name}")
  file(GLOB_RECURSE sources CONFIGURE_DEPENDS "${dir}/src/*.cpp")
  add_library(fusion_${name} STATIC ${sources})
  add_library(fusion::${name} ALIAS fusion_${name})
  target_include_directories(fusion_${name} PUBLIC "${dir}/include" PRIVATE "${dir}/src")
  target_link_libraries(fusion_${name} PUBLIC fusion_flags ${M_PUBLIC_LINKS} PRIVATE ${M_PRIVATE_LINKS})
  # A module's own build steps (generated sources, compiled-in assets) live in its module.cmake,
  # so modules never edit this file. It sees the target as fusion_${name} and its folder as ${dir}.
  if(EXISTS "${dir}/module.cmake")
    include("${dir}/module.cmake")
  endif()
endfunction()

fusion_add_module(core       PUBLIC_LINKS nlohmann_json::nlohmann_json PRIVATE_LINKS FastFloat::fast_float)
# core's own interface passes JSON (events, journals, check reports), so it links nlohmann_json publicly.
fusion_add_module(network    PUBLIC_LINKS fusion::core PRIVATE_LINKS pugixml::pugixml)
fusion_add_module(expression PUBLIC_LINKS fusion::core fusion::network)
fusion_add_module(merge      PUBLIC_LINKS fusion::network fusion::expression)
fusion_add_module(corridor   PUBLIC_LINKS fusion::network fusion::expression)
fusion_add_module(solve      PUBLIC_LINKS fusion::corridor PRIVATE_LINKS z3::libz3)
fusion_add_module(results    PUBLIC_LINKS fusion::solve PRIVATE_LINKS nlohmann_json::nlohmann_json)
fusion_add_module(enrichment PUBLIC_LINKS fusion::network fusion::expression)
fusion_add_module(run        PUBLIC_LINKS fusion::core PRIVATE_LINKS nlohmann_json::nlohmann_json)
fusion_add_module(batch      PUBLIC_LINKS fusion::network fusion::merge fusion::expression fusion::corridor
                                          fusion::solve fusion::results fusion::enrichment fusion::run
                             PRIVATE_LINKS nlohmann_json::nlohmann_json)

add_library(fusion_libfusion INTERFACE)
target_link_libraries(fusion_libfusion INTERFACE fusion::core fusion::network fusion::expression fusion::merge
  fusion::corridor fusion::solve fusion::results fusion::enrichment fusion::run fusion::batch)
add_library(fusion::libfusion ALIAS fusion_libfusion)

add_executable(fusion cli/main.cpp)
target_link_libraries(fusion PRIVATE fusion::libfusion)

if(FUSION_BUILD_TESTS)
  add_subdirectory(tests)
endif()
