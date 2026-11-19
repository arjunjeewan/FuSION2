// The prerank engine on hand-checked cases (section 08's test hooks "Generator" and "Walk and
// statistics"): the walk's score and its first extreme, the leading edge, the statistics' edge
// cases, 08.C7's all-zero draws, the limits (S7.09, 08.C8), a stop, and candidate order.
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <numeric>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "../../../modules/enrichment/src/detail.hpp"
#include "fusion/core/xoshiro.hpp"
#include "support.hpp"

namespace {

using namespace enrichment_test;
namespace detail = fusion::enrichment::detail;

// N genes g01..gNN (zero-padded, so byte order is numeric order) with the given values, ranked.
struct Case {
    core::Interner ids;
    enrichment::Ranking ranking;
};
std::unique_ptr<Case> ranked(const std::vector<double>& values) {
    auto c = std::make_unique<Case>();
    std::vector<std::pair<core::GeneId, double>> pairs;
    for (std::size_t i = 0; i < values.size(); ++i) {
        const std::string id = (i < 9 ? "g0" : "g") + std::to_string(i + 1);
        pairs.emplace_back(c->ids.id(id), values[i]);
    }
    c->ranking = detail::rank_pairs(pairs, c->ids);
    return c;
}

std::vector<std::string> genes(std::size_t first, std::size_t last) {   // g<first>..g<last>, 1-based
    std::vector<std::string> out;
    for (std::size_t i = first; i <= last; ++i) out.push_back((i < 10 ? "g0" : "g") + std::to_string(i));
    return out;
}

std::vector<std::uint32_t> positions(std::uint32_t first, std::uint32_t last, bool descending = false) {
    std::vector<std::uint32_t> out;
    for (std::uint32_t p = first; p <= last; ++p) out.push_back(p);
    if (descending) std::reverse(out.begin(), out.end());
    return out;
}

const enrichment::Score& score_of(const enrichment::Prerank& pr, const std::vector<enrichment::Candidate>& cs,
                                  std::string_view id) {
    for (const enrichment::Row& row : pr.rows)
        if (cs[row.candidate].id == id) return *row.score;
    throw std::runtime_error("no row for " + std::string(id));
}

}  // namespace

TEST_CASE("enrichment: generator and shuffle golden values", "[unit][enrichment][08.C1]") {
    // rand 0.9.2's own test stable_seed_from_u64_and_from_seed.
    core::Xoshiro256pp zero(0);
    CHECK(zero.next_u64() == 5987356902031041503ULL);
    CHECK(zero.next_u64() == 7051070477665621255ULL);
    CHECK(zero.next_u64() == 6633766593972829180ULL);
    // Section 08: the first shuffle of 0..3053 with seed 42, which matches gseapy, and the 1378
    // 32-bit draws it takes.
    std::vector<std::uint32_t> perm(3054);
    std::iota(perm.begin(), perm.end(), 0U);
    core::Xoshiro256pp rng(42);
    core::shuffle_like_rand09(perm, rng);
    CHECK(std::vector<std::uint32_t>(perm.begin(), perm.begin() + 10) ==
          std::vector<std::uint32_t>{2614, 515, 2774, 958, 1504, 1071, 1924, 2135, 1637, 2881});
    core::Xoshiro256pp counted(42);
    for (int i = 0; i < 1378; ++i) (void)counted.next_u64();
    CHECK(counted == rng);
}

TEST_CASE("enrichment: the walk's score and leading edge on hand-checked rankings", "[unit][enrichment][S7.10]") {
    // 20 genes of value 1, so W = 16 for a 16-gene set and every step is exact: hits 1/16,
    // misses 1/(20 - 16) = 1/4.
    auto c = ranked(std::vector<double>(20, 1.0));
    std::vector<enrichment::Candidate> cs = {candidate("top", genes(1, 16), c->ids),
                                             candidate("bottom", genes(5, 20), c->ids)};
    const enrichment::Prerank pr = enrichment::prerank(c->ranking, cs, enrichment::Settings{42, 1}, {});
    // The top block climbs to 1 at its last hit (position 15): the leading edge is every hit and
    // Gene % = 16/20.
    const enrichment::Score& top = score_of(pr, cs, "top");
    CHECK(top.es == 1.0);
    CHECK(top.peak == 15);
    CHECK(top.leading_edge == positions(0, 15));
    CHECK(detail::gene_percent(top.peak, 20, top.es) == "80.00%");
    // The bottom block falls to -1 over the four misses before its first hit: the running sum's
    // first minimum is position 3, a miss, so the leading edge is every hit, listed bottom up,
    // and Gene % = (20 - 3)/20 (gseapy base.py:664-666).
    const enrichment::Score& bottom = score_of(pr, cs, "bottom");
    CHECK(bottom.es == -1.0);
    CHECK(bottom.peak == 3);
    CHECK(bottom.leading_edge == positions(4, 19, true));
    CHECK(detail::gene_percent(bottom.peak, 20, bottom.es) == "85.00%");
}

