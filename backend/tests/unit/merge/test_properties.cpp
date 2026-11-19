// Properties of the union on random synthetic sets (section 12's merge row): order independence over
// permutations of the list and of each file (S2.17), each id in exactly one node (stage 2 open question
// 4), no isolated node, and a file listed twice changing nothing.
#include <map>

#include <catch2/catch_test_macros.hpp>

#include "generator.hpp"
#include "support.hpp"

using namespace merge_test;
using fusion::merge::ExceptionSet;
using fusion::merge::merge_direct;
using fusion::network::BuildSettings;

TEST_CASE("the merge does not depend on the order of the list or the order inside a file", "[unit][merge][S2.17]") {
    Context c;
    auto rng = fusion::core::Rng::stream(2026, 1);
    for (int round = 0; round < 40; ++round) {
        const auto set = generate(rng, 10, 4);
        BuildSettings s;
        s.split_threshold = std::vector<int>{0, 1, 2, 15000}[rng.below(4)];
        ExceptionSet e;
        for (const auto& id : set.ids)
            if (rng.below(5) == 0) e.add(std::string_view(id), fusion::merge::Reason::up);
        const auto docs = read_all(c, set.pathways, "o" + std::to_string(round));
        const auto reference = merge_direct(docs, s, e);
        for (int p = 0; p < 5; ++p) {
            std::vector<Kgml> shuffled;
            for (const auto& k : set.pathways) shuffled.push_back(k.shuffled(rng));
            rng.shuffle(shuffled.begin(), shuffled.end());
            const auto again = merge_direct(read_all(c, shuffled, "o" + std::to_string(round) + "p" + std::to_string(p)), s, e);
            REQUIRE(dump(*again) == dump(*reference));
            REQUIRE(again->content_hash() == reference->content_hash());
        }
    }
}

TEST_CASE("each id is in exactly one node, and no node is isolated", "[unit][merge][S2.03][S2.17]") {
    Context c;
    auto rng = fusion::core::Rng::stream(2026, 2);
    for (int round = 0; round < 80; ++round) {
        const auto set = generate(rng, 14, 4);
        BuildSettings s;
        s.split_threshold = std::vector<int>{0, 1, 2, 3, 15000}[rng.below(5)];
        ExceptionSet e;
        for (const auto& id : set.ids)
            if (rng.below(4) == 0) e.add(std::string_view(id), fusion::merge::Reason::down);
        const auto net = merge_direct(read_all(c, set.pathways, "n" + std::to_string(round)), s, e);
        std::map<std::string, int> seen;
        for (fusion::network::NodeIdx n = 0; n < net->nodes().size(); ++n) {
            REQUIRE(net->out_edges(n).size() + net->in_edges(n).size() > 0);
            if (net->kind(n) == fusion::network::NodeKind::complex) continue;
            for (const auto m : net->members(n)) {
                ++seen[std::string(net->interner().str(m))];
                REQUIRE(net->ids().holders(m).size() == 1);
            }
            // An exception id never shares a node (S2.13), and every single-id node of E is marked.
            const bool single = net->members(n).size() == 1;
            for (const auto m : net->members(n))
                if (e.contains(net->interner().str(m))) REQUIRE(single);
            if (single) REQUIRE(has(net->nodes()[n].flags, fusion::network::NodeFlag::exception) ==
                                e.contains(net->interner().str(net->members(n)[0])));
        }
        for (const auto& [id, count] : seen) REQUIRE(count == 1);
    }
}

TEST_CASE("listing a file twice changes nothing", "[unit][merge][03.C6]") {
    Context c;
    auto rng = fusion::core::Rng::stream(2026, 4);
    for (int round = 0; round < 10; ++round) {
        const auto set = generate(rng, 10, 3);
        auto docs = read_all(c, set.pathways, "t" + std::to_string(round));
        const auto once = merge_direct(docs, BuildSettings{}, ExceptionSet{});
        docs.push_back(docs.front());
        CHECK(dump(*merge_direct(docs, BuildSettings{}, ExceptionSet{})) == dump(*once));
    }
}
