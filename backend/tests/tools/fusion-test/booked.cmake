# The guard of CTest's "ref: t2" (prompt log entry 379): T2 loads 8 cores for about 15 minutes and is booked
# with the user (12.C1), so it runs only when FUSION_T2_BOOKED=1, which the t2 test preset sets. Otherwise
# it prints why and exits 4, which CTest reports as skipped. fusion-test ref --tier t2 itself is unguarded.
#   cmake -P booked.cmake -- <program> <argument>...
set(command "")
set(after_dashes FALSE)
math(EXPR last "${CMAKE_ARGC} - 1")
foreach(i RANGE ${last})
  if(after_dashes)
    string(REPLACE ";" "\\;" argument "${CMAKE_ARGV${i}}")   # an argument stays one, semicolons and all
    list(APPEND command "${argument}")
  elseif(CMAKE_ARGV${i} STREQUAL "--")
    set(after_dashes TRUE)
  endif()
endforeach()
if(NOT command)
  message(FATAL_ERROR "booked.cmake: no command after --")
endif()

if(NOT "$ENV{FUSION_T2_BOOKED}" STREQUAL "1")
  message("ref: t2 is skipped: T2 is booked with the user (12.C1); the t2 test preset sets FUSION_T2_BOOKED=1")
  set(code 4)
else()
  execute_process(COMMAND ${command} RESULT_VARIABLE code)
  if(NOT code MATCHES "^[0-9]+$")   # the program did not start
    message("booked.cmake: ${code}")
    set(code 2)
  endif()
endif()

if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.29)
  cmake_language(EXIT ${code})
elseif(NOT code EQUAL 0)            # before 3.29 a script can only fail; the test's skip pattern still applies
  message(FATAL_ERROR "exit ${code}")
endif()