TEST_CASE("enrichment: the score is the first extreme, by strict comparison", "[unit][enrichment][S7.10]") {
    // 32 genes of value 1; the set holds positions 0..7 and 24..31: the running sum reaches +0.5
    // at position 7 and -0.5 just before position 24. The first stays the score.
    auto c = ranked(std::vector<double>(32, 1.0));
    std::vector<std::string> set = genes(1, 8);
    for (const std::string& g : genes(25, 32)) set.push_back(g);
    std::vector<enrichment::Candidate> cs = {candidate("split", set, c->ids)};
    const enrichment::Prerank pr = enrichment::prerank(c->ranking, cs, enrichment::Settings{42, 1}, {});
    const enrichment::Score& s = score_of(pr, cs, "split");
    CHECK(s.es == 0.5);
    CHECK(s.peak == 7);
    CHECK(s.leading_edge == positions(0, 7));
    CHECK(detail::gene_percent(s.peak, 32, s.es) == "25.00%");
}

TEST_CASE("enrichment: nominal p counts a null equal to a negative score", "[unit][enrichment][S7.11]") {
    // e = -0.5 with nulls -0.5, -0.25, 0.75, -0.75: the core counts null <= e (S7.11's Phase 2
    // correction), so p = 2/3. NES = e/|mean(-0.5, -0.25, -0.75)| = -1.
    std::vector<double> observed = {-0.5};
    std::vector<double> nulls = {-0.5, -0.25, 0.75, -0.75};
    const detail::Statistics st = detail::statistics(observed, nulls, 4);
    CHECK(st.nom_p[0] == 2.0 / 3.0);
    CHECK(st.nes[0] == -1.0);
    // Normalised nulls -1, -0.5, 1, -1.5. FWER: per-permutation minima -1, -0.5, 0, -1.5, so
    // #(minimum < -1) / #(minimum < 0) = 1/3. FDR: #(nulls <= -1)/#(nulls < 0) = 2/3 over
    // #(NES <= -1)/#(NES < 0) = 1.
    CHECK(st.fwer_p[0] == 1.0 / 3.0);
    CHECK(st.fdr_q[0] == 2.0 / 3.0);
}

TEST_CASE("enrichment: a set with no null of the needed sign", "[unit][enrichment][S7.11][08.C7]") {
    SECTION("negative score, no negative null") {
        std::vector<double> observed = {-0.5};
        std::vector<double> nulls = {0.25, 0.5, 0.75};
        const detail::Statistics st = detail::statistics(observed, nulls, 3);
        CHECK(st.nes[0] == -1.0);     // the mean falls back to e itself (stats.rs:170-174)
        CHECK(st.nom_p[0] == 1.0);    // 0 of 0: 1 (stats.rs:211-213)
        CHECK(st.fwer_p[0] == 1.0);   // 0 of 0: gseapy's NaN, written 1.0 (08.C7)
        CHECK(st.fdr_q[0] == 0.0);    // no normalised null below 0
    }
    SECTION("positive score, no non-negative null") {
        std::vector<double> observed = {0.5};
        std::vector<double> nulls = {-0.25, -0.5};
        const detail::Statistics st = detail::statistics(observed, nulls, 2);
        CHECK(st.nes[0] == 1.0);
        CHECK(st.nom_p[0] == 1.0);
        CHECK(st.fwer_p[0] == 0.0);   // no permutation's maximum reaches 1
        CHECK(st.fdr_q[0] == 0.0);
    }
}

TEST_CASE("enrichment: FDR and FWER pool every tested set", "[unit][enrichment][S7.11][S7.12]") {
    // Set 0: nulls 0.25, 0.5, -0.25, 0.75 (means 0.5 and -0.25); set 1: e = 0.25, nulls 0.5, 0.25,
    // 0.75, -0.5 (means 0.5 and -0.5), so NES1 = 0.5 and the pooled normalised nulls are
    // -1, -1, 0.5, 0.5, 1, 1, 1.5, 1.5 (two of each); per-permutation maxima 1, 1, 1.5, 1.5.
    const std::vector<double> set0 = {0.25, 0.5, -0.25, 0.75}, set1 = {0.5, 0.25, 0.75, -0.5};
    auto run = [&](double e0) {
        std::vector<double> observed = {e0, 0.25};
        std::vector<double> nulls = set0;
        nulls.insert(nulls.end(), set1.begin(), set1.end());
        return detail::statistics(observed, nulls, 4);
    };
    SECTION("e0 = 0.75") {
        const detail::Statistics st = run(0.75);
        CHECK(st.nes == std::vector<double>{1.5, 0.5});
        CHECK(st.nom_p == std::vector<double>{1.0 / 3.0, 1.0});
        CHECK(st.fwer_p == std::vector<double>{0.5, 1.0});
        // Set 0: (2/6) / (1/2); set 1: (6/6) / (2/2).
        CHECK(st.fdr_q[0] == (2.0 / 6.0) / 0.5);
        CHECK(st.fdr_q[1] == 1.0);
    }
    SECTION("e0 = 0.5: the q-value is capped at 1") {
        const detail::Statistics st = run(0.5);
        CHECK(st.nes[0] == 1.0);
        CHECK(st.fdr_q[0] == 1.0);   // (4/6) / (1/2) = 4/3, capped
    }
}

