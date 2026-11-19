// GSEA agreement with gseapy 1.3.1 (section 12, "GSEA agreement with gseapy (S7.11)"; 08.C1):
// on W5's synthetic fixtures (backend/tests/fixtures/gsea/, read only), every tested set's ES,
// NES, nominal p, FDR q and FWER p must equal gseapy's as doubles, and Tag %, Gene % and
// Lead_genes as text, at every seed. The values go through section 04's reader and rank()
// (S7.04, S7.05, 08.C2), and the comparison reads 2.0's own table (S7.16).
#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "../../../modules/enrichment/src/detail.hpp"
#include "support.hpp"

namespace {

using namespace enrichment_test;

struct Observed {
    std::string es, tag, gene, lead;
};
struct Stats {
    std::string nes, nom_p, fdr_q, fwer_p;
};

std::map<std::string, Observed> read_observed(const std::string& fixture) {
    std::map<std::string, Observed> out;
    const auto rows = lines(read_file(gsea_fixtures() / fixture / "observed.tsv"));
    REQUIRE(rows.at(0) == "Term\tES\tTag %\tGene %\tLead_genes");
    for (std::size_t i = 1; i < rows.size(); ++i) {
        const auto c = split(rows[i], '\t');
        out[c.at(0)] = Observed{c.at(1), c.at(2), c.at(3), c.at(4)};
    }
    return out;
}

std::map<std::pair<std::uint64_t, std::string>, Stats> read_stats(const std::string& fixture) {
    std::map<std::pair<std::uint64_t, std::string>, Stats> out;
    const auto rows = lines(read_file(gsea_fixtures() / fixture / "stats.tsv"));
    REQUIRE(rows.at(0) == "seed\tTerm\tNES\tNOM p-val\tFDR q-val\tFWER p-val");
    for (std::size_t i = 1; i < rows.size(); ++i) {
        const auto c = split(rows[i], '\t');
        out[{std::stoull(c.at(0)), c.at(1)}] = Stats{c.at(2), c.at(3), c.at(4), c.at(5)};
    }
    return out;
}

// The table 2.0 writes for one seed, by Term: its cells in column order.
std::map<std::string, std::vector<std::string>> table_for(const Fixture& f, std::uint64_t seed,
                                                          enrichment::Prerank& pr) {
    enrichment::Shortlist sl;
    sl.candidates = f.candidates;
    sl.ranking = f.ranking;
    sl.prerank = pr;
    sl.selection = enrichment::select(pr, sl.candidates, enrichment::Selector{}, {}, f.ids);
    sl.settings = enrichment::Settings{seed, 1000};
    std::ostringstream out;
    enrichment::write_table(out, sl, f.ids);
    std::map<std::string, std::vector<std::string>> table;
    const auto rows = lines(out.str());
    for (std::size_t i = 1; i < rows.size(); ++i) {
        auto cells = split(rows[i], '\t');
        table[cells.at(0)] = std::move(cells);
    }
    return table;
}

void check_fixture(const std::string& fixture, const std::vector<std::uint64_t>& seeds) {
    Harness h;
    const auto f = load_fixture(fixture, h);
    const auto observed = read_observed(fixture);
    const auto stats = read_stats(fixture);
    for (const std::uint64_t seed : seeds) {
        enrichment::Prerank pr = enrichment::prerank(f->ranking, f->candidates, enrichment::Settings{seed, 1000}, {});
        CHECK(pr.zero_weight_draws == 0);   // the fixtures keep 08.C7's case out (their README)
        const auto table = table_for(*f, seed, pr);
        std::set<std::string> scored;
        for (const enrichment::Row& row : pr.rows) {
            const std::string& term = f->candidates[row.candidate].id;
            INFO(fixture << ", seed " << seed << ", set " << term);
            if (!row.score) {
                CHECK(observed.count(term) == 0);   // gseapy did not test it either (S7.09)
                continue;
            }
            scored.insert(term);
            REQUIRE(observed.count(term) == 1);
            const Observed& o = observed.at(term);
            const Stats& s = stats.at({seed, term});
            const enrichment::Score& score = *row.score;
            // As doubles (08.C1).
            CHECK(score.es == number(o.es));
            CHECK(score.nes == number(s.nes));
            CHECK(score.nom_p == number(s.nom_p));
            CHECK(score.fdr_q == number(s.fdr_q));
            CHECK(score.fwer_p == number(s.fwer_p));
            // As 2.0's table writes them: the same text as gseapy's repr (1.20).
            const auto& cells = table.at(term);
            REQUIRE(cells.size() == 15);
            CHECK(cells[4] == o.es);
            CHECK(cells[5] == s.nes);
            CHECK(cells[6] == s.nom_p);
            CHECK(cells[7] == s.fdr_q);
            CHECK(cells[8] == s.fwer_p);
            CHECK(cells[9] == o.tag);
            CHECK(cells[10] == o.gene);
            CHECK(cells[11] == o.lead);
        }
        CHECK(scored.size() == observed.size());
    }
}

const std::vector<std::uint64_t> all_seeds = {42, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20};

}  // namespace

