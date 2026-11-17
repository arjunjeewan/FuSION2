# Compiler flags every FuSION target uses (11.C7): C++20, warnings, and floating point that
# gives the same bytes on every platform (09.C16: no contraction, no fast math).
add_library(fusion_flags INTERFACE)
target_compile_features(fusion_flags INTERFACE cxx_std_20)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(fusion_flags INTERFACE
    -Wall -Wextra -Wpedantic -Wshadow -Wnon-virtual-dtor -Wold-style-cast
    -ffp-contract=off -fno-fast-math)
  if(FUSION_WARNINGS_AS_ERRORS)
    target_compile_options(fusion_flags INTERFACE -Werror)
  endif()
elseif(MSVC)
  target_compile_options(fusion_flags INTERFACE /W4 /permissive- /fp:precise /utf-8)
  if(FUSION_WARNINGS_AS_ERRORS)
    target_compile_options(fusion_flags INTERFACE /WX)
  endif()
endif()
