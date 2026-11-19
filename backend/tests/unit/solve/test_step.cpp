// The whole step on 2.0's own pieces: each toy network built through 02's NetworkBuilder, its
// corridor cut by 05's compute(), its observations read by 04's load_lists(), and solve() binding
// the lists (1.22) and solving both FSC cases or a positional line's free case (06, S5.01, S5.17).
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <map>
#include <set>
#include <sstream>

#include "fusion/corridor/corridor.hpp"
#include "kit.hpp"
#include "step.hpp"

using namespace solvekit;
namespace network = fusion::network;
namespace expression = fusion::expression;
namespace corridor = fusion::corridor;
using solve::Case;
using solve::CaseStatus;

namespace {

// One toy's network and corridor in 2.0, with the run's strings, maps and bindings.
struct ToyRunIn20 {
  // shifted: the run's interner holds an unrelated id first, so its handles differ from the network's.
  ToyRunIn20(const ToyCase& toy, core::Context& ctx, bool shifted = false)
      : bindings(make_network(toy, ctx, shifted), maps) {
    corridor::Request request;
    request.source = {"hsa1"};
    request.target = {"hsa22"};
    request.bound = toy.graph.bound;
    auto cut = corridor::compute(net, bindings, request, ctx);
    REQUIRE_FALSE(cut.report);
    path = cut.value;
    const auto run = tests_dir() / "reference" / "synthetic" / toy.name / "f1" / "run";
    observations = expression::load_lists(run / "up_reg", run / "down_reg", network::IdMap::none(), strings, ctx);
    match = expression::match_observations(observations, path->view());
  }

  std::shared_ptr<const network::Network> make_network(const ToyCase& toy, core::Context& ctx, bool shifted) {
    if (shifted) strings.id("unrelated:1");
    network::NetworkBuilder builder(network::NetworkInfo{}, strings);
    std::map<std::string, network::NodeIdx> index;
    for (const solve::CaseGraph::Node& n : toy.graph.nodes) {
      const core::GeneId id = strings.id(n.key.value);
      index[n.key.value] = builder.add_node(strings.sym("gene"), std::span<const core::GeneId>(&id, 1));
    }
    for (const solve::CaseGraph::Edge& e : toy.graph.edges) {
      std::vector<core::Sym> subtypes;
      for (const std::string& s : e.key.subtypes) subtypes.push_back(strings.sym(s));
      const core::Sym pathway = strings.sym("path:toy");
      builder.add_edge(index.at(e.key.source.value), index.at(e.key.target.value), strings.sym(e.key.type), subtypes,
                       std::span<const core::Sym>(&pathway, 1), e.score);
    }
    net = std::make_shared<const network::Network>(std::move(builder).finish(ctx));
    return net;
  }

  solve::Inputs inputs(solve::NodeLists nodes = {}) {
    return solve::Inputs{*path, observations, match, bindings, std::move(nodes), {}, nullptr, &maps, std::nullopt,
                         &strings};
  }

  core::Interner strings;
  std::shared_ptr<const network::Network> net;
  network::Maps maps;
  network::BindingTable bindings;
  std::shared_ptr<const corridor::Corridor> path;
  expression::Observations observations;
  std::vector<expression::ObservationMatch> match;
};

std::string bindings_tsv(const network::BindingTable& table) {
  std::ostringstream out;
  table.write(out);
  return out.str();
}

}  // namespace

TEST_CASE("solve: solve() on 2.0's corridor of the toy networks: both FSC cases", "[unit][solve][S5.17][06.C10][S4.20]") {
  for (const char* name : {"toy-hsa5-wni-tsv-b10", "toy-hsa15-wni-tsv-b10", "toy-hsa13-wni-tsv-b15"}) {
    INFO(name);
    const ToyCase toy = read_toy(name);
    Step step;
    core::Context ctx = step.context();
    ToyRunIn20 run(toy, ctx);
    const auto digest = run.path->digest();
    solve::Settings settings;
    settings.fsc = {toy.fsc};
    settings.box = toy.box;
    const auto outcome = solve::solve(run.inputs(), settings, ctx);
    const solve::Results& results = outcome.value;
    REQUIRE(results.cases.size() == 2);
    const solve::CaseResult& in = results.cases[0];
    const solve::CaseResult& out = results.cases[1];
    CHECK(points_of(out.frontier) == toy.without.dat);       // as FuSION 1's without-FSC solve
    if (toy.fsc == "hsa15") {                                // down-regulated: S5.17's fix
      CHECK(toy.with.dat.empty());
      CHECK_FALSE(in.frontier.points.empty());
    } else {
      CHECK(points_of(in.frontier) == toy.with.dat);
    }
    for (const solve::CaseResult* r : {&in, &out}) {
      CHECK(r->stats.nodes == run.path->view().node_count());   // the whole corridor is encoded (S5.10)
      for (const solve::FrontierPoint& f : r->frontier.points) CHECK(check(*r->problem, r->witnesses[f.witness]).valid);
    }
    CHECK(run.path->digest() == digest);                     // the solve leaves the corridor unchanged (S4.20)
    const std::string table = bindings_tsv(run.bindings);
    CHECK(table.find("fsc\t" + toy.fsc + "\t") != std::string::npos);   // one row for the FSC (1.22)
    CHECK(step.read("in/case.json").find("\"status\": \"complete\"") != std::string::npos);
  }
}

