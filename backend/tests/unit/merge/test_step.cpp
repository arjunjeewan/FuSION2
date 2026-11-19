// The mff step (S2.16; section 03, "The step"): files, events, console lines and reuse; network files
// (03.C7) and legacy files (03.C9) in the list. The legacy case is FuSION 1's regenerated toy case
// toy-hsa7-wni-tsv-b10 (booking 1), whose batch runs a one-file `mff ... y` over a FuSION 1-written network.
#include <regex>
#include <set>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "support.hpp"

using namespace merge_test;
using Catch::Matchers::ContainsSubstring;
using Catch::Matchers::StartsWith;
using fusion::merge::MergeRequest;
using fusion::merge::MergeStore;
using fusion::merge::merge_step;
using fusion::network::Format;
using fusion::network::SourceRef;

namespace {

SourceRef listed(const fs::path& path, Format format = Format::kgml) {
    SourceRef s;
    s.path = path;
    s.format = format;
    return s;
}

struct TwoPathways {
    fs::path a, b;
    explicit TwoPathways(Context& c) {
        a = c.scratch.write("kegg/hsa04010.xml", Kgml("path:hsa04010", "MAPK signaling pathway")
                                                    .entry(1, "hsa:2353 hsa:3725")
                                                    .entry(2, "hsa:5599 hsa:5601 hsa:5602")
                                                    .entry(3, "hsa:1")
                                                    .rel(2, 1, {"phosphorylation"}, "PPrel", "120")
                                                    .rel(1, 3, {"expression"}, "GErel")
                                                    .text());
        b = c.scratch.write("kegg/hsa05161.xml", Kgml("path:hsa05161", "Hepatitis B")
                                                    .entry(1, "hsa:3725 hsa:5599 hsa:5601 hsa:5602")
                                                    .entry(2, "hsa:7")
                                                    .rel(1, 2, {"activation"})
                                                    .text());
    }
};

}  // namespace

TEST_CASE("mff merges, splits, saves the step and reports it", "[unit][merge][S2.16][S2.24]") {
    Context c;
    TwoPathways p(c);
    fusion::core::Interner ids;
    const std::vector<fusion::core::GeneId> fsc = {ids.id("hsa3725")};
    MergeRequest req;
    req.inputs = {listed(p.b), listed(p.a)};
    req.exceptions.fscs = fsc;
    req.exceptions.not_merged = c.scratch.write("hsa_ids_not_to_be_merged.txt", "hsa5599\n");
    req.exceptions.interner = &ids;
    req.report = "merged_edges.tsv";
    req.graph = 1;
    MergeStore store;
    fusion::network::StepDocs docs;
    const auto out = merge_step(req, store, docs, c.ctx);
    CHECK_FALSE(out.report);
    const auto& net = *out.value.network;
    CHECK(member_sets(net) == std::vector<std::string>{"hsa1", "hsa2353", "hsa3725", "hsa5599", "hsa5601 hsa5602", "hsa7"});
    CHECK(out.value.exceptions.size() == 2);
    CHECK(out.value.source == fusion::merge::BaseSource::built);

    // The saved step, exceptions.tsv and the report, in the step's directory (09.C2).
    const fs::path dir = c.files.step_dir();
    CHECK(fs::exists(dir / "merged_graph.xml"));
    CHECK(read_file(dir / "exceptions.tsv") ==
          "id\tlabel\treasons\tnode\tbase_node\tbase_size\n"
          "hsa3725\thsa3725\tfsc\thsa3725\thsa2353\t5\n"
          "hsa5599\thsa5599\tnot_merged\thsa5599\thsa2353\t5\n");
    CHECK_THAT(read_file(dir / "merged_edges.tsv"), StartsWith("source\ttarget\ttype\tsubtypes\t"));
    CHECK(c.files.committed.size() == 3);

    // The saved network reads back as it was (02.C4, S2.26).
    const auto saved = fusion::network::read_fusion_network(dir / "merged_graph.xml", c.ctx);
    CHECK(saved->content_hash() == net.content_hash());
    REQUIRE(saved->info().merge);
    CHECK(saved->info().merge->key == out.value.base.hex());

    // Events and the three console lines (section 03, "Events", "Console").
    const auto* base_event = c.events.find("merge.base");
    REQUIRE(base_event != nullptr);
    CHECK((*base_event)["source"] == "built");
    CHECK((*base_event)["pathways"] == 2);
    CHECK((*base_event)["key"] == out.value.base.hex());
    const auto* split_event = c.events.find("merge.split");
    REQUIRE(split_event != nullptr);
    CHECK((*split_event)["exceptions"] == 2);
    CHECK((*split_event)["split_out"] == 2);
    const auto* done = c.events.find("merge.done");
    REQUIRE(done != nullptr);
    CHECK((*done)["graph"] == 1);
    CHECK((*done)["nodes"] == 6);
    CHECK((*done)["multi_gene_nodes"] == 1);
    std::vector<std::string> mff;
    for (const auto& line : c.console->lines)
        if (line.starts_with("mff: ")) mff.push_back(line);
    REQUIRE(mff.size() == 3);
    CHECK_THAT(mff[0], StartsWith("mff: 2 pathway files; base merge " + out.value.base.hex().substr(0, 8)) &&
                           ContainsSubstring("built: 3 nodes, 3 edges"));
    CHECK(mff[1] == "mff: exception set: 2 ids (FSC, 1 not merged); 2 in the network, 2 split out of family nodes");
    CHECK_THAT(mff[2], StartsWith("mff: graph 1: 6 nodes (6 genes, 0 complexes, 0 compounds), ") &&
                           ContainsSubstring("1 node holds more than one gene, the largest hsa5601 with 2"));

    const auto details = fusion::merge::step_details(out.value);
    CHECK(details["merge_key"] == out.value.base.hex());
    CHECK(details["base_merge"] == "built");
    CHECK(details["exceptions"] == 2);

    SECTION("a second mff on the same pathways reuses the base merge from memory (03.C3)") {
        Context again;
        fusion::network::StepDocs docs2;
        const auto second = merge_step(req, store, docs2, again.ctx);
        CHECK((*again.events.find("merge.base"))["source"] == "memory");
        CHECK(second.value.source == fusion::merge::BaseSource::memory);
        CHECK(second.value.network->content_hash() == net.content_hash());
        CHECK(store.size() == 1);
    }
}

