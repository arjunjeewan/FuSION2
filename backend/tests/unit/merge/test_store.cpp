// Reuse (03.C3): the interpreter's MergeStore, its memory tier, and base merges from a saved network's
// embedded documents (2A.4, 03.C7); a stop request between documents.
#include <catch2/catch_test_macros.hpp>

#include "support.hpp"

using namespace merge_test;
using fusion::merge::BaseSource;
using fusion::merge::MergeStore;
using fusion::network::BuildSettings;
using fusion::network::IdMap;

namespace {

fusion::network::SourceRef input(const fs::path& path, std::string pathway) {
    fusion::network::SourceRef s;
    s.path = path;
    s.size = fs::file_size(path);
    s.sha256 = fusion::core::Sha256::of_file(path);
    s.format = fusion::network::Format::kgml;
    s.pathway = std::move(pathway);
    return s;
}

}  // namespace

TEST_CASE("the store builds each key once; later steps, in any list order, get it from memory",
          "[unit][merge][03.C3]") {
    Context c;
    const auto a = c.scratch.write("kegg/hsa00601.xml",
                                   Kgml("path:hsa00601").entry(1, "hsa:1 hsa:2").entry(2, "hsa:3").rel(1, 2, {"activation"}).text());
    const auto b = c.scratch.write("kegg/hsa00602.xml",
                                   Kgml("path:hsa00602").entry(1, "hsa:2 hsa:4").entry(2, "hsa:3").rel(1, 2, {"inhibition"}).text());
    const std::vector<fusion::network::SourceRef> inputs = {input(a, "path:hsa00601"), input(b, "path:hsa00602")};
    const std::vector<fusion::network::SourceRef> reversed = {inputs[1], inputs[0]};
    MergeStore store;
    fusion::network::StepDocs docs;
    const auto first = store.get_or_build(inputs, BuildSettings{}, IdMap::none(), docs, c.ctx);
    CHECK(first.source == BaseSource::built);
    CHECK(first.merge->key() == fusion::merge::merge_key(inputs, BuildSettings{}, IdMap::none()));
    const auto again = store.get_or_build(reversed, BuildSettings{}, IdMap::none(), docs, c.ctx);
    CHECK(again.source == BaseSource::memory);
    CHECK(again.merge == first.merge);
    // The documents' own records give the same key as the run's records of its inputs.
    const auto by_docs = store.get_or_build(std::vector<DocPtr>(first.merge->docs().begin(), first.merge->docs().end()),
                                            BuildSettings{}, c.ctx);
    CHECK(by_docs.source == BaseSource::memory);
    BuildSettings two;
    two.split_threshold = 2;
    CHECK(store.get_or_build(inputs, two, IdMap::none(), docs, c.ctx).source == BaseSource::built);
    CHECK(store.size() == 2);
}

TEST_CASE("from_embedded: a saved network's documents give its base merge, and splitting it equals a fresh merge",
          "[unit][merge][03.C7][2A.4]") {
    Context c;
    const std::vector<DocPtr> docs = {
        read(c, Kgml("path:hsa00701").entry(1, "hsa:1 hsa:2 hsa:3").entry(2, "hsa:9").rel(1, 2, {"activation"}, "PPrel", "40")),
        read(c, Kgml("path:hsa00702").entry(1, "hsa:3 hsa:4").entry(2, "hsa:9").rel(2, 1, {"binding/association"}))};
    MergeStore store;
    const auto hit = store.get_or_build(docs, BuildSettings{}, c.ctx);
    const auto saved_path = c.scratch.dir() / "saved.xml";
    {
        std::ofstream out(saved_path, std::ios::binary);
        fusion::network::write_fusion_network(fusion::network::GraphView(hit.merge->network()), out,
                                              fusion::network::Maps{}, fusion::network::WriteOptions{});
    }
    const auto saved = fusion::network::read_fusion_network(saved_path, c.ctx);
    MergeStore fresh;
    const auto rebuilt = fresh.from_embedded(*saved, BuildSettings{}, c.ctx);
    CHECK(rebuilt.source == BaseSource::embedded);
    CHECK(rebuilt.merge->key() == hit.merge->key());
    CHECK(fresh.from_embedded(*saved, BuildSettings{}, c.ctx).source == BaseSource::memory);
    const auto e = exceptions({"hsa3"});
    CHECK(dump(*fusion::merge::split(*rebuilt.merge, e).network) ==
          dump(*fusion::merge::merge_direct(docs, BuildSettings{}, e)));

    SECTION("a saved network without sources cannot be rebuilt") {
        const auto bare_path = c.scratch.dir() / "bare.xml";
        {
            std::ofstream out(bare_path, std::ios::binary);
            fusion::network::WriteOptions options;
            options.embed_sources = false;
            fusion::network::write_fusion_network(fusion::network::GraphView(hit.merge->network()), out,
                                                  fusion::network::Maps{}, options);
        }
        const auto bare = fusion::network::read_fusion_network(bare_path, c.ctx);
        try {
            fresh.from_embedded(*bare, BuildSettings{}, c.ctx);
            FAIL("no failure");
        } catch (const fusion::core::Failure& f) {
            CHECK(f.code() == "merge.no_sources");
        }
    }
}

TEST_CASE("a stop request is honoured between documents", "[unit][merge]") {
    Context c;
    const std::vector<DocPtr> docs = {read(c, Kgml("path:hsa00801").entry(1, "hsa:1").entry(2, "hsa:2").rel(1, 2, {"activation"}))};
    c.stop.request_stop();
    CHECK_THROWS_AS(fusion::merge::build_base_merge(docs, BuildSettings{}, fusion::merge::MergeKey{}, c.ctx),
                    fusion::core::Stopped);
}
