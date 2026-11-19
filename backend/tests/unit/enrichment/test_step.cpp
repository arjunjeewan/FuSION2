// The gsea step end to end (section 08 "Files written", "Events", "Errors and reported
// results"; S7.01 to S7.03, S7.14 to S7.17, 08.C10, 08.C13, 2B.3): n50's values through section
// 04's reader, its sets as hand-made KGML files through section 02's reader.
#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "support.hpp"

namespace {

using namespace enrichment_test;
namespace network = fusion::network;
namespace fs = std::filesystem;

// One KGML per set: a gene entry per gene, and one relation between the first two.
std::string kgml_for(const GmtSet& set) {
    std::string xml = "<?xml version=\"1.0\"?>\n<pathway name=\"path:" + set.id + "\" title=\"Set " + set.id + "\">\n";
    for (std::size_t i = 0; i < set.genes.size(); ++i)
        xml += "  <entry id=\"" + std::to_string(i + 1) + "\" name=\"" + set.genes[i] + "\" type=\"gene\"/>\n";
    if (set.genes.size() >= 2)
        xml += "  <relation entry1=\"1\" entry2=\"2\" type=\"PPrel\"><subtype name=\"activation\" value=\"--&gt;\"/></relation>\n";
    return xml + "</pathway>\n";
}

struct Inputs {
    Harness prep;   // reads the inputs, as the check before the run does (02.C15, 04.C9)
    core::Interner ids;
    std::unique_ptr<fusion::expression::GeneValues> values;
    std::vector<network::ListLine> entries;
    std::vector<std::shared_ptr<const network::PathwayDoc>> docs;
};

std::unique_ptr<Inputs> inputs(const std::string& values_text, const std::vector<GmtSet>& sets) {
    auto in = std::make_unique<Inputs>();
    in->values = std::make_unique<fusion::expression::GeneValues>(read_values(values_text, in->ids, in->prep));
    fs::create_directories(in->prep.scratch.path() / "kgml");
    for (const GmtSet& set : sets) {
        const fs::path path = in->prep.scratch.path() / "kgml" / (set.id + ".xml");
        std::ofstream(path, std::ios::binary) << kgml_for(set);
        core::Context ctx = in->prep.context();
        in->entries.push_back(network::ListLine{"kgml/" + set.id + ".xml", path});
        in->docs.push_back(network::read_kgml(path, network::IdMap::none(), ctx));
    }
    return in;
}

std::unique_ptr<Inputs> n50(const std::string& values_text = read_file(gsea_fixtures() / "n50" / "values.tsv")) {
    return inputs(values_text, read_gmt(gsea_fixtures() / "n50" / "sets.gmt"));
}

enrichment::Options options(const std::string& selector, std::vector<std::string> keep = {},
                            const std::string& shortlist = "gsea_kgml_files.txt") {
    enrichment::Options o;
    o.selector = enrichment::parse_selector(selector);
    o.keep = std::move(keep);
    o.settings = enrichment::Settings{42, 1000};
    o.shortlist_name = shortlist;
    o.candidates = "list.txt";
    return o;
}

core::Outcome<std::shared_ptr<const enrichment::Shortlist>> run(Inputs& in, Harness& h, const enrichment::Options& o,
                                                                core::StopToken stop = {}) {
    core::Context ctx = h.context(stop);
    return enrichment::gsea(*in.values, in.entries, in.docs, in.prep.scratch.path(), network::RelationFilters{}, in.ids, o, ctx);
}

std::vector<std::string> files_in(const fs::path& dir) {
    std::vector<std::string> out;
    for (const auto& e : fs::directory_iterator(dir)) out.push_back(e.path().filename().string());
    std::sort(out.begin(), out.end());
    return out;
}

std::vector<std::pair<std::string, core::Json>> gsea_events(const Harness& h) {
    std::vector<std::pair<std::string, core::Json>> out;
    for (const auto& e : h.events.events)
        if (e.first.rfind("gsea.", 0) == 0) out.push_back(e);
    return out;
}

}  // namespace