TEST_CASE("mff over one KGML is a one-file merge, with a report (03.C5, S2.16)", "[unit][merge][03.C5]") {
    Context c;
    TwoPathways p(c);
    MergeRequest req;
    req.inputs = {listed(p.b)};
    req.report = "";   // report= with no name writes merged_edges.tsv
    MergeStore store;
    fusion::network::StepDocs docs;
    const auto out = merge_step(req, store, docs, c.ctx);
    CHECK(member_sets(*out.value.network) == std::vector<std::string>{"hsa3725 hsa5599 hsa5601 hsa5602", "hsa7"});
    // FuSION 1 wrote no report after a one-file mff, and the batch's `exec mv` then failed (S2.16).
    CHECK(fs::exists(c.files.step_dir() / "merged_edges.tsv"));
}

TEST_CASE("mff: an absolute report path is honoured as well, and the file is kept in the step's directory",
          "[unit][merge][S2.24][2B.3]") {
    Context c;
    TwoPathways p(c);
    MergeRequest req;
    req.inputs = {listed(p.a)};
    const fs::path elsewhere = c.scratch.dir() / "elsewhere" / "edges.tsv";
    fs::create_directories(elsewhere.parent_path());
    req.report = fusion::core::to_utf8(elsewhere);
    MergeStore store;
    fusion::network::StepDocs docs;
    merge_step(req, store, docs, c.ctx);
    CHECK(fs::exists(elsewhere));
    CHECK(fs::exists(c.files.step_dir() / "edges.tsv"));
}

TEST_CASE("mff: an empty list fails", "[unit][merge][S2.16]") {
    Context c;
    MergeStore store;
    fusion::network::StepDocs docs;
    try {
        merge_step(MergeRequest{}, store, docs, c.ctx);
        FAIL("no failure");
    } catch (const fusion::core::Failure& f) {
        CHECK(f.code() == "merge.empty_list");
    }
}

