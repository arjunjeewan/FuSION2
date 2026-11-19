// Observed genes against the network and the corridor (section 04, "The match report"; S3.12,
// S4.16, S2.03; 05.C9). FuSION 1 dropped every list entry the solved graph lacked without a word.
#include <catch2/catch_test_macros.hpp>

#include "support.hpp"

using namespace fusion;
using namespace fusion::expression;
using namespace fusion::expression::test;

namespace {

ClassifyRequest request(double up, double down) {
    ClassifyRequest r;
    r.thresholds = Thresholds{up, down};
    return r;
}

// hsa1 alone, the family node {hsa2, hsa3} (key hsa2), and hsa4 alone; hsa1 -> family.
std::shared_ptr<const network::Network> toy_network(Harness& h) {
    core::Interner& in = h.interner;
    network::NetworkBuilder builder(network::NetworkInfo{}, in);
    const core::Sym gene = in.sym("gene");
    const std::vector<core::GeneId> one{in.id("hsa1")}, family{in.id("hsa3"), in.id("hsa2")}, four{in.id("hsa4")};
    const network::NodeIdx a = builder.add_node(gene, one);
    const network::NodeIdx b = builder.add_node(gene, family);
    builder.add_node(gene, four);
    builder.add_edge(a, b, in.sym("PPrel"), {}, {}, std::nullopt);
    return std::make_shared<const network::Network>(std::move(builder).finish(h.ctx));
}

}  // namespace

TEST_CASE("expression: each observed gene is in the corridor, outside it, or not in the network (S3.12, S4.16)",
          "[unit][expression]") {
    Harness h("fb_rch");
    const auto net = toy_network(h);
    network::Bitset nodes(net->nodes().size()), edges(net->edges().size());
    nodes.set(*net->find_node(network::NodeKey{"hsa1"}));
    nodes.set(*net->find_node(network::NodeKey{"hsa2"}));
    for (std::size_t e = 0; e < net->edges().size(); ++e) edges.set(e);
    const network::GraphView corridor(net, nodes, edges);

    const GeneValues values = parse(h, "hsa1\t2\nhsa2\t-2\nhsa3\t3\nhsa4\t1.5\nhsa8\t-3\nhsa9\t2\n");
    const Observations obs = classify(values, request(1, -1), h.ctx).value;
    const std::vector<ObservationMatch> matches = match_observations(obs, corridor);

    struct Expected {
        std::string id;
        Regulation regulation;
        MatchGroup group;
        std::string node;
        std::uint32_t members;
    };
    // By group, up before down, then input order.
    const std::vector<Expected> expected{
        {"hsa9", Regulation::up, MatchGroup::not_in_network, "", 0},
        {"hsa8", Regulation::down, MatchGroup::not_in_network, "", 0},
        {"hsa4", Regulation::up, MatchGroup::outside_corridor, "hsa4", 1},
        {"hsa1", Regulation::up, MatchGroup::in_corridor, "hsa1", 1},
        {"hsa3", Regulation::up, MatchGroup::in_corridor, "hsa2", 2},   // binds to its family node (S2.03)
        {"hsa2", Regulation::down, MatchGroup::in_corridor, "hsa2", 2}};
    REQUIRE(matches.size() == expected.size());
    for (std::size_t i = 0; i < expected.size(); ++i) {
        INFO("row " << i);
        CHECK(h.interner.str(matches[i].id) == expected[i].id);
        CHECK(matches[i].regulation == expected[i].regulation);
        CHECK(matches[i].group == expected[i].group);
        CHECK(matches[i].node.value_or(network::NodeKey{}).value == expected[i].node);
        CHECK(matches[i].node.has_value() == !expected[i].node.empty());
        CHECK(matches[i].members == expected[i].members);
    }

    // observations_match.tsv (05.C9), with symbols and the values as read.
    const fs::path symbols_file = h.run_dir() / "symbols.txt";
    spit(symbols_file, "hsa1\tONE\nhsa3\tTHREE\n");
    const network::LabelMap symbols = network::LabelMap::read(symbols_file, h.ctx);
    save_match(matches, &values, symbols, h.interner, h.ctx);
    CHECK(slurp(h.step_dir() / "observations_match.tsv") ==
          "id\tsymbol\tregulation\tvalue\tgroup\tnode\tmembers\n"
          "hsa9\t\tup\t2.0\tnot_in_network\t\t0\n"
          "hsa8\t\tdown\t-3.0\tnot_in_network\t\t0\n"
          "hsa4\t\tup\t1.5\toutside_corridor\thsa4\t1\n"
          "hsa1\tONE\tup\t2.0\tin_corridor\thsa1\t1\n"
          "hsa3\tTHREE\tup\t3.0\tin_corridor\thsa2\t2\n"
          "hsa2\t\tdown\t-2.0\tin_corridor\thsa2\t2\n");
    const std::vector<std::string> infos = h.sink->infos();
    REQUIRE_FALSE(infos.empty());
    CHECK(infos.back() == "fb_rch: observed genes: 3 in the corridor, 1 outside it, 2 not in the network");
    const auto events = h.events.of("expression.matched");
    REQUIRE(events.size() == 1);
    CHECK(events[0] == core::Json::parse(
                           R"({"n_observed": 6, "n_not_in_network": 2, "n_outside_corridor": 1, "n_in_corridor": 3})"));
}

TEST_CASE("expression: matched against the whole network, only absent genes are left out", "[unit][expression]") {
    Harness h("fb_rch");
    const auto net = toy_network(h);
    const network::GraphView whole(net);
    // Lists loaded from files, as a FuSION 1 pathz3 line names them.
    spit(h.run_dir() / "up", "hsa4\nhsa77\n");
    spit(h.run_dir() / "down", "hsa:3\n");
    const Observations obs = load_lists(h.run_dir() / "up", h.run_dir() / "down", network::IdMap::none(), h.interner, h.ctx);
    const std::vector<ObservationMatch> matches = match_observations(obs, whole);
    REQUIRE(matches.size() == 3);
    CHECK(h.interner.str(matches[0].id) == "hsa77");
    CHECK(matches[0].group == MatchGroup::not_in_network);
    CHECK(h.interner.str(matches[1].id) == "hsa4");
    CHECK(matches[1].group == MatchGroup::in_corridor);
    CHECK(h.interner.str(matches[2].id) == "hsa3");
    CHECK(matches[2].group == MatchGroup::in_corridor);
    CHECK(matches[2].regulation == Regulation::down);
    // Without values the value column stays empty.
    save_match(matches, nullptr, network::LabelMap(), h.interner, h.ctx);
    CHECK(slurp(h.step_dir() / "observations_match.tsv") ==
          "id\tsymbol\tregulation\tvalue\tgroup\tnode\tmembers\n"
          "hsa77\t\tup\t\tnot_in_network\t\t0\n"
          "hsa4\t\tup\t\tin_corridor\thsa4\t1\n"
          "hsa3\t\tdown\t\tin_corridor\thsa2\t2\n");
}
