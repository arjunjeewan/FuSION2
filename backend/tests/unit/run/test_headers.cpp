#include <catch2/catch_test_macros.hpp>

#include "fusion/run/run.hpp"

TEST_CASE("run's header compiles", "[unit][run]") {
  fusion::run::StepSpec spec;
  CHECK(spec.line == 0);
}