TEST_CASE("mff: a FuSION network file contributes its embedded documents, as if they were listed (03.C7)",
          "[unit][merge][03.C7]") {
    Context first;
    TwoPathways p(first);
    MergeRequest req;
    req.inputs = {listed(p.a), listed(p.b)};
    MergeStore store;
    fusion::network::StepDocs docs;
    const auto plain = merge_step(req, store, docs, first.ctx);
    const fs::path saved = first.files.step_dir() / "merged_graph.xml";

    // A later run lists the saved network and splits it with a new FSC: the result equals a fresh merge
    // of the original files with that FSC.
    Context later;
    fusion::core::Interner ids;
    const std::vector<fusion::core::GeneId> fsc = {ids.id("hsa5601")};
    MergeRequest from_saved;
    from_saved.inputs = {listed(saved, Format::fusion_network)};
    from_saved.exceptions.fscs = fsc;
    from_saved.exceptions.interner = &ids;
    MergeStore fresh_store;
    fusion::network::StepDocs later_docs;
    const auto rebuilt = merge_step(from_saved, fresh_store, later_docs, later.ctx);
    CHECK(rebuilt.value.source == fusion::merge::BaseSource::embedded);
    CHECK(rebuilt.value.base == plain.value.base);   // the key comes from the documents' records

    Context direct;
    MergeRequest original = from_saved;
    original.inputs = {listed(p.a), listed(p.b)};
    MergeStore other_store;
    fusion::network::StepDocs direct_docs;
    const auto expected = merge_step(original, other_store, direct_docs, direct.ctx);
    CHECK(dump(*rebuilt.value.network) == dump(*expected.value.network));

    SECTION("beside the files it embeds, a network file merges once") {
        Context both;
        MergeRequest mixed = original;
        mixed.inputs.push_back(listed(saved, Format::fusion_network));
        MergeStore s;
        fusion::network::StepDocs d;
        CHECK(dump(*merge_step(mixed, s, d, both.ctx).value.network) == dump(*expected.value.network));
    }
}