TEST_CASE("enrichment: the gsea step writes the shortlist and the table", "[unit][enrichment][S7.14][S7.15][S7.16][S7.17]") {
    auto in = n50();
    Harness h;
    // g53 is in no ranking, only in s11's file; g999 is in no file.
    const auto outcome = run(*in, h, options("fdr:0.25", {"g53", "g999"}));
    REQUIRE(outcome.value);
    CHECK_FALSE(outcome.report.has_value());
    const enrichment::Shortlist& sl = *outcome.value;
    const fs::path step = h.scratch.path() / "gsea";
    CHECK(files_in(step) == std::vector<std::string>{"gsea_kgml_files.gsea.tsv", "gsea_kgml_files.txt"});

    // At seed 42, s9 and s10 have FDR 0 (gseapy's stats.tsv); s11 (FDR 0.257) comes in by g53.
    CHECK(read_file(step / "gsea_kgml_files.txt") == "kgml/s9.xml\nkgml/s10.xml\nkgml/s11.xml\n");
    CHECK(sl.chosen_paths() == std::vector<fs::path>{in->entries[8].path, in->entries[9].path, in->entries[10].path});
    std::ostringstream table;
    enrichment::write_table(table, sl, in->ids);
    CHECK(read_file(step / "gsea_kgml_files.gsea.tsv") == table.str());
    const auto rows = lines(table.str());
    REQUIRE(rows.size() == 12);   // every candidate (S7.09)
    CHECK(split(rows[1], '\t')[0] == "s9");
    CHECK(split(rows[1], '\t')[3] == "2/15");   // the relation share (08.C5)
    CHECK(split(rows[11], '\t')[0] == "s8");   // the untested s1 and s8 last, in list order
    CHECK(sl.base == in->prep.scratch.path());
    CHECK(sl.settings.seed == 42);

    // The warnings (08.C10, S7.14) and the events (section 08 "Events").
    REQUIRE(h.warnings("gsea.ties").size() == 1);
    CHECK(h.warnings("gsea.ties")[0].text ==
          "Duplicated values found in the ranking: 64.00% of genes. Tied genes are ordered by gene id.");
    REQUIRE(h.warnings("gsea.keep_unmatched").size() == 1);
    CHECK(h.warnings("gsea.keep_unmatched")[0].text == "keep id g999 is in none of the 11 candidate pathways");
    CHECK(h.warnings("gsea.zero_weight_draws").empty());
    const auto events = gsea_events(h);
    REQUIRE(events.size() == 3);
    CHECK(events[0].first == "gsea.ranked");
    CHECK(events[0].second == core::Json{{"genes", 50}, {"distinct_values", 18}, {"in_no_candidate", 0}});
    CHECK(events[1].first == "gsea.tested");
    CHECK(events[1].second == core::Json{{"candidates", 11}, {"tested", 9}, {"permutations", 1000}, {"seed", 42}});
    CHECK(events[2].first == "gsea.selected");
    CHECK(events[2].second == core::Json{{"selector", "fdr:0.25"}, {"by_gsea", 2}, {"by_keep", 1},
                                         {"shortlist", "gsea/gsea_kgml_files.txt"}, {"table", "gsea/gsea_kgml_files.gsea.tsv"}});

    // The console report (S7.17).
    const auto info = h.info();
    REQUIRE(info.size() >= 6);
    CHECK(info[0] == "GSEA pre-ranked on values.tsv");
    CHECK(info[1] == "  50 genes ranked by log-fold change, 1000 permutations");
    CHECK(info[2] == "  9 of 11 candidate pathways tested (the rest have fewer than 15 or more than 500 ranked genes)");
    CHECK(info[3] == "  NES over tested pathways: -2.672 to +2.757");
    CHECK(info[5] == "selected 2 of 9 tested pathways (fdr:0.25)");
    CHECK(info[6] == "plus 1 kept for containing a keep gene (candidate pathways per gene: g53 1, g999 0)");
    CHECK(info.back() == "  full table -> " + core::to_utf8(step / "gsea_kgml_files.gsea.tsv"));
    CHECK(info[info.size() - 2] ==
          "  shortlist  -> " + core::to_utf8(step / "gsea_kgml_files.txt") + "  (3 KGML files, in list.txt order)");
}

TEST_CASE("enrichment: an empty selection is a reported result", "[unit][enrichment][S7.03]") {
    auto in = n50();
    Harness h;
    const auto outcome = run(*in, h, options("nes:9"));
    REQUIRE(outcome.report.has_value());
    CHECK(outcome.report->code == "gsea.empty_selection");
    CHECK(outcome.report->message == "no pathway passed 'nes:9'; loosen the selector");
    REQUIRE(outcome.value);
    CHECK(outcome.value->selection.empty());
    // The table and a 0-byte shortlist are written (S7.03, S7.15).
    const fs::path step = h.scratch.path() / "gsea";
    CHECK(fs::file_size(step / "gsea_kgml_files.txt") == 0);
    CHECK(lines(read_file(step / "gsea_kgml_files.gsea.tsv")).size() == 12);
}

