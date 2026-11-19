#include <catch2/catch_test_macros.hpp>

#include "fusion/network/network.hpp"

TEST_CASE("network's header compiles, and Bitset works", "[unit][network]") {
  fusion::network::Bitset b(70);
  b.set(3);
  b.set(69);
  CHECK(b.test(3));
  CHECK(b.test(69));
  CHECK_FALSE(b.test(4));
}
