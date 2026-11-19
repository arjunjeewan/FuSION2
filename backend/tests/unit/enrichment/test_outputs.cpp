// The outputs (S7.15 to S7.17; 08.C5, 08.C10): the table's name, the shortlist, the table's
// rows and values, and the console report. Expected text was worked out with FuSION 1's own
// Python format strings (gsea_select_pathways.py:138-160) and Python's number formats.
#include <catch2/catch_test_macros.hpp>

#include <locale>
#include <sstream>
#include <string>
#include <vector>

#include "../../../modules/enrichment/src/detail.hpp"
#include "support.hpp"

namespace {

using namespace enrichment_test;
namespace detail = fusion::enrichment::detail;

// Three candidates: hsa04010 chosen by GSEA, hsa05200 tested but not chosen, hsa04122 untested
// and kept for hsa4357.
struct Example {
    core::Interner ids;
    enrichment::Shortlist shortlist;
};
std::unique_ptr<Example> example() {
    auto e = std::make_unique<Example>();
    enrichment::Shortlist& sl = e->shortlist;
    for (const char* g : {"g1", "g2", "g3", "g4", "g5"}) sl.ranking.id.push_back(e->ids.id(g));
    sl.ranking.value = {2.0, 1.0, 0.5, -1.0, -2.0};
    sl.ranking.weight = {2.0, 1.0, 0.5, 1.0, 2.0};
    sl.ranking.distinct_values = 5;
    sl.candidates = {candidate("hsa04010", {"g1", "g2", "g3", "g9"}, e->ids, "MAPK\tsignaling"),
                     candidate("hsa04122", {"g4", "x1", "x2", "x3", "x4", "x5", "x6", "x7"}, e->ids, "Sulfur relay system"),
                     candidate("hsa05200", {"g3", "g4", "g5"}, e->ids, "Other")};
    sl.candidates[0].on_relations = 2;
    sl.candidates[1].on_relations = 8;
    sl.prerank.rows = {enrichment::Row{0, 3, enrichment::Score{0.1 + 0.2, 1.25, 0.0, 0.001, 1.0, {0, 1}, 1}},
                       enrichment::Row{2, 3, enrichment::Score{-0.75, -1.0, 0.5, 0.25, 0.125, {4, 3}, 3}},
                       enrichment::Row{1, 1, std::nullopt}};
    sl.prerank.tested = 2;
    sl.selection.by_gsea = {true, false, false};
    sl.selection.kept_for = {{}, {"hsa4357"}, {}};
    sl.selection.keep_counts = {{"hsa4357", 1}};
    sl.selector = enrichment::parse_selector("fdr:0.25");
    sl.settings = enrichment::Settings{42, 1000};
    return e;
}

}  // namespace

TEST_CASE("enrichment: the table's name is the shortlist's stem plus .gsea.tsv", "[unit][enrichment][S7.16]") {
    // os.path.splitext's rule (gsea_select_pathways.py:231).
    CHECK(enrichment::table_name("gsea_kgml_files.txt") == "gsea_kgml_files.gsea.tsv");
    CHECK(enrichment::table_name("run/gsea_kgml_files.txt") == "run/gsea_kgml_files.gsea.tsv");
    CHECK(enrichment::table_name("list") == "list.gsea.tsv");
    CHECK(enrichment::table_name("a.b.c") == "a.b.gsea.tsv");
    CHECK(enrichment::table_name(".hidden") == ".hidden.gsea.tsv");
    CHECK(enrichment::table_name("..x") == "..x.gsea.tsv");
    CHECK(enrichment::table_name("x.") == "x.gsea.tsv");
    CHECK(enrichment::table_name("dir.d/list") == "dir.d/list.gsea.tsv");
    CHECK(enrichment::table_name("C:\\out\\list.txt") == "C:\\out\\list.gsea.tsv");
    CHECK(enrichment::table_name("x.gsea.tsv") != "x.gsea.tsv");
}

TEST_CASE("enrichment: the shortlist lists the chosen files' lines in list order", "[unit][enrichment][S7.15]") {
    auto e = example();
    std::ostringstream out;
    enrichment::write_shortlist(out, e->shortlist);
    CHECK(out.str() == "/kgml/hsa04010.xml\n/kgml/hsa04122.xml\n");
    CHECK(e->shortlist.chosen_paths() == std::vector<std::filesystem::path>{"/kgml/hsa04010.xml", "/kgml/hsa04122.xml"});
    // Nothing chosen: 0 bytes (S7.15).
    e->shortlist.selection.by_gsea = {false, false, false};
    e->shortlist.selection.kept_for = {{}, {}, {}};
    std::ostringstream none;
    enrichment::write_shortlist(none, e->shortlist);
    CHECK(none.str().empty());
}

