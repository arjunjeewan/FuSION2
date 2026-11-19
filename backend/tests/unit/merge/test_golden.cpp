// The golden base merge (section 03, Test hooks; 03.C8): the digest of a fixture's base merge is pinned.
// When it changes, the merge's or the reader's rules changed: bump rules_version, so the shared store can
// never serve a merge made under the old rules, and pin the new digest with it.
#include <catch2/catch_test_macros.hpp>

#include "support.hpp"

using namespace merge_test;

TEST_CASE("the golden base merge's digest goes with rules_version", "[unit][merge][03.C8][golden]") {
    Context c;
    const std::vector<DocPtr> docs = {
        read(c, Kgml("path:hsa04010", "MAPK signaling pathway")
                    .entry(1, "hsa:2353 hsa:3725")
                    .entry(2, "hsa:5599 hsa:5601 hsa:5602")
                    .entry(3, "hsa:1")
                    .entry(4, "hsa:4149")
                    .entry(5, "hsa:4609")
                    .group(6, {4, 5})
                    .entry(7, "cpd:C00076", "compound")
                    .rel(2, 1, {"phosphorylation"}, "PPrel", "120")
                    .rel(1, 3, {"expression"}, "GErel")
                    .rel(6, 3, {"binding/association"}, "PPrel", "33.5")
                    .rel(7, 4, {"compound"}, "ECrel")
                    .rel(3, 4, {"indirect effect"})),
        read(c, Kgml("path:hsa05161").entry(1, "hsa:3725 hsa:5599 hsa:5601 hsa:5602").entry(2, "hsa:7").rel(1, 2, {"activation"}, "PPrel", "7")),
        read(c, Kgml("path:hsa05224").entry(1, "hsa:2353 hsa:3725 hsa:6667").entry(2, "hsa:1").rel(1, 2, {"expression"}, "GErel"))};
    const auto base = fusion::merge::build_base_merge(docs, fusion::network::BuildSettings{}, fusion::merge::MergeKey{}, c.ctx);
    STATIC_REQUIRE(fusion::merge::rules_version == 1);
    CHECK(fusion::core::Sha256::of(dump(*base->network())).hex() ==
          "9a222c846489a186befba9609b07e26db5f02a31c9ea672c7325b440e8c930cc");
}
