// results: the page assets compiled into libfusion (07.C3, 11.C11; section 12's results row) and the
// site key's check (S6.19, section 07's site rules).
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "fixture.hpp"
#include "fusion/results/assets.hpp"
#include "fusion/results/site.hpp"

using namespace fusion;
using namespace fusion::results;
using Catch::Matchers::ContainsSubstring;

TEST_CASE("the compiled-in page assets match their pinned SHA-256 (07.C3, 11.C11)", "[unit][results]") {
  const auto assets = page_assets();
  REQUIRE(assets.size() == 4);
  for (const PageAsset& a : assets) {
    INFO(a.name);
    CHECK_FALSE(a.bytes.empty());
    CHECK(core::Sha256::of(a.bytes).hex() == a.sha256);   // the bytes are the ones the build hashed
  }
  // vis-network 10.1.5's standalone build, at the SHA-256 module.cmake pins (653,713 bytes).
  const PageAsset* vis = find_page_asset("vis-network.min.js");
  REQUIRE(vis != nullptr);
  CHECK(vis->sha256 == "b427322ad953d70ab75d1b798ebb8349863502f244e02de3e12a4ab7dfeddf2c");
  CHECK(vis->bytes.size() == 653713);
  CHECK_THAT(std::string(vis->bytes.substr(0, 200)), ContainsSubstring("@version " + std::string(vis_network_version)));
  CHECK_THAT(std::string(find_page_asset("LICENSES.txt")->bytes), ContainsSubstring("vis-network 10.1.5"));
  CHECK_THAT(std::string(find_page_asset("fusion-explain.js")->bytes), ContainsSubstring("fusion-view"));
  CHECK(find_page_asset("vis.js") == nullptr);
}

TEST_CASE("site keys are checked before the run (S6.19)", "[unit][results]") {
  for (const char* good : {"jun_b10", "r7.epha2-curated", "A", "0", "conference"}) CHECK_NOTHROW(validate_site_key(good));
  for (const char* bad : {"CON", "con", "COM0", "lpt9", "nul.txt", "Aux.html", "a/b", "trailing.", "", "_x", ".x",
                          "with space"}) {
    INFO(bad);
    CHECK_THROWS_AS(validate_site_key(bad), core::Failure);
  }
  CHECK_THROWS_WITH(validate_site_key("CON"), ContainsSubstring("run_key 'CON' is a reserved file name on Windows"));
  CHECK_THROWS_WITH(validate_site_key(std::string(65, 'a')), ContainsSubstring("must be 1 to 64 characters long"));
  try {
    validate_site_key("a/b");
  } catch (const core::Failure& f) {
    CHECK(f.code() == "batch.bad_value");
  }
}
