// results: build(PublishInputs), the path a real solve takes, on a synthetic network built through 02's
// NetworkBuilder and a corridor from 05's compute(): labels with family size (07.C10), members with the
// values the solve used (4A.13), corridor distances (S4.07), pathway names and the case as 06 encoded it.
#include <catch2/catch_test_macros.hpp>

#include "fixture.hpp"
#include "fusion/corridor/corridor.hpp"
#include "fusion/expression/expression.hpp"
#include "fusion/network/network.hpp"

using namespace fusion;
using namespace fusion::results;
using fx::pt;

TEST_CASE("build(PublishInputs) reads labels, members, values and distances from the run", "[unit][results]") {
  fx::Step step("pathz3");
  auto ctx = step.context();
  core::Interner strings;
  network::NetworkBuilder builder(network::NetworkInfo{}, strings);
  const auto gene = strings.sym("gene");
  const auto node = [&](std::vector<std::string> ids) {
    std::vector<core::GeneId> members;
    for (const auto& id : ids) members.push_back(strings.id(id));
    return builder.add_node(gene, members);
  };
  const auto s = node({"hsa1"}), m = node({"hsa2"}), f = node({"hsa3", "hsa4"}), t = node({"hsa5"});
  const auto edge = [&](network::NodeIdx a, network::NodeIdx b, std::string subtype, double score) {
    const std::vector<core::Sym> subtypes{strings.sym(subtype)};
    const std::vector<core::Sym> pathways{strings.sym("path:hsa04010")};
    builder.add_edge(a, b, strings.sym("PPrel"), subtypes, pathways, score);
  };
  edge(s, m, "activation", 900);
  edge(m, f, "activation", 500);
  edge(f, t, "expression", 950);
  const auto net = std::make_shared<const network::Network>(std::move(builder).finish(ctx));
  const network::Maps maps;
  network::BindingTable table(net, maps);
  corridor::Request request;
  request.source = {"hsa1"};
  request.target = {"hsa5"};
  request.bound = 3;
  const auto cut = corridor::compute(net, table, request, ctx);
  REQUIRE(cut.value);
  REQUIRE_FALSE(cut.report);

  // The values the solve's observations came from (04.C9), read by 04's one reader.
  const auto values = expression::parse_values("hsa3\t1.5\nhsa4\t-0.25\nhsa5\t2\n", expression::ValueInput{},
                                               network::IdMap::none(), false, strings, ctx);
  const std::vector<expression::ObservationMatch> match{
      {*strings.find_id("hsa3"), expression::Regulation::up, expression::MatchGroup::in_corridor, network::NodeKey{"hsa3"}, 2},
      {*strings.find_id("hsa5"), expression::Regulation::up, expression::MatchGroup::in_corridor, network::NodeKey{"hsa5"}, 1}};
  const expression::Observations observations;
  solve::Inputs inputs{*cut.value, observations, match, table, {}, {}, nullptr, &maps, std::nullopt};
  solve::Settings settings;
  settings.fsc = {"hsa3"};
  settings.seed = 7;

  fx::W w{{"hsa1", "hsa2", "hsa3", "hsa5"}, {}, {fx::ek("hsa1", "hsa2", {"activation"}), fx::ek("hsa2", "hsa3", {"activation"}),
                                                fx::ek("hsa3", "hsa5", {"expression"})}, {}, {}};
  solve::Results results;
  results.cases.push_back(fx::frontier_case(solve::Case::in, {w}));
  results.cases[0].observations = {solve::ObservationRow{"hsa3", "hsa3", 'u', solve::Role::forced},
                                   solve::ObservationRow{"hsa5", "hsa5", 'u', solve::Role::exempt_endpoint}};
  auto problem = std::make_shared<solve::Problem>();
  problem->fsc = {"hsa3"};
  problem->requested.hi = pt(8, 8);
  problem->box.hi = pt(1, 3);
  problem->families.frozen = true;
  problem->families.frozen_threshold = 800;
  results.cases[0].problem = problem;

  const PublishInputs in{results, inputs, *cut.value, match, maps, "solve", settings, &values, "fb_rch/corridor.json",
                         &strings};
  SolveResult r = build(in);
  REQUIRE(analyse(r).empty());

  std::map<std::string, const NodeRecord*> by_key;
  for (const auto& n : r.tables.nodes) by_key[n.key.value] = &n;
  REQUIRE(by_key.size() == 4);
  const NodeRecord& family = *by_key.at("hsa3");
  CHECK(family.label == "hsa3+1");                      // 07.C10: the rep's symbol (here its id) plus one
  REQUIRE(family.members.size() == 2);
  CHECK(family.members[0].id == "hsa3");
  CHECK(family.members[0].lfc == 1.5);
  CHECK(family.members[0].observed == Observed::up);
  CHECK(family.members[1].lfc == -0.25);
  CHECK(family.members[1].observed == Observed::none);
  CHECK(family.observed == Observed::up);
  CHECK(by_key.at("hsa1")->from_source == 0);
  CHECK(by_key.at("hsa1")->to_target == 3);
  CHECK(family.from_source == 2);
  CHECK(family.to_target == 1);
  CHECK(by_key.at("hsa1")->label == "hsa1");

  REQUIRE(r.tables.edges.size() == 3);
  for (const auto& e : r.tables.edges) CHECK(e.pathways == std::vector<std::string>{"path:hsa04010"});
  // Frozen when the solve used the family and the score is strictly above its threshold (S5.16).
  CHECK(r.tables.edges[0].key.text() == "hsa1|hsa2|PPrel|activation");
  CHECK(r.tables.edges[0].frozen);
  CHECK_FALSE(r.tables.edges[1].frozen);   // hsa2 -> hsa3, score 500
  CHECK(r.tables.pathway_names.at("path:hsa04010") == "path:hsa04010");

  CHECK(r.header.name == "solve");
  CHECK(r.tables.nodes[r.header.source].key.value == "hsa1");
  REQUIRE(r.header.fsc.size() == 1);
  CHECK(r.tables.nodes[r.header.fsc[0]].key.value == "hsa3");
  CHECK(r.header.seed == 7);
  CHECK(r.header.bound == 3);
  CHECK_FALSE(r.header.coexpression_threshold.has_value());
  CHECK(r.header.frozen_threshold == 800);
  CHECK(r.header.corridor.nodes == 4);
  CHECK(r.header.corridor.edges == 3);
  CHECK(r.header.corridor.artefact == "fb_rch/corridor.json");
  CHECK(r.cases[0].label == "with FSC (hsa3+1)");
  CHECK(r.cases[0].searched == pt(1, 3));
  CHECK(r.cases[0].observations.size() == 2);
  CHECK(r.cases[0].observations[0].node.has_value());
}
