// The selector and the keep list (S7.13, S7.14): the grammar, thresholds before top:N, and
// keep ids choosing candidates whether tested or not.
#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "support.hpp"

namespace {

using namespace enrichment_test;

// Five candidates in list order; rows in table order (|NES| descending), c4 untested.
struct Table {
    core::Interner ids;
    std::vector<enrichment::Candidate> candidates;
    enrichment::Prerank prerank;
};
std::unique_ptr<Table> table() {
    auto t = std::make_unique<Table>();
    t->candidates = {candidate("c0", {"hsa1956", "hsa1"}, t->ids), candidate("c1", {"hsa2"}, t->ids),
                     candidate("c2", {"hsa1956", "hsa3"}, t->ids), candidate("c3", {"hsa4"}, t->ids),
                     candidate("c4", {"hsa4357"}, t->ids, "Sulfur relay system")};
    auto row = [](std::uint32_t c, double nes, double p, double fdr) {
        return enrichment::Row{c, 20, enrichment::Score{nes / 3, nes, p, fdr, 1.0, {}, 0}};
    };
    t->prerank.rows = {row(0, 2.0, 0.0, 0.01), row(1, -1.8, 0.01, 0.05), row(2, 1.5, 0.02, 0.3),
                       row(3, -1.2, 0.2, 0.04), enrichment::Row{4, 6, std::nullopt}};
    t->prerank.tested = 4;
    return t;
}

std::vector<std::uint32_t> chosen_by_gsea(const std::string& selector, std::vector<std::string> keep = {}) {
    const auto t = table();
    const enrichment::Selection sel =
        enrichment::select(t->prerank, t->candidates, enrichment::parse_selector(selector), keep, t->ids);
    std::vector<std::uint32_t> out;
    for (std::uint32_t c = 0; c < sel.by_gsea.size(); ++c)
        if (sel.by_gsea[c]) out.push_back(c);
    return out;
}

}  // namespace

TEST_CASE("enrichment: bad selector terms fail, naming the term", "[unit][enrichment][S7.13]") {
    // S7.13's measured cases, which FuSION 1 refused with exit 2 or accepted silently, and the
    // out-of-range fdr:1.5.
    const std::vector<std::pair<std::string, std::string>> cases = {
        {"FDR:0.25", "'FDR:0.25'"}, {"fdr:", "'fdr:'"},     {"fdr:0.25,", "''"},     {"top:2.5", "'top:2.5'"},
        {"", "''"},                 {"top:-5", "'top:-5'"}, {"top:0", "'top:0'"},    {"fdr:nan", "'fdr:nan'"},
        {"fdr:inf", "'fdr:inf'"},   {"nes:-1", "'nes:-1'"}, {"fdr:0_5", "'fdr:0_5'"}, {"fdr:1.5", "'fdr:1.5'"},
        {"pval:2", "'pval:2'"},     {"fdr:0.2,top", "'top'"}, {"size:3", "'size:3'"},
    };
    for (const auto& [text, named] : cases) {
        INFO("selector \"" << text << "\"");
        try {
            (void)enrichment::parse_selector(text);
            FAIL("no failure");
        } catch (const core::Failure& f) {
            CHECK(f.code() == "gsea.selector");
            CHECK(std::string(f.what()).find("bad selector term " + named) != std::string::npos);
        }
    }
}