TEST_CASE("solve: solve() on a positional line: one free case with its lists as written", "[unit][solve][S5.01][S5.05][05.C7]") {
  const ToyCase toy = read_toy("toy-hsa7-wni-tsv-b10");
  const auto run_dir = tests_dir() / "reference" / "synthetic" / toy.name / "f1" / "run";
  for (const bool with : {true, false}) {
    INFO((with ? "with" : "without") << " the FSC");
    Step step;
    core::Context ctx = step.context();
    ToyRunIn20 run(toy, ctx);
    const std::string tag = with ? "_w_" : "_wo_";
    solve::NodeLists lists;
    lists.essential = solve::read_node_list(run_dir / ("essential_nodes_file" + tag + toy.fsc));
    lists.active = solve::read_node_list(run_dir / ("active_nodes_file" + tag + toy.fsc));
    lists.non_relaxed = solve::read_node_list(run_dir / ("nonrelaxed_nodes_file" + tag + toy.fsc));
    if (!with) lists.avoid = solve::read_node_list(run_dir / ("avoid_" + toy.fsc + "_file"));
    solve::Inputs inputs = run.inputs(lists);
    inputs.connect_pair = {"hsa1", "hsa22"};
    solve::Settings settings;
    settings.positional = true;
    settings.path_bound = toy.graph.bound;
    settings.box = toy.box;
    const auto results = solve::solve(inputs, settings, ctx).value;
    REQUIRE(results.cases.size() == 1);
    CHECK(results.cases[0].kase == Case::free);
    CHECK(points_of(results.cases[0].frontier) == (with ? toy.with.dat : toy.without.dat));
    const std::string table = bindings_tsv(run.bindings);
    CHECK(table.find("essential_nodes\thsa1\t") != std::string::npos);
    CHECK(table.find("nonrelaxed_nodes\thsa22\t") != std::string::npos);

    // A connect pair or bound other than the corridor's is a failure (05.C7).
    solve::Inputs wrong = run.inputs(lists);
    wrong.connect_pair = {"hsa2", "hsa22"};
    try {
      solve::solve(wrong, settings, ctx);
      FAIL("a wrong connect pair must fail");
    } catch (const core::Failure& f) {
      CHECK(f.code() == "solve.bound_mismatch");
    }
  }
}

TEST_CASE("solve: solve() binds each list once, warns on what it drops, and fails an unresolved FSC", "[unit][solve][S5.05][S5.17][1.22]") {
  const ToyCase toy = read_toy("toy-hsa9-wni-tsv-b10");
  Step step;
  core::Context ctx = step.context();
  ToyRunIn20 run(toy, ctx);
  solve::NodeLists lists;
  lists.inactive = {"hsa404"};                               // binds to nothing: dropped, with a warning
  lists.non_relaxed = {"hsa1"};                              // the source: no effect
  solve::Inputs inputs = run.inputs(lists);
  inputs.edges.avoid = {solve::parse_edge_line("hsa1 hsa2 activation", 1), solve::parse_edge_line("hsa2 hsa1 activation", 2)};
  solve::Settings settings;
  settings.fsc = {toy.fsc};
  settings.box = toy.box;
  const auto results = solve::solve(inputs, settings, ctx).value;
  CHECK(results.cases.size() == 2);
  CHECK(step.lines->any("solve.list_id_unresolved"));
  CHECK(step.lines->any("solve.edge_line_unmatched"));
  const std::string table = bindings_tsv(run.bindings);
  const auto rows = [&](const std::string& role) {
    std::size_t n = 0;
    for (std::size_t at = 0; (at = table.find("\n" + role + "\t", at)) != std::string::npos; ++at) ++n;
    return n;
  };
  CHECK(rows("fsc") == 1);                                   // bound once for both cases
  CHECK(rows("inactive_nodes") == 1);
  CHECK(rows("avoid_edges") == 2);
  CHECK(table.find("matches no corridor edge") != std::string::npos);
  for (const solve::CaseResult& r : results.cases)          // the avoided edge is never selected
    for (const solve::Witness& w : r.witnesses)
      for (const auto& key : w.selected) CHECK_FALSE((key.source.value == "hsa1" && key.target.value == "hsa2"));

  solve::Settings unknown = settings;
  unknown.fsc = {"hsa404"};
  try {
    solve::solve(run.inputs(), unknown, ctx);
    FAIL("an FSC that binds to nothing must fail");
  } catch (const core::Failure& f) {
    CHECK(f.code() == "solve.fsc_unresolved");
  }
}

TEST_CASE("solve: observations.tsv names the run's genes when the network's handles differ", "[unit][solve][S5.12][02.C3]") {
  const ToyCase toy = read_toy("toy-hsa5-wni-tsv-b10");
  Step step;
  core::Context ctx = step.context();
  ToyRunIn20 run(toy, ctx, true);
  // The fault needs an observed gene whose handle names another gene in the network's own interner.
  const core::Interner& own = run.net->interner();
  CHECK(std::any_of(run.match.begin(), run.match.end(), [&](const expression::ObservationMatch& m) {
    return m.id.value < own.id_count() && own.str(m.id) != run.strings.str(m.id);
  }));
  solve::Settings settings;
  settings.fsc = {toy.fsc};
  settings.box = toy.box;
  solve::solve(run.inputs(), settings, ctx);
  std::set<std::string> expected;
  for (const core::GeneId id : run.observations.observed()) expected.emplace(run.strings.str(id));
  for (const char* kase : {"in", "out"}) {
    INFO(kase);
    std::istringstream rows(step.read(std::string(kase) + "/observations.tsv"));
    std::string line;
    std::getline(rows, line);                                // the header
    std::set<std::string> genes;
    while (std::getline(rows, line)) genes.insert(line.substr(0, line.find('\t')));
    CHECK(genes == expected);
  }
}