TEST_CASE("enrichment: the table has a row for every candidate", "[unit][enrichment][S7.09][S7.16][08.C5]") {
    auto e = example();
    std::ostringstream out;
    enrichment::write_table(out, e->shortlist, e->ids);
    const auto rows = lines(out.str());
    REQUIRE(rows.size() == 4);
    CHECK(rows[0] == "Term\tpathway_name\tgenes\trelation_share\tES\tNES\tNOM p-val\tFDR q-val\tFWER p-val\t"
                     "Tag %\tGene %\tLead_genes\tselected\tkept_for\tkgml");
    // Tested rows by |NES|, numbers in Python's repr layout, the tab in the title a space, the
    // relation share over every gene of the file (08.C5), ranked or not.
    CHECK(rows[1] == "hsa04010\tMAPK signaling\t3\t2/4\t0.30000000000000004\t1.25\t0.0\t0.001\t1.0\t2/3\t40.00%\t"
                     "g1;g2\tTrue\t\t/kgml/hsa04010.xml");
    CHECK(rows[2] == "hsa05200\tOther\t3\t0/3\t-0.75\t-1.0\t0.5\t0.25\t0.125\t2/3\t40.00%\tg5;g4\tFalse\t\t/kgml/hsa05200.xml");
    // The untested candidate: its m and no scores (S7.09), kept for hsa4357 (S7.14).
    CHECK(rows[3] == "hsa04122\tSulfur relay system\t1\t8/8" + std::string(9, '\t') + "True\thsa4357\t/kgml/hsa04122.xml");
    CHECK(out.str().find('\r') == std::string::npos);
    // Numbers read back to the same doubles.
    const auto cells = split(rows[1], '\t');
    CHECK(number(cells[4]) == 0.1 + 0.2);
    CHECK(number(cells[7]) == 0.001);
}

TEST_CASE("enrichment: the table's bytes do not depend on the stream's locale", "[unit][enrichment][S7.16]") {
    // A locale that groups digits and writes a decimal comma, as de_DE does (09.C16).
    struct Grouping : std::numpunct<char> {
        explicit Grouping(std::size_t refs) : std::numpunct<char>(refs) {}
        char do_thousands_sep() const override { return '.'; }
        std::string do_grouping() const override { return "\3"; }
        char do_decimal_point() const override { return ','; }
    };
    static Grouping facet{1};   // refs = 1: the locale never deletes it
    auto e = example();
    e->shortlist.prerank.rows[0].ranked_genes = 3054;
    std::ostringstream plain, grouped;
    grouped.imbue(std::locale(std::locale::classic(), &facet));
    enrichment::write_table(plain, e->shortlist, e->ids);
    enrichment::write_table(grouped, e->shortlist, e->ids);
    CHECK(grouped.str() == plain.str());
    CHECK(plain.str().find("\t3054\t") != std::string::npos);
}

TEST_CASE("enrichment: Python's fixed-point and percent formats", "[unit][enrichment][S7.17]") {
    CHECK(detail::fixed(0.125, 2) == "0.12");
    CHECK(detail::fixed(0.375, 2) == "0.38");
    CHECK(detail::fixed(2.675, 2) == "2.67");
    CHECK(detail::fixed(1.005, 2) == "1.00");
    CHECK(detail::signed_fixed(-0.0004, 3) == "-0.000");
    CHECK(detail::signed_fixed(0.0004, 3) == "+0.000");
    CHECK(detail::signed_fixed(-0.0, 3) == "-0.000");
    CHECK(detail::signed_fixed(0.9995, 3) == "+1.000");
    CHECK(detail::signed_fixed(-2.0005, 3) == "-2.001");
    CHECK(detail::percent(2864.0 / 3054.0) == "93.78%");
    CHECK(detail::percent(1.0 / 3.0) == "33.33%");
    CHECK(detail::percent(2.0 / 3.0) == "66.67%");
    CHECK(detail::gene_percent(0, 3054, 0.5) == "0.03%");
    CHECK(detail::gene_percent(3053, 3054, -0.5) == "0.03%");
    CHECK(detail::gene_percent(6, 7, 0.1) == "100.00%");
}

TEST_CASE("enrichment: the console report", "[unit][enrichment][S7.17][08.C10]") {
    auto e = example();
    Harness h;
    const enrichment::Shown shown{"values.tsv", "list.txt", "/run/gsea/list.txt", "/run/gsea/list.gsea.tsv"};
    enrichment::report(h.log, e->shortlist, e->ids, shown);
    CHECK(h.info() == std::vector<std::string>{
                          "GSEA pre-ranked on values.tsv",
                          "  5 genes ranked by log-fold change, 1000 permutations",
                          "  2 of 3 candidate pathways tested (the rest have fewer than 15 or more than 500 ranked genes)",
                          "  NES over tested pathways: -1.000 to +1.250",
                          "",
                          "selected 1 of 2 tested pathways (fdr:0.25)",
                          "plus 1 kept for containing a keep gene (candidate pathways per gene: hsa4357 1)",
                          "    #  pathway   name                genes      ES     NES   NOM p   FDR q  kept for",
                          "  ----------------------------------------------------------------------------------",
                          "    1  hsa04010  MAPK signaling          3  +0.300  +1.250   0.000   0.001  ",
                          "    2  hsa04122  Sulfur relay system     1       -       -       -       -  hsa4357",
                          "",
                          "  shortlist  -> /run/gsea/list.txt  (2 KGML files, in list.txt order)",
                          "  full table -> /run/gsea/list.gsea.tsv",
                      });
}