TEST_CASE("enrichment: selector values and repeated keys", "[unit][enrichment][S7.13]") {
    const enrichment::Selector s = enrichment::parse_selector("fdr:0.3,pval:0.05,nes:1.5,top:10,fdr:0.2,top:20,nes:1");
    CHECK(s.text == "fdr:0.3,pval:0.05,nes:1.5,top:10,fdr:0.2,top:20,nes:1");
    CHECK(s.max_fdr == 0.2);       // every term applies: the strictest of each key stands
    CHECK(s.max_pval == 0.05);
    CHECK(s.min_abs_nes == 1.5);
    CHECK(s.top == 10U);
    CHECK(enrichment::parse_selector("fdr:1e-1").max_fdr == 0.1);
    CHECK(enrichment::parse_selector("fdr:0").max_fdr == 0.0);
    CHECK(enrichment::parse_selector("pval:1").max_pval == 1.0);
    CHECK(enrichment::parse_selector("top:099999999999").top == std::numeric_limits<std::uint32_t>::max());
    const enrichment::Selector none = enrichment::parse_selector("top:3");
    CHECK_FALSE(none.max_fdr.has_value());
    CHECK_FALSE(none.max_pval.has_value());
    CHECK_FALSE(none.min_abs_nes.has_value());
}

TEST_CASE("enrichment: thresholds apply before top, in table order", "[unit][enrichment][S7.13]") {
    CHECK(chosen_by_gsea("fdr:0.06") == std::vector<std::uint32_t>{0, 1, 3});
    CHECK(chosen_by_gsea("top:2") == std::vector<std::uint32_t>{0, 1});
    CHECK(chosen_by_gsea("pval:0.015") == std::vector<std::uint32_t>{0, 1});
    CHECK(chosen_by_gsea("nes:1.5") == std::vector<std::uint32_t>{0, 1, 2});   // both signs count
    // Term order never matters: FuSION 1 applied top:3 first and kept c0 and c1 only
    // (gsea_select_pathways.py:119-130).
    CHECK(chosen_by_gsea("top:3,fdr:0.06") == std::vector<std::uint32_t>{0, 1, 3});
    CHECK(chosen_by_gsea("fdr:0.06,top:3") == std::vector<std::uint32_t>{0, 1, 3});
    CHECK(chosen_by_gsea("top:10,fdr:0.06") == chosen_by_gsea("fdr:0.06,top:10"));
    // The untested c4 never passes, whatever the selector (S7.09).
    CHECK(chosen_by_gsea("pval:1") == std::vector<std::uint32_t>{0, 1, 2, 3});
}

TEST_CASE("enrichment: the keep list chooses every candidate that holds a keep id", "[unit][enrichment][S7.14][S7.03]") {
    const auto t = table();
    const std::vector<std::string> keep = {"hsa1956", "", "hsa4357", "hsa22822", "hsa1956"};
    const enrichment::Selection sel =
        enrichment::select(t->prerank, t->candidates, enrichment::parse_selector("fdr:0.06"), keep, t->ids);
    // c0 holds hsa1956 but GSEA chose it, so it has no kept_for; c2 is chosen by hsa1956 alone;
    // the untested c4 by hsa4357 (FuSION 1's copy kept Sulfur relay system for MPST, S7.16).
    CHECK(sel.chosen(0));
    CHECK(sel.kept_for[0].empty());
    CHECK(sel.chosen(2));
    CHECK_FALSE(sel.by_gsea[2]);
    CHECK(sel.kept_for[2].front() == "hsa1956");
    CHECK(sel.chosen(4));
    CHECK(sel.kept_for[4] == std::vector<std::string>{"hsa4357"});
    CHECK_FALSE(sel.empty());
    // hsa22822 is in no candidate: one warning's worth (S7.14). Counts are per distinct id.
    CHECK(sel.unmatched == std::vector<std::string>{"hsa22822"});
    CHECK(sel.keep_counts == std::vector<std::pair<std::string, std::uint32_t>>{{"hsa1956", 2}, {"hsa4357", 1}, {"hsa22822", 0}});
}

TEST_CASE("enrichment: nothing passing is an empty selection", "[unit][enrichment][S7.03]") {
    const auto t = table();
    const enrichment::Selection sel =
        enrichment::select(t->prerank, t->candidates, enrichment::parse_selector("nes:2.5"), {}, t->ids);
    CHECK(sel.empty());
    CHECK(sel.keep_counts.empty());
    enrichment::Shortlist sl;
    sl.candidates = t->candidates;
    sl.selection = sel;
    CHECK(sl.chosen_paths().empty());
}
