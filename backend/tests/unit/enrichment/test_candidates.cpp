// Candidates from KGML files (S7.06 to S7.08; 08.C3 to 08.C5): ids, titles, gene sets, the
// relation share, duplicate pathway ids and the run's id space. The files are hand-written, with
// made-up ids, and read by section 02's reader.
#include <catch2/catch_test_macros.hpp>

#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "support.hpp"

namespace {

using namespace enrichment_test;
namespace network = fusion::network;

// hsa101 and hsa102 share entry 1; hsa101 repeats in entry 5; the group 7 holds hsa104 (entry 3)
// and hsa106; hsa105 and hsa107 sit only on an indirect relation; hsa108 on none; a compound and
// a map entry add no gene.
constexpr const char* alpha_kgml = R"(<?xml version="1.0"?>
<pathway name="path:hsa90001" org="hsa" number="90001" title="Alpha pathway">
  <entry id="1" name="hsa:101 hsa:102" type="gene"/>
  <entry id="2" name="hsa:103" type="gene"/>
  <entry id="3" name="hsa:104" type="gene"/>
  <entry id="4" name="hsa:105" type="gene"/>
  <entry id="5" name="hsa:101" type="gene"/>
  <entry id="6" name="cpd:C00001" type="compound"/>
  <entry id="7" name="undefined" type="group">
    <component id="3"/>
    <component id="8"/>
  </entry>
  <entry id="8" name="hsa:106" type="gene"/>
  <entry id="9" name="hsa:107" type="gene"/>
  <entry id="10" name="hsa:108" type="gene"/>
  <entry id="11" name="path:hsa04010" type="map"/>
  <relation entry1="1" entry2="2" type="PPrel"><subtype name="activation" value="--&gt;"/></relation>
  <relation entry1="7" entry2="6" type="PCrel"><subtype name="activation" value="--&gt;"/></relation>
  <relation entry1="4" entry2="9" type="GErel"><subtype name="indirect effect" value="..&gt;"/></relation>
  <relation entry1="2" entry2="11" type="maplink"/>
</pathway>
)";

// No root name or title: the id is the file stem (2B.12), the title the id.
constexpr const char* nameless_kgml = R"(<?xml version="1.0"?>
<pathway>
  <entry id="1" name="hsa:201 hsa:202" type="gene"/>
  <entry id="2" name="hsa:203" type="gene"/>
  <relation entry1="1" entry2="2" type="PPrel"><subtype name="binding/association" value="---"/></relation>
</pathway>
)";

// The same pathway id as alpha, in another file (S7.07).
constexpr const char* alpha_twin_kgml = R"(<?xml version="1.0"?>
<pathway name="path:hsa90001" title="Alpha again">
  <entry id="1" name="hsa:301" type="gene"/>
  <entry id="2" name="hsa:302" type="gene"/>
  <relation entry1="1" entry2="2" type="PPrel"><subtype name="activation" value="--&gt;"/></relation>
</pathway>
)";

struct Docs {
    std::vector<network::ListLine> entries;
    std::vector<std::shared_ptr<const network::PathwayDoc>> docs;
};

std::filesystem::path write(const ScratchDir& dir, const std::string& name, const char* text) {
    const std::filesystem::path path = dir.path() / name;
    std::ofstream(path, std::ios::binary) << text;
    return path;
}

void add(Docs& d, Harness& h, const std::filesystem::path& path, const std::string& text_as_listed) {
    core::Context ctx = h.context();
    d.entries.push_back(network::ListLine{text_as_listed, path});
    d.docs.push_back(network::read_kgml(path, network::IdMap::none(), ctx));
}

}  // namespace

TEST_CASE("enrichment: a candidate's id, title and gene set come from its KGML", "[unit][enrichment][S7.07][S7.08][08.C3]") {
    Harness h;
    Docs d;
    add(d, h, write(h.scratch, "alpha.xml", alpha_kgml), "kgml/alpha.xml");
    add(d, h, write(h.scratch, "hsa90002.xml", nameless_kgml), "hsa90002");
    const auto candidates = enrichment::make_candidates(d.entries, d.docs, network::RelationFilters{});
    REQUIRE(candidates.size() == 2);

    const enrichment::Candidate& alpha = candidates[0];
    CHECK(alpha.id == "hsa90001");            // "path:" removed (gsea_select_pathways.py:111)
    CHECK(alpha.title == "Alpha pathway");    // the root title (entry 205)
    CHECK(alpha.entry.text == "kgml/alpha.xml");
    // Every id of every gene entry, ':' removed, once each; the compound, group and map add none.
    CHECK(alpha.gene_text == std::vector<std::string>{"hsa101", "hsa102", "hsa103", "hsa104", "hsa105", "hsa106",
                                                      "hsa107", "hsa108"});
    CHECK(alpha.genes.size() == 8);

    const enrichment::Candidate& nameless = candidates[1];
    CHECK(nameless.id == "hsa90002");         // the file stem (2B.12)
    CHECK(nameless.title == "hsa90002");      // no title: the id
    CHECK(nameless.gene_text == std::vector<std::string>{"hsa201", "hsa202", "hsa203"});
}

