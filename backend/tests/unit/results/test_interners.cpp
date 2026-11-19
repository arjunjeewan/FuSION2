// results: genes are matched by text across interners. A network keeps an interner of its own, so the
// run's observations, values and match (interned by the batch) must not be read with the network's.
// Integration follow-up, prompt log entry 308: an observed gene relaxed in a witness was written as
// not_diff_expr through the batch.
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "fixture.hpp"
#include "fusion/corridor/corridor.hpp"
#include "fusion/expression/expression.hpp"
#include "fusion/network/network.hpp"
#include "fusion/results/json.hpp"
#include "fusion/results/legacy.hpp"

using namespace fusion;
using namespace fusion::results;
using fx::pt;
using Catch::Matchers::ContainsSubstring;

TEST_CASE("an observed gene in a witness keeps its class and value whatever the interners", "[unit][results]") {
  fx::Step step("pathz3");
  auto ctx = step.context();
  // The network, built through an interner of its own, in its own order.
  core::Interner net_strings;
  network::NetworkBuilder builder(network::NetworkInfo{}, net_strings);
  const auto gene = net_strings.sym("gene");
  const auto node = [&](std::vector<std::string> ids) {
    std::vector<core::GeneId> members;
    for (const auto& id : ids) members.push_back(net_strings.id(id));
    return builder.add_node(gene, members);
  };
  const auto x = node({"hsa6"}), t = node({"hsa5"}), f = node({"hsa3", "hsa4"}), m = node({"hsa2"}), s = node({"hsa1"});
  const auto edge = [&](network::NodeIdx a, network::NodeIdx b) {
    const std::vector<core::Sym> sub{net_strings.sym("activation")};
    builder.add_edge(a, b, net_strings.sym("PPrel"), sub, std::vector<core::Sym>{}, std::nullopt);
  };
  edge(s, m);
  edge(m, f);
  edge(f, t);
  edge(s, x);
  edge(x, t);
  const auto net = std::make_shared<const network::Network>(std::move(builder).finish(ctx));
  const network::Maps maps;
  network::BindingTable table(net, maps);
  corridor::Request request;
  request.source = {"hsa1"};
  request.target = {"hsa5"};
  request.bound = 3;
  const auto cut = corridor::compute(net, table, request, ctx);
  REQUIRE(cut.value);

  // The run's interner holds other ids first, so its handles differ from the network's.
  core::Interner run;
  for (int i = 0; i < 50; ++i) run.id("zz" + std::to_string(i));
  const auto values = expression::parse_values("hsa3\t1.5\nhsa4\t-0.25\nhsa5\t2\nhsa6\t2.5\nhsa9\t3\n",
                                               expression::ValueInput{}, network::IdMap::none(), false, run, ctx);
  expression::ClassifyRequest classify;
  classify.thresholds = {1.0, -1.0};
  const auto observations = expression::classify(values, classify, ctx);
  REQUIRE_FALSE(observations.report);
  const auto match = expression::match_observations(observations.value, cut.value->view());

  // hsa6, up-regulated, is relaxed and absent in the witness, as DUSP1 and SGK1 were (entry 308).
  fx::W w{{"hsa1", "hsa2", "hsa3", "hsa5"}, {}, {fx::ek("hsa1", "hsa2", {"activation"}), fx::ek("hsa2", "hsa3", {"activation"}),
                                                fx::ek("hsa3", "hsa5", {"activation"})}, {{"hsa6", false}}, {}};
  solve::Results results;
  results.cases.push_back(fx::frontier_case(solve::Case::free, {w}));
  results.cases[0].observations = {solve::ObservationRow{"hsa3", "hsa3", 'u', solve::Role::counted},
                                   solve::ObservationRow{"hsa5", "hsa5", 'u', solve::Role::exempt_endpoint},
                                   solve::ObservationRow{"hsa6", "hsa6", 'u', solve::Role::counted}};
  solve::Inputs inputs{*cut.value, observations.value, match, table, {}, {}, nullptr, &maps, std::nullopt};
  const solve::Settings settings;
  const auto by_key = [](const SolveResult& r) {
    std::map<std::string, const NodeRecord*> out;
    for (const auto& n : r.tables.nodes) out[n.key.value] = &n;
    return out;
  };

  SECTION("with the run's interner: classes, values and observation nodes by text") {
    const PublishInputs in{results, inputs, *cut.value, match, maps, "solve", settings, &values, "", &run};
    SolveResult r = build(in);
    REQUIRE(analyse(r).empty());
    const auto nodes = by_key(r);
    CHECK(nodes.at("hsa6")->observed == Observed::up);
    CHECK(nodes.at("hsa6")->members.at(0).lfc == 2.5);
    CHECK(nodes.at("hsa3")->observed == Observed::up);
    CHECK(nodes.at("hsa3")->members.at(0).observed == Observed::up);
    CHECK(nodes.at("hsa3")->members.at(0).lfc == 1.5);
    CHECK(nodes.at("hsa3")->members.at(1).observed == Observed::none);
    CHECK(nodes.at("hsa3")->members.at(1).lfc == -0.25);
    CHECK(nodes.at("hsa1")->members.at(0).lfc == std::nullopt);
    CHECK_THAT(legacy_relaxations(r, Case::free), ContainsSubstring("hsa6\thsa6\thsa6\tup_reg\tnot_present\tNA\n"));
    CHECK_THAT(legacy_solutions(r, Case::free), ContainsSubstring("hsa3\thsa3\thsa3\tup_reg\tpresent\tactive\n"));
    const auto json = core::Json::parse(to_json(r, {}, std::nullopt));
    for (const auto& n : json["nodes"]) {
      if (n["key"] == "hsa6") CHECK(n["observed"] == "up");
    }
    const auto hsa6 = std::find_if(r.cases[0].observations.begin(), r.cases[0].observations.end(),
                                   [](const Observation& o) { return o.gene == "hsa6"; });
    REQUIRE(hsa6 != r.cases[0].observations.end());
    REQUIRE(hsa6->node.has_value());
    CHECK(r.tables.nodes[*hsa6->node].key.value == "hsa6");
  }

  SECTION("without it: the class the solve used, by node key from 06's Problem, and no guessed value") {
    auto problem = std::make_shared<solve::Problem>();
    for (const char* k : {"hsa1", "hsa2", "hsa3", "hsa5", "hsa6"}) problem->nodes.push_back(network::NodeKey{k});
    problem->regulation = {expression::Regulation::none, expression::Regulation::none, expression::Regulation::up,
                           expression::Regulation::up, expression::Regulation::up};
    results.cases[0].problem = problem;
    results.cases[0].observations.clear();
    const PublishInputs in{results, inputs, *cut.value, match, maps, "solve", settings, &values, ""};
    SolveResult r = build(in);
    const auto nodes = by_key(r);
    CHECK(nodes.at("hsa6")->observed == Observed::up);
    CHECK(nodes.at("hsa6")->members.at(0).observed == Observed::up);
    CHECK(nodes.at("hsa6")->members.at(0).lfc == std::nullopt);
    CHECK(nodes.at("hsa2")->observed == Observed::none);
    CHECK_THAT(legacy_relaxations(r, Case::free), ContainsSubstring("hsa6\thsa6\thsa6\tup_reg\tnot_present\tNA\n"));
  }
}