TEST_CASE("enrichment: an absolute shortlist path gets both files there and in the step folder", "[unit][enrichment][2B.3]") {
    auto in = n50();
    Harness h;
    const fs::path elsewhere = h.scratch.path() / "elsewhere";
    fs::create_directories(elsewhere);
    const auto outcome = run(*in, h, options("fdr:0.25", {}, core::to_utf8(elsewhere / "picked.txt")));
    const fs::path step = h.scratch.path() / "gsea";
    CHECK(files_in(step) == std::vector<std::string>{"picked.gsea.tsv", "picked.txt"});
    CHECK(files_in(elsewhere) == std::vector<std::string>{"picked.gsea.tsv", "picked.txt"});
    CHECK(read_file(elsewhere / "picked.txt") == read_file(step / "picked.txt"));
    CHECK(read_file(elsewhere / "picked.gsea.tsv") == read_file(step / "picked.gsea.tsv"));
    CHECK(gsea_events(h).back().second["shortlist"] == "gsea/picked.txt");
    CHECK(h.info().back() == "  full table -> " + core::to_utf8(elsewhere / "picked.gsea.tsv"));
}

TEST_CASE("enrichment: a failure while writing leaves no shortlist", "[unit][enrichment][08.C13]") {
    auto in = n50();
    Harness h;
    h.files.fail_on_create = 2;   // the table's file cannot be made, after the shortlist's was
    try {
        (void)run(*in, h, options("fdr:0.25"));
        FAIL("no failure");
    } catch (const core::Failure& f) {
        CHECK(f.code() == "io.write");
    }
    CHECK(files_in(h.scratch.path() / "gsea").empty());   // no shortlist, no temporary file
}

TEST_CASE("enrichment: a stop leaves no output", "[unit][enrichment]") {
    auto in = n50();
    Harness h;
    core::StopSource source;
    source.request_stop();
    CHECK_THROWS_AS(run(*in, h, options("fdr:0.25"), source.token()), core::Stopped);
    CHECK(files_in(h.scratch.path() / "gsea").empty());
}

TEST_CASE("enrichment: no candidate of 15 to 500 ranked genes fails the step", "[unit][enrichment][S7.09]") {
    auto in = inputs(read_file(gsea_fixtures() / "n50" / "values.tsv"),
                     {GmtSet{"small", "random", {"g1", "g2", "g3"}}, GmtSet{"other", "random", {"g4", "g5"}}});
    Harness h;
    try {
        (void)run(*in, h, options("fdr:0.25"));
        FAIL("no failure");
    } catch (const core::Failure& f) {
        CHECK(f.code() == "gsea.no_testable");
    }
    CHECK(files_in(h.scratch.path() / "gsea").empty());
}

TEST_CASE("enrichment: settings out of range fail before any output", "[unit][enrichment][08.C8]") {
    auto in = n50();
    Harness h;
    enrichment::Options o = options("fdr:0.25");
    o.settings.permutations = 100'001;
    try {
        (void)run(*in, h, o);
        FAIL("no failure");
    } catch (const core::Failure& f) {
        CHECK(f.code() == "batch.bad_value");
    }
    CHECK(files_in(h.scratch.path() / "gsea").empty());
}

TEST_CASE("enrichment: the value file's line order leaves the table's bytes unchanged", "[unit][enrichment][S7.05]") {
    const std::string text = read_file(gsea_fixtures() / "n50" / "values.tsv");
    std::vector<std::string> rows = lines(text);
    std::reverse(rows.begin(), rows.end());
    std::string reversed;
    for (const std::string& row : rows) reversed += row + "\n";
    auto a = n50(text), b = n50(reversed);
    Harness ha, hb;
    (void)run(*a, ha, options("fdr:0.25"));
    (void)run(*b, hb, options("fdr:0.25"));
    CHECK(read_file(ha.scratch.path() / "gsea" / "gsea_kgml_files.gsea.tsv") ==
          read_file(hb.scratch.path() / "gsea" / "gsea_kgml_files.gsea.tsv"));
}

TEST_CASE("enrichment: draws whose hits all have value 0 are warned about", "[unit][enrichment][08.C7]") {
    // 3 genes of value 1, 15 of value 0, 2 of value -1, and a set on the 15 zeros.
    std::string values;
    std::vector<std::string> zeros;
    for (int i = 1; i <= 20; ++i) {
        const std::string id = "z" + std::to_string(i);
        values += id + "\t" + (i <= 3 ? "1.00" : i <= 18 ? "0.00" : "-1.00") + "\n";
        if (i > 3 && i <= 18) zeros.push_back(id);
    }
    auto in = inputs(values, {GmtSet{"zeros", "zero-heavy", zeros}});
    Harness h;
    enrichment::Options o = options("pval:1");
    o.settings.permutations = 20;
    const auto outcome = run(*in, h, o);
    REQUIRE(outcome.value);
    CHECK(outcome.value->prerank.zero_weight_draws >= 1);
    const auto warned = h.warnings("gsea.zero_weight_draws");
    REQUIRE(warned.size() == 1);
    CHECK(warned[0].text == std::to_string(outcome.value->prerank.zero_weight_draws) +
                                " draws had hits that all have value 0; each of their hits took the step 1/m (08.C7)");
}