TEST_CASE("enrichment: the relation share counts genes on relations the merge keeps", "[unit][enrichment][S7.08][08.C4][08.C5]") {
    Harness h;
    Docs d;
    add(d, h, write(h.scratch, "alpha.xml", alpha_kgml), "alpha.xml");
    // By default the indirect relation is dropped (S2.11) and the maplink skipped (02.C7): genes
    // on the PPrel (hsa101 to hsa103) and on the group's PCrel (hsa104, hsa106) count.
    CHECK(enrichment::make_candidates(d.entries, d.docs, network::RelationFilters{})[0].on_relations == 5);
    network::RelationFilters keep_indirect;
    keep_indirect.indirect = network::IndirectMode::keep;
    CHECK(enrichment::make_candidates(d.entries, d.docs, keep_indirect)[0].on_relations == 7);
    network::RelationFilters keep_maplink;
    keep_maplink.maplink = network::MaplinkMode::keep;   // its gene, hsa103, is already counted
    CHECK(enrichment::make_candidates(d.entries, d.docs, keep_maplink)[0].on_relations == 5);
}

TEST_CASE("enrichment: two files with one pathway id fail, naming both", "[unit][enrichment][S7.07]") {
    Harness h;
    Docs d;
    const auto first = write(h.scratch, "alpha.xml", alpha_kgml);
    const auto second = write(h.scratch, "alpha_again.xml", alpha_twin_kgml);
    add(d, h, first, "alpha.xml");
    add(d, h, second, "alpha_again.xml");
    try {
        (void)enrichment::make_candidates(d.entries, d.docs, network::RelationFilters{});
        FAIL("no failure");
    } catch (const core::Failure& f) {
        CHECK(f.code() == "list.duplicate_pathway");
        const std::string message = f.what();
        CHECK(message.find(core::to_utf8(first)) != std::string::npos);
        CHECK(message.find(core::to_utf8(second)) != std::string::npos);
    }
}

TEST_CASE("enrichment: one file listed twice is one candidate", "[unit][enrichment][S7.07]") {
    Harness h;
    Docs d;
    const auto path = write(h.scratch, "alpha.xml", alpha_kgml);
    add(d, h, path, "alpha.xml");
    d.entries.push_back(network::ListLine{"./alpha.xml", h.scratch.path() / "." / "alpha.xml"});
    d.docs.push_back(d.docs[0]);
    const auto candidates = enrichment::make_candidates(d.entries, d.docs, network::RelationFilters{});
    REQUIRE(candidates.size() == 1);
    CHECK(candidates[0].entry.text == "alpha.xml");
}

TEST_CASE("enrichment: gene ids are mapped into the run's interner by text", "[unit][enrichment][S7.08][S7.14][08.C3]") {
    Harness h;
    Docs d;
    add(d, h, write(h.scratch, "alpha.xml", alpha_kgml), "alpha.xml");
    // The run's interner, which the values went through, knows two of alpha's genes.
    core::Interner ids;
    const core::GeneId other = ids.id("hsa999");
    const core::GeneId g103 = ids.id("hsa103");
    const core::GeneId g101 = ids.id("hsa101");
    const auto candidates = enrichment::make_candidates(d.entries, d.docs, network::RelationFilters{}, ids);
    REQUIRE(candidates.size() == 1);
    CHECK(candidates[0].genes == std::vector<core::GeneId>{g103, g101});   // sorted by handle
    CHECK(candidates[0].gene_text.size() == 8);                            // every gene, ranked or not
    CHECK(candidates[0].on_relations == 5);
    (void)other;

    // A keep id that only the file names, never ranked, still chooses the pathway (S7.14).
    enrichment::Prerank pr;
    pr.rows.push_back(enrichment::Row{0, 2, std::nullopt});
    const std::vector<std::string> keep = {"hsa108"};
    const enrichment::Selection sel = enrichment::select(pr, candidates, enrichment::Selector{}, keep, ids);
    CHECK(sel.chosen(0));
    CHECK(sel.kept_for[0] == std::vector<std::string>{"hsa108"});
    CHECK(sel.unmatched.empty());
}