TEST_CASE("enrichment: n50 matches gseapy 1.3.1 at every seed", "[unit][enrichment][S7.10][S7.11][S7.12]") {
    check_fixture("n50", all_seeds);
}

TEST_CASE("enrichment: n500 matches gseapy 1.3.1 at every seed", "[unit][enrichment][S7.10][S7.11][S7.12]") {
    check_fixture("n500", all_seeds);
}

TEST_CASE("enrichment: n3054 matches gseapy 1.3.1 at seed 42", "[unit][enrichment][S7.10][S7.11][S7.12]") {
    check_fixture("n3054", {42});
}

// Seeds 1 to 20 all pass on n3054 as well, but take about 9 s in a debug build; the unit tier
// keeps three of them, and n50 and n500 cover every seed.
TEST_CASE("enrichment: n3054 matches gseapy 1.3.1 at seeds 1, 10 and 20", "[unit][enrichment][S7.11]") {
    check_fixture("n3054", {1, 10, 20});
}

TEST_CASE("enrichment: the fixtures' ties, untested sets and twins", "[unit][enrichment][S7.05][S7.09][08.C9][08.C10]") {
    struct Expected {
        std::string fixture;
        std::uint32_t genes, repeats;
        std::string share;
        std::vector<std::pair<std::string, std::uint32_t>> untested;   // list order, with m
        bool twins;
    };
    const std::vector<Expected> cases = {
        {"n50", 50, 32, "64.00%", {{"s1", 14}, {"s8", 50}}, false},
        {"n500", 500, 345, "69.00%", {{"s1", 14}}, true},
        {"n3054", 3054, 2864, "93.78%", {{"s1", 14}, {"s31", 501}}, true},
    };
    for (const Expected& e : cases) {
        INFO(e.fixture);
        Harness h;
        const auto f = load_fixture(e.fixture, h);
        // gseapy's count of repeated values (gsea.py:516), with "0.00" and "-0.00" one value.
        REQUIRE(f->ranking.id.size() == e.genes);
        CHECK(e.genes - f->ranking.distinct_values == e.repeats);
        CHECK(fusion::enrichment::detail::percent(static_cast<double>(e.repeats) / e.genes) == e.share);
        const enrichment::Prerank pr = enrichment::prerank(f->ranking, f->candidates, enrichment::Settings{42, 50}, {});
        // Every candidate has a row; the untested follow the tested, in list order (S7.09).
        REQUIRE(pr.rows.size() == f->candidates.size());
        CHECK(pr.tested == f->candidates.size() - e.untested.size());
        for (std::size_t i = 0; i < e.untested.size(); ++i) {
            const enrichment::Row& row = pr.rows[pr.tested + i];
            CHECK(f->candidates[row.candidate].id == e.untested[i].first);
            CHECK(row.ranked_genes == e.untested[i].second);
            CHECK_FALSE(row.score.has_value());
        }
        if (!e.twins) continue;
        // s9 and s10 have the same ranked genes, so the shared permutations give them equal
        // scores; 08.C9 lists s10 first, by byte order.
        std::size_t s9 = 0, s10 = 0;
        for (std::size_t i = 0; i < pr.tested; ++i) {
            if (f->candidates[pr.rows[i].candidate].id == "s9") s9 = i;
            if (f->candidates[pr.rows[i].candidate].id == "s10") s10 = i;
        }
        CHECK(s10 + 1 == s9);
        CHECK(pr.rows[s9].score->nes == pr.rows[s10].score->nes);
        CHECK(pr.rows[s9].score->fdr_q == pr.rows[s10].score->fdr_q);
        CHECK(pr.rows[s9].score->leading_edge == pr.rows[s10].score->leading_edge);
    }
}