TEST_CASE("mff: a legacy file is read FuSION 1's way, and may not share its list with a KGML (03.C9, S2.27)",
          "[unit][merge][03.C9][S2.27]") {
    // FuSION 1's toy case: report_example_with_pathways.xml, read with `mff ... y` after start read the
    // batch's exception file. FuSION 1 printed 22 nodes (22 genes) and 25 edges for it, and wrote
    // capture/graph_reread.xml after its rgx.
    const fs::path toy = fs::path(FUSION_TEST_SOURCE_DIR) / "reference/synthetic/toy-hsa7-wni-tsv-b10";
    Context c;
    MergeRequest req;
    req.inputs = {listed(toy / "inputs/report_example_with_pathways.xml", Format::legacy)};
    req.exceptions.start_file = toy / "f1/run/exception_file.txt";
    MergeStore store;
    fusion::network::StepDocs docs;
    const auto out = merge_step(req, store, docs, c.ctx);
    const auto& net = *out.value.network;
    CHECK(net.info().legacy);
    CHECK(net.nodes().size() == 22);
    CHECK(net.edges().size() == 25);
    CHECK(fs::exists(c.files.step_dir() / "merged_graph.xml"));
    CHECK(fs::exists(c.files.step_dir() / "exceptions.tsv"));
    CHECK(fusion::merge::step_details(out.value).contains("merge_key") == false);

    // Every edge of FuSION 1's re-read graph is in 2.0's, with its subtypes and pathways.
    const std::string reread = read_file(toy / "f1/capture/graph_reread.xml");
    const std::regex relation(R"re(<relation entry1="(\d+)" entry2="(\d+)" type="(\w+)" pathways="([^"]*)"[^>]*>\s*<subtype name="(\w+)"/>)re");
    const std::regex entry(R"re(<entry id="(\d+)" name="(\w+)")re");
    std::map<std::string, std::string> names;
    for (auto it = std::sregex_iterator(reread.begin(), reread.end(), entry); it != std::sregex_iterator(); ++it)
        names[(*it)[1]] = (*it)[2];
    int edges = 0;
    for (auto it = std::sregex_iterator(reread.begin(), reread.end(), relation); it != std::sregex_iterator(); ++it) {
        ++edges;
        const auto* e = edge(net, names[(*it)[1]], names[(*it)[2]], (*it)[3], {(*it)[5]});
        REQUIRE(e != nullptr);
        CHECK(pathways(net, *e) == std::vector<std::string>{(*it)[4]});
    }
    CHECK(edges == 25);

    SECTION("beside a KGML it is merge.legacy_mixed") {
        Context mixed;
        TwoPathways p(mixed);
        MergeRequest bad = req;
        bad.inputs.push_back(listed(p.a));
        MergeStore s;
        fusion::network::StepDocs d;
        try {
            merge_step(bad, s, d, mixed.ctx);
            FAIL("no failure");
        } catch (const fusion::core::Failure& f) {
            CHECK(f.code() == "merge.legacy_mixed");
        }
    }
}

TEST_CASE("mff: beside the legacy master, an edge table and add's output bind by first claim (03.C9)",
          "[unit][merge][03.C9][S8.24]") {
    const fs::path toy = fs::path(FUSION_TEST_SOURCE_DIR) / "reference/synthetic/toy-hsa7-wni-tsv-b10";
    Context c;
    const fs::path table = c.scratch.write("extra_edges.tsv",
                                           "source\ttarget\ttype\tsubtypes\n"
                                           "hsa1\thsa99\tPPrel\tactivation\n"
                                           "hsa5\thsa22\tPPrel\tinhibition\n");
    // add's output: FuSION's mark, one gene entry per id, pathways="KEGG" (02.C17).
    const fs::path added = c.scratch.dir() / "additional_edges.xml";
    {
        const fs::path four = c.scratch.write("additional_edges.txt", "hsa2 hsa100 PPrel activation\n");
        std::ofstream out(added, std::ios::binary);
        fusion::network::add_edges(four, out, fusion::network::IdMap::none(), c.ctx);
    }
    MergeRequest req;
    req.inputs = {listed(toy / "inputs/report_example_with_pathways.xml", Format::legacy),
                  listed(table, Format::edge_table), listed(added, Format::fusion_network)};
    MergeStore store;
    fusion::network::StepDocs docs;
    const auto out = merge_step(req, store, docs, c.ctx);
    const auto& net = *out.value.network;
    CHECK(net.info().legacy);
    CHECK(net.nodes().size() == 24);                     // the toy's 22, and hsa99 and hsa100, new
    CHECK(net.edges().size() == 28);
    const auto* extra = edge(net, "hsa1", "hsa99", "PPrel", {"activation"});
    REQUIRE(extra != nullptr);
    CHECK(pathways(net, *extra) == std::vector<std::string>{"extra_edges"});   // the table's stem (02.C2)
    CHECK(edge(net, "hsa5", "hsa22", "PPrel", {"inhibition"}) != nullptr);
    const auto* add = edge(net, "hsa2", "hsa100", "PPrel", {"activation"});
    REQUIRE(add != nullptr);
    CHECK(pathways(net, *add) == std::vector<std::string>{"KEGG"});
    // The legacy graph's own edges stand as they were.
    CHECK(edge(net, "hsa1", "hsa2", "PPrel", {"activation"}) != nullptr);
}

TEST_CASE("mff: a network file without embedded sources cannot be split exactly (03.C7)", "[unit][merge][03.C7]") {
    Context c;
    TwoPathways p(c);
    MergeRequest req;
    req.inputs = {listed(p.a)};
    MergeStore store;
    fusion::network::StepDocs docs;
    const auto first = merge_step(req, store, docs, c.ctx);
    const fs::path bare = c.scratch.dir() / "bare.xml";
    {
        std::ofstream out(bare, std::ios::binary);
        fusion::network::WriteOptions options;
        options.embed_sources = false;
        fusion::network::write_fusion_network(fusion::network::GraphView(first.value.network), out,
                                              fusion::network::Maps{}, options);
    }
    MergeRequest from_bare;
    from_bare.inputs = {listed(bare, Format::fusion_network)};
    try {
        merge_step(from_bare, store, docs, c.ctx);
        FAIL("no failure");
    } catch (const fusion::core::Failure& f) {
        CHECK(f.code() == "merge.no_sources");
    }
}
