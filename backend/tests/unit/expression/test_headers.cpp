#include <catch2/catch_test_macros.hpp>

#include "fusion/expression/expression.hpp"

TEST_CASE("expression's header compiles", "[unit][expression]") {
  fusion::expression::ValueInput a, b;
  CHECK(a == b);
}
