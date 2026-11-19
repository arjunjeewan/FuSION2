// Compiles the finished public headers together, so a header that does not compile fails the build.
#include <catch2/catch_test_macros.hpp>

#include "fusion/core/core.hpp"

TEST_CASE("core's header compiles", "[unit][core]") {
  fusion::core::GeneId a{1}, b{2};
  CHECK(a < b);
}