TEST_CASE("enrichment: draws whose hits all have value 0 step by 1/m", "[unit][enrichment][08.C7]") {
    // 3 genes of value 1, 15 of value 0, 2 of value -1; the set is the 15 zeros, so its observed
    // draw has W = 0. Each hit steps by 1/15 instead of gseapy's 0 · (1/0): the walk first falls
    // by 3 · (1/5), then climbs, so the score is that first fall.
    std::vector<double> values(20, 0.0);
    values[0] = values[1] = values[2] = 1.0;
    values[18] = values[19] = -1.0;
    auto c = ranked(values);
    std::vector<enrichment::Candidate> cs = {candidate("zeros", genes(4, 18), c->ids)};
    const enrichment::Prerank pr = enrichment::prerank(c->ranking, cs, enrichment::Settings{42, 1}, {});
    const enrichment::Score& s = score_of(pr, cs, "zeros");
    CHECK(s.es == 0.0 - (1.0 / 5.0) * 3.0);
    CHECK(pr.zero_weight_draws >= 1);
    // The running sum's first minimum is position 2, so every hit is in the leading edge.
    CHECK(s.peak == 2);
    CHECK(s.leading_edge == positions(3, 17, true));
}

TEST_CASE("enrichment: the size filter and no testable candidate", "[unit][enrichment][S7.09]") {
    auto c = ranked(std::vector<double>(20, 1.0));
    std::vector<enrichment::Candidate> cs = {candidate("small", genes(1, 14), c->ids),   // m = 14
                                             candidate("all", genes(1, 20), c->ids)};    // m = N
    try {
        (void)enrichment::prerank(c->ranking, cs, enrichment::Settings{}, {});
        FAIL("no failure");
    } catch (const core::Failure& f) {
        CHECK(f.code() == "gsea.no_testable");
        CHECK(std::string(f.what()).find("15 to 500 ranked genes") != std::string::npos);
    }
    cs.push_back(candidate("ok", genes(1, 15), c->ids));
    const enrichment::Prerank pr = enrichment::prerank(c->ranking, cs, enrichment::Settings{42, 10}, {});
    CHECK(pr.tested == 1);
    REQUIRE(pr.rows.size() == 3);
    CHECK(cs[pr.rows[0].candidate].id == "ok");
    CHECK(pr.rows[1].candidate == 0);   // the untested in list order, with their m
    CHECK(pr.rows[1].ranked_genes == 14);
    CHECK(pr.rows[2].candidate == 1);
    CHECK(pr.rows[2].ranked_genes == 20);
}

TEST_CASE("enrichment: permutation counts outside 1 to 100,000 fail", "[unit][enrichment][08.C8]") {
    for (const std::uint32_t bad : {0U, 100'001U}) {
        try {
            enrichment::check_settings(enrichment::Settings{42, bad});
            FAIL("no failure");
        } catch (const core::Failure& f) {
            CHECK(f.code() == "batch.bad_value");
        }
    }
    CHECK_NOTHROW(enrichment::check_settings(enrichment::Settings{42, 1}));
    CHECK_NOTHROW(enrichment::check_settings(enrichment::Settings{0, 100'000}));
}

TEST_CASE("enrichment: a stop request stops the permutations", "[unit][enrichment]") {
    auto c = ranked(std::vector<double>(20, 1.0));
    std::vector<enrichment::Candidate> cs = {candidate("ok", genes(1, 15), c->ids)};
    core::StopSource source;
    source.request_stop();
    CHECK_THROWS_AS(enrichment::prerank(c->ranking, cs, enrichment::Settings{}, source.token()), core::Stopped);
}

TEST_CASE("enrichment: candidate order changes no score", "[unit][enrichment][S7.12][08.C9]") {
    Harness h;
    const auto f = load_fixture("n500", h);
    std::vector<enrichment::Candidate> reversed(f->candidates.rbegin(), f->candidates.rend());
    const enrichment::Prerank a = enrichment::prerank(f->ranking, f->candidates, enrichment::Settings{7, 200}, {});
    const enrichment::Prerank b = enrichment::prerank(f->ranking, reversed, enrichment::Settings{7, 200}, {});
    REQUIRE(a.tested == b.tested);
    for (std::size_t i = 0; i < a.tested; ++i) {   // the same table order, by |NES| then id
        INFO("row " << i);
        CHECK(f->candidates[a.rows[i].candidate].id == reversed[b.rows[i].candidate].id);
        const enrichment::Score &x = *a.rows[i].score, &y = *b.rows[i].score;
        CHECK(x.es == y.es);
        CHECK(x.nes == y.nes);
        CHECK(x.nom_p == y.nom_p);
        CHECK(x.fdr_q == y.fdr_q);
        CHECK(x.fwer_p == y.fwer_p);
        CHECK(x.leading_edge == y.leading_edge);
    }
}
