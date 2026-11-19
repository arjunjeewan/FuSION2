// The ranking (S7.04, S7.05; 08.C2, 08.C10): values descending, ties by gene id in byte order,
// read through section 04's one reader.
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "support.hpp"

namespace {

using namespace enrichment_test;

std::vector<std::string> names(const enrichment::Ranking& r, const core::Interner& ids) {
    std::vector<std::string> out;
    for (const core::GeneId g : r.id) out.emplace_back(ids.str(g));
    return out;
}

}  // namespace

TEST_CASE("enrichment: tied values are ordered by gene id in byte order", "[unit][enrichment][S7.05][08.C2]") {
    Harness h;
    core::Interner ids;
    // hsa1000 < hsa10000 < hsa999 in byte order; numeric order would put hsa999 first. FuSION 1
    // left ties in whatever order numpy's quicksort gave (gseapy gsea.py:493).
    const auto values = read_values("hsa999\t1.5\nhsa10000\t1.5\nhsa5\t2\nhsa1000\t1.5\nhsa8\t-3\nhsa7\t0.00\nhsa6\t-0.00\n",
                                    ids, h);
    const enrichment::Ranking r = enrichment::rank(values, ids);
    CHECK(names(r, ids) == std::vector<std::string>{"hsa5", "hsa1000", "hsa10000", "hsa999", "hsa6", "hsa7", "hsa8"});
    CHECK(r.value == std::vector<double>{2.0, 1.5, 1.5, 1.5, -0.0, 0.0, -3.0});
    // Weights are |value| (S7.10: weight 1), so -0.00 weighs +0.
    CHECK(r.weight == std::vector<double>{2.0, 1.5, 1.5, 1.5, 0.0, 0.0, 3.0});
    CHECK_FALSE(std::signbit(r.weight[4]));
    // 0.00 and -0.00 are one value, as pandas counts them (08.C10's count: N - distinct).
    CHECK(r.distinct_values == 4);
}

TEST_CASE("enrichment: the value file's line order does not change the ranking", "[unit][enrichment][S7.05]") {
    const std::string text = read_file(gsea_fixtures() / "n500" / "values.tsv");
    std::vector<std::string> rows = lines(text);
    std::reverse(rows.begin(), rows.end());
    std::string reversed;
    for (const std::string& row : rows) reversed += row + "\n";

    Harness h;
    core::Interner a, b;
    const enrichment::Ranking ra = enrichment::rank(read_values(text, a, h), a);
    const enrichment::Ranking rb = enrichment::rank(read_values(reversed, b, h), b);
    CHECK(names(ra, a) == names(rb, b));
    CHECK(ra.value == rb.value);
    CHECK(ra.distinct_values == rb.distinct_values);
}
