// Building a case (06, "Building a case"; S5.03 to S5.17): the encoded sets, lists, observations and
// their roles, the FSC cases, the complex rule, the endpoint defaults, the clamps, the pre-checks,
// the families and the fingerprint; and the solve's own input files. No Z3.
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <fstream>
#include <iterator>

#include "kit.hpp"

using namespace solvekit;
using fusion::expression::MatchGroup;
using fusion::expression::Regulation;
using solve::Case;
using solve::CaseStatus;
using solve::Role;

namespace {

std::filesystem::path data(const char* file) { return tests_dir() / "unit" / "solve" / "data" / file; }

std::vector<std::string> keys(const solve::Problem& p, const std::vector<std::uint32_t>& list) {
  std::vector<std::string> out;
  for (std::uint32_t i : list) out.push_back(p.nodes[i].value);
  std::sort(out.begin(), out.end());
  return out;
}

Role role(const solve::Problem& p, const std::string& gene) {
  for (const solve::ObservationRow& row : p.observations)
    if (row.gene == gene) return row.role;
  throw std::runtime_error("no row for " + gene);
}

std::string failure_code(const std::function<void()>& f) {
  try {
    f();
  } catch (const fusion::core::Failure& e) {
    return e.code();
  }
  return "";
}

// S -> A -> T and S -> B -> T, with A -| B; every gene observed up.
Graph diamond() {
  Graph g(10);
  g.edge("S", "A");
  g.edge("A", "T");
  g.edge("S", "B");
  g.edge("B", "T");
  g.edge("A", "B", "inhibition");
  g.ends("S", "T");
  return g;
}

}  // namespace

TEST_CASE("solve: the encoded sets come from one search that never expands the target", "[unit][solve][S5.10]") {
  Graph g(10);
  g.edge("S", "A");
  g.edge("A", "T");
  g.edge("A", "A");                                          // a self-loop leaves E (S5.13)
  g.edge("T", "X");                                          // never traversed: T is not expanded
  g.edge("X", "Y");
  g.node("Z");                                               // in the corridor, unreachable
  g.ends("S", "T");
  solve::CaseSpec spec;
  observe(spec, g.g, "A", 'u');
  const solve::Problem p = problem_of(g.g, spec, {}, Case::free);
  CHECK(keys(p, {0, 1, 2}) == std::vector<std::string>{"A", "S", "T"});
  REQUIRE(p.nodes.size() == 3);
  REQUIRE(p.edges.size() == 2);
  CHECK(p.edges[0].key.target.value == "A");
  CHECK(p.edges[1].key.target.value == "T");
  CHECK(p.outside_encoding == 3);
  CHECK(p.nodes[p.source].value == "S");
  CHECK(p.nodes[p.target].value == "T");

  // An essential node joins N, but cannot lie on a path: the case is infeasible (S5.11).
  spec.essential = ids({"Z"});
  const auto skipped = solve::make_case(g.g, spec, {}, Case::free);
  REQUIRE(std::holds_alternative<solve::Skipped>(skipped));
  CHECK(std::get<solve::Skipped>(skipped).status == CaseStatus::infeasible);
  CHECK(std::get<solve::Skipped>(skipped).message.find("Z (essential) cannot lie on a path from S to T") != std::string::npos);
  CHECK(built(skipped).nodes.size() == 4);
}

TEST_CASE("solve: an essential node or edge that cannot lie on a path makes the case infeasible", "[unit][solve][S5.11]") {
  Graph g = diamond();
  g.edge("T", "Q");                                          // an edge out of the target
  solve::CaseSpec spec;
  observe(spec, g.g, "A", 'u');
  CHECK(std::holds_alternative<solve::Problem>(solve::make_case(g.g, spec, {}, Case::free)));
  solve::CaseSpec with_node = spec;
  with_node.essential = ids({"NOWHERE"});                    // bound in the network, not the corridor
  CHECK(std::get<solve::Skipped>(solve::make_case(g.g, with_node, {}, Case::free)).status == CaseStatus::infeasible);
  solve::CaseSpec with_edge = spec;
  with_edge.essential_edges = {5};                           // T -> Q is not encoded
  CHECK(std::get<solve::Skipped>(solve::make_case(g.g, with_edge, {}, Case::free)).status == CaseStatus::infeasible);
  // The in case's FSC is essential too: an FSC on no path makes only the in case infeasible.
  solve::CaseSpec with_fsc = spec;
  with_fsc.fsc = ids({"Q"});
  CHECK(std::get<solve::Skipped>(solve::make_case(g.g, with_fsc, {}, Case::in)).status == CaseStatus::infeasible);
  CHECK(std::holds_alternative<solve::Problem>(solve::make_case(g.g, with_fsc, {}, Case::out)));
}

TEST_CASE("solve: no DE gene in the corridor is the case status no_de_gene", "[unit][solve][06.C15][S5.09]") {
  Graph g = diamond();
  solve::CaseSpec spec;
  observe(spec, g.g, "ELSEWHERE", 'u');
  const auto v = solve::make_case(g.g, spec, {}, Case::free);
  REQUIRE(std::holds_alternative<solve::Skipped>(v));
  CHECK(std::get<solve::Skipped>(v).status == CaseStatus::no_de_gene);
  CHECK(role(built(v), "ELSEWHERE") == Role::not_in_network);
  // The out case removes its FSC from the observations; if it was the only DE gene, nothing is left.
  solve::CaseSpec only_fsc;
  observe(only_fsc, g.g, "A", 'u');
  only_fsc.fsc = ids({"A"});
  CHECK(std::holds_alternative<solve::Problem>(solve::make_case(g.g, only_fsc, {}, Case::in)));
  CHECK(std::get<solve::Skipped>(solve::make_case(g.g, only_fsc, {}, Case::out)).status == CaseStatus::no_de_gene);
}

TEST_CASE("solve: the up and down sets follow FuSION 1's set arithmetic", "[unit][solve][S5.07]") {
  Graph g(10);
  for (const char* v : {"G1", "G2", "G3", "G4", "G5", "G6"}) {
    g.edge("S", v);
    g.edge(v, "T");
  }
  g.ends("S", "T");
  solve::CaseSpec spec;
  observe(spec, g.g, "G1", 'u');                             // avoided: leaves up
  observe(spec, g.g, "G2", 'u');                             // confirmed down: becomes down
  observe(spec, g.g, "G3", 'd');                             // confirmed up: becomes up
  observe(spec, g.g, "G5", 'd');                             // avoided: stays down, held absent
  observe(spec, g.g, "G6", 'u');
  spec.avoid = ids({"G1", "G5"});
  spec.confirmed_down = ids({"G2"});
  spec.confirmed_up = ids({"G3", "G4"});                     // G4 is observed only by confirmation
  const solve::Problem p = problem_of(g.g, spec, {}, Case::free);
  CHECK(p.regulation[at(p, "G1")] == Regulation::none);
  CHECK(p.regulation[at(p, "G2")] == Regulation::down);
  CHECK(p.regulation[at(p, "G3")] == Regulation::up);
  CHECK(p.regulation[at(p, "G4")] == Regulation::up);
  CHECK(p.regulation[at(p, "G5")] == Regulation::down);
  CHECK(p.regulation[at(p, "G6")] == Regulation::up);
  CHECK(keys(p, p.counted) == std::vector<std::string>{"G2", "G3", "G4", "G6"});
  CHECK(role(p, "G1") == Role::forced);
  CHECK(role(p, "G5") == Role::forced);
  CHECK(role(p, "G4") == Role::counted);
  const auto row = std::find_if(p.observations.begin(), p.observations.end(), [](const auto& r) { return r.gene == "G2"; });
  CHECK(row->observed == 'd');                               // the direction the case uses
}

TEST_CASE("solve: every observed gene gets one of the seven roles", "[unit][solve][S5.12][1.5]") {
  Graph g = diamond();
  g.node("C");                                               // in the corridor, not reached
  g.edge("C", "T");
  solve::CaseSpec spec;
  observe(spec, g.g, "S", 'u');
  observe(spec, g.g, "A", 'u');
  observe(spec, g.g, "B", 'd');
  observe(spec, g.g, "C", 'u');
  observe(spec, g.g, "T", 'u');
  observe(spec, g.g, "GONE", 'd');
  solve::CaseSpec::Observation outside;
  outside.gene = "FAR";
  outside.regulation = Regulation::up;
  outside.group = MatchGroup::outside_corridor;
  outside.node = NodeKey{"FAR"};
  spec.observations.push_back(outside);
  spec.non_relaxed = ids({"B"});
  spec.fsc = ids({"A"});
  const solve::Problem in = problem_of(g.g, spec, {}, Case::in);
  CHECK(role(in, "S") == Role::exempt_endpoint);
  CHECK(role(in, "T") == Role::exempt_endpoint);
  CHECK(role(in, "A") == Role::forced);                      // the in case's FSC
  CHECK(role(in, "B") == Role::forced);                      // held by the non-relaxed list
  CHECK(role(in, "C") == Role::outside_encoding);
  CHECK(role(in, "FAR") == Role::outside_corridor);
  CHECK(role(in, "GONE") == Role::not_in_network);
  CHECK(in.counted.empty());
  const solve::Problem out = problem_of(g.g, spec, {}, Case::out);
  CHECK(role(out, "A") == Role::removed_with_fsc);
  const solve::Problem free = problem_of(g.g, spec, {}, Case::free);
  CHECK(role(free, "A") == Role::counted);
  CHECK(keys(free, free.counted) == std::vector<std::string>{"A"});
}

TEST_CASE("solve: non-relaxed observations are held consistent; endpoints are exempt", "[unit][solve][S5.05][S5.12]") {
  Graph g = diamond();
  solve::CaseSpec spec;
  observe(spec, g.g, "A", 'u');
  observe(spec, g.g, "B", 'd');
  observe(spec, g.g, "S", 'u');
  spec.non_relaxed = ids({"A", "B", "S", "T"});              // the batches list the endpoints too
  const solve::Problem p = problem_of(g.g, spec, {}, Case::free);
  CHECK(keys(p, p.consistent) == std::vector<std::string>{"A", "B"});
  CHECK(p.counted.empty());
}

TEST_CASE("solve: contradictory lists are failures before solving", "[unit][solve][S5.05][S3.07]") {
  Graph g = diamond();
  g.node("CX", {"A", "Q"});                                  // a complex holding A
  g.edge("S", "CX");
  g.edge("CX", "T");
  solve::CaseSpec base;
  observe(base, g.g, "A", 'u');
  const auto code = [&](const solve::CaseSpec& spec, Case kase = Case::free) {
    return failure_code([&] { solve::make_case(g.g, spec, {}, kase); });
  };
  solve::CaseSpec s1 = base;
  s1.essential = ids({"B"});
  s1.avoid = ids({"B"});
  CHECK(code(s1) == "solve.contradictory_lists");
  solve::CaseSpec s2 = base;
  s2.active = ids({"B"});
  s2.inactive = ids({"B"});
  CHECK(code(s2) == "solve.contradictory_lists");
  solve::CaseSpec s3 = base;
  s3.confirmed_up = ids({"B"});
  s3.confirmed_down = ids({"B"});
  CHECK(code(s3) == "solve.contradictory_lists");
  solve::CaseSpec s4 = base;
  s4.fsc = ids({"B"});
  s4.avoid = ids({"B"});
  CHECK(code(s4, Case::out) == "solve.contradictory_lists");  // an FSC on an avoid list, whatever the case
  solve::CaseSpec s5 = base;
  s5.essential = ids({"CX"});
  s5.avoid = ids({"Q"});                                     // avoiding Q forbids the complex (entry 229)
  CHECK(code(s5) == "solve.contradictory_lists");
  solve::CaseSpec s6 = base;
  s6.avoid = ids({"S"});                                     // the named form's endpoints are present
  CHECK(code(s6) == "solve.contradictory_lists");
  solve::Settings positional;
  positional.positional = true;
  CHECK_NOTHROW(solve::make_case(g.g, s6, positional, Case::free));   // positional lists as written
  solve::CaseSpec s7 = base;
  s7.non_relaxed = ids({"A"});
  s7.inactive = ids({"A"});                                  // up, held consistent, yet inactive:
  CHECK(code(s7).empty());                                   // UNSAT, as S5.17's positional trap is
  solve::CaseSpec s8 = base;
  s8.fsc = ids({"A"});
  s8.inactive = ids({"A"});                                  // the in case wants the up FSC active
  CHECK(code(s8, Case::in) == "solve.contradictory_lists");
  CHECK(code(s8, Case::out).empty());
  CHECK(code(base).empty());
}

TEST_CASE("solve: the FSC cases in, out and free", "[unit][solve][S5.17][06.C10]") {
  Graph g = diamond();
  g.edge("S", "D");
  g.edge("D", "T");
  solve::CaseSpec spec;
  observe(spec, g.g, "A", 'u');
  observe(spec, g.g, "D", 'd');
  spec.fsc = ids({"A", "D", "B"});                           // a set: up, down and unobserved
  const solve::Problem in = problem_of(g.g, spec, {}, Case::in);
  CHECK(keys(in, in.present) == std::vector<std::string>{"A", "B", "D", "S", "T"});
  CHECK(keys(in, in.active) == std::vector<std::string>{"A", "B", "S", "T"});
  CHECK(keys(in, in.inactive) == std::vector<std::string>{"D"});   // a down-regulated FSC is inactive
  CHECK(in.absent.empty());
  const solve::Problem out = problem_of(g.g, spec, {}, Case::out);
  CHECK(keys(out, out.absent) == std::vector<std::string>{"A", "B", "D"});
  CHECK(out.regulation[at(out, "A")] == Regulation::none);   // removed from the observations
  CHECK(out.regulation[at(out, "D")] == Regulation::none);
  CHECK(keys(out, out.present) == std::vector<std::string>{"S", "T"});
  const solve::Problem free = problem_of(g.g, spec, {}, Case::free);
  CHECK(keys(free, free.present) == std::vector<std::string>{"S", "T"});
  CHECK(free.absent.empty());
  CHECK(free.inactive.empty());

  solve::Settings settings;
  CHECK(solve::requested_cases(settings) == std::vector<Case>{Case::free});
  settings.fsc = {"A"};
  CHECK(solve::requested_cases(settings) == std::vector<Case>{Case::in, Case::out});
  settings.cases = {Case::free, Case::in, Case::free};
  CHECK(solve::requested_cases(settings) == std::vector<Case>{Case::in, Case::free});
  solve::CaseSpec no_fsc;
  observe(no_fsc, g.g, "A", 'u');
  CHECK(failure_code([&] { solve::make_case(g.g, no_fsc, {}, Case::in); }) == "batch.bad_argument");
}

TEST_CASE("solve: an avoided gene forbids every complex that holds it", "[unit][solve][entry 170][entry 229]") {
  Graph g(10);
  g.node("C1", {"G", "H"});                                  // [G+H]
  g.node("C2", {"C1", "G", "H", "K"});                       // [[G+H]+K], closed over C1
  for (const char* v : {"G", "C1", "C2", "K", "H"}) {
    g.edge("S", v);
    g.edge(v, "T");
  }
  g.ends("S", "T");
  solve::CaseSpec spec;
  observe(spec, g.g, "K", 'u');
  spec.avoid = ids({"G"});
  const solve::Problem p = problem_of(g.g, spec, {}, Case::free);
  CHECK(keys(p, p.absent) == std::vector<std::string>{"C1", "C2", "G"});
  solve::Settings positional;                                // a positional line's avoid list alike
  positional.positional = true;
  const solve::Problem q = problem_of(g.g, spec, positional, Case::free);
  CHECK(keys(q, q.absent) == std::vector<std::string>{"C1", "C2", "G"});
  // An avoided gene outside the corridor still forbids the corridor's complexes that hold it.
  solve::CaseSpec far;
  observe(far, g.g, "K", 'u');
  far.avoid = {{"hsa:H", NodeKey{"H"}}};
  far.avoid.push_back({"Z", NodeKey{"Z"}});
  g.node("C3", {"Z"});
  g.edge("S", "C3");
  g.edge("C3", "T");
  const solve::Problem r = problem_of(g.g, far, {}, Case::free);
  CHECK(keys(r, r.absent) == std::vector<std::string>{"C1", "C2", "C3", "H"});
  // The out case's FSC follows the same rule (entry 170).
  solve::CaseSpec fsc;
  observe(fsc, g.g, "K", 'u');
  fsc.fsc = ids({"H"});
  const solve::Problem out = problem_of(g.g, fsc, {}, Case::out);
  CHECK(keys(out, out.absent) == std::vector<std::string>{"C1", "C2", "H"});
  const solve::Problem in = problem_of(g.g, fsc, {}, Case::in);
  CHECK(in.absent.empty());
}

TEST_CASE("solve: the named form makes the endpoints present and active; a positional line does not", "[unit][solve][06.C16]") {
  Graph g = diamond();
  solve::CaseSpec spec;
  observe(spec, g.g, "A", 'u');
  const solve::Problem named = problem_of(g.g, spec, {}, Case::free);
  CHECK(keys(named, named.present) == std::vector<std::string>{"S", "T"});
  CHECK(keys(named, named.active) == std::vector<std::string>{"S", "T"});
  CHECK(named.named);
  solve::Settings positional;
  positional.positional = true;
  const solve::Problem pos = problem_of(g.g, spec, positional, Case::free);
  CHECK(pos.present.empty());                                // its lists as written
  CHECK(keys(pos, pos.active) == std::vector<std::string>{"S"});   // FuSION 1 forced the source active
  CHECK_FALSE(pos.named);
}

TEST_CASE("solve: bounds clamp to the counted observations and the encoded edges", "[unit][solve][S5.07][06.C14][06.C17]") {
  Graph g(10);
  g.edge("S", "A");
  g.edge("A", "B");
  g.edge("B", "T");
  g.ends("S", "T");
  solve::CaseSpec spec;
  observe(spec, g.g, "A", 'u');
  solve::Settings settings;                                  // the default box, 0..8 and 0..8 (06.C17)
  CHECK(settings.box.lo == Point{0, 0});
  CHECK(settings.box.hi == Point{8, 8});
  settings.box.hi.e = 4;                                     // an edge bound of 4 on three edges
  const solve::Problem p = problem_of(g.g, spec, settings, Case::free);
  CHECK(p.box.hi == Point{1, 3});
  CHECK(p.requested.hi == Point{8, 4});
  REQUIRE(p.warnings.size() == 2);
  CHECK(p.warnings[0].first == "solve.bound_clamped");
  CHECK(p.warnings[0].second == "node_max 8 -> 1 (1 counted observation)");
  CHECK(p.warnings[1].second == "edge_max 4 -> 3 (3 encoded edges)");
  settings.box = {{2, 5}, {3, 9}};
  const solve::Problem q = problem_of(g.g, spec, settings, Case::free);
  CHECK(q.box.lo == Point{1, 3});
  CHECK(q.box.hi == Point{1, 3});
}

TEST_CASE("solve: a positional line's bound must be the corridor's", "[unit][solve][05.C7]") {
  Graph g = diamond();
  solve::CaseSpec spec;
  observe(spec, g.g, "A", 'u');
  solve::Settings settings;
  settings.path_bound = 7;
  CHECK(failure_code([&] { solve::make_case(g.g, spec, settings, Case::free); }) == "solve.bound_mismatch");
  settings.path_bound = 10;
  CHECK_NOTHROW(solve::make_case(g.g, spec, settings, Case::free));
}

TEST_CASE("solve: the constraint families: a given flag decides, else the inputs", "[unit][solve][S5.03][S5.15][S5.16]") {
  Graph g(10);
  g.edge("S", "A", "activation", 900);
  g.edge("A", "T", "activation", 800);                       // exactly the threshold: not frozen
  g.edge("S", "B", "activation", 0, "PPrel", true);          // unscored
  g.edge("B", "T", "activation", 801);
  g.ends("S", "T");
  solve::CaseSpec spec;
  observe(spec, g.g, "A", 'u');
  spec.coexpression_table = true;
  spec.coexpression = {{NodeKey{"A"}, NodeKey{"B"}, 750},
                       {NodeKey{"B"}, NodeKey{"A"}, 701},    // the same unordered pair
                       {NodeKey{"A"}, NodeKey{"A"}, 999},    // not a pair
                       {NodeKey{"A"}, NodeKey{"S"}, 700},    // not strictly above
                       {NodeKey{"A"}, NodeKey{"FAR"}, 999}}; // not encoded
  solve::Settings settings;
  settings.families.coexpression_threshold = 700;
  settings.families.frozen_threshold = 800;
  const solve::Problem on = problem_of(g.g, spec, settings, Case::free);
  CHECK(on.families.coexpression);
  CHECK(on.families.frozen);
  REQUIRE(on.coexpressed.size() == 1);
  CHECK(keys(on, {on.coexpressed[0].first, on.coexpressed[0].second}) == std::vector<std::string>{"A", "B"});
  REQUIRE(on.frozen.size() == 2);                            // 900 and 801; 800 is not above 800
  CHECK(on.edges[on.frozen[0]].score == 900);
  CHECK(on.edges[on.frozen[1]].score == 801);
  CHECK(on.unscored == 1);
  settings.families.enabled = false;                         // a given flag decides
  const solve::Problem off = problem_of(g.g, spec, settings, Case::free);
  CHECK_FALSE(off.families.coexpression);
  CHECK_FALSE(off.families.frozen);
  CHECK(off.coexpressed.empty());
  CHECK(off.frozen.empty());
  settings.families.enabled.reset();
  settings.families.frozen_threshold = -1;                   // FuSION 1's own "off"
  spec.coexpression_table = false;                           // a threshold without a CSV
  const solve::Problem none = problem_of(g.g, spec, settings, Case::free);
  CHECK_FALSE(none.families.coexpression);
  CHECK_FALSE(none.families.frozen);
}

TEST_CASE("solve: edge lines match by their ends and their subtype set", "[unit][solve][S5.06]") {
  Graph g(10);
  g.edge("S", "A", "activation,phosphorylation");
  g.edge("S", "A", "phosphorylation,activation", 0, "GErel");
  g.edge("S", "A", "activation");
  g.edge("A", "T", "expression");
  g.ends("S", "T");
  const std::vector<std::string> pa{"phosphorylation", "activation"};
  CHECK(solve::match_edge_line(g.g, NodeKey{"S"}, NodeKey{"A"}, pa) == std::vector<std::uint32_t>{0, 1});
  CHECK(solve::match_edge_line(g.g, NodeKey{"S"}, NodeKey{"A"}, std::vector<std::string>{"activation"}) ==
        std::vector<std::uint32_t>{2});
  CHECK(solve::match_edge_line(g.g, NodeKey{"A"}, NodeKey{"S"}, pa).empty());

  const solve::EdgeLine line = solve::parse_edge_line("hsa1 hsa2 activation_phosphorylation", 4);
  CHECK(line.source == "hsa1");
  CHECK(line.target == "hsa2");
  CHECK(line.subtypes == std::vector<std::string>{"activation", "phosphorylation"});
  CHECK(line.line == 4);
  CHECK(solve::parse_edge_line("a b x,y", 1).subtypes == std::vector<std::string>{"x", "y"});
  CHECK(solve::parse_edge_line("a b -", 1).subtypes.empty());
  CHECK(failure_code([] { solve::parse_edge_line("hsa2 hsa3", 2); }) == "solve.edge_line_no_subtype");

  const auto lines = solve::read_edge_list(data("edges.txt"));
  REQUIRE(lines.size() == 4);
  CHECK(lines[0].source == "c_s_hsa6772_c_e");
  CHECK(lines[0].subtypes == std::vector<std::string>{"expression"});
  CHECK(lines[2].subtypes == std::vector<std::string>{"inhibition", "dephosphorylation"});
  CHECK(lines[2].line == 4);
  CHECK(lines[3].subtypes.empty());
  try {
    solve::read_edge_list(data("edges_bad.txt"));
    FAIL("a line without subtypes must fail");
  } catch (const fusion::core::Failure& f) {
    CHECK(f.code() == "solve.edge_line_no_subtype");
    CHECK(f.where().line == 2);
  }
  CHECK(solve::read_node_list(data("nodes.txt")) == std::vector<std::string>{"hsa1956", "hsa3576", "hsa3725", "hsa:6774"});
  CHECK(failure_code([] { solve::read_node_list(data("missing.txt")); }) == "input.unreadable");
}

TEST_CASE("solve: the coexpression CSV is read by its column names, with skipped rows counted", "[unit][solve][S5.15]") {
  const auto text = [] {
    std::ifstream in(data("coexpression.csv"), std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
  }();
  const solve::CoexpressionTable table = solve::CoexpressionTable::parse(text, "coexpression.csv");
  REQUIRE(table.rows().size() == 2);
  CHECK(table.rows()[0].ids1 == std::vector<std::string>{"hsa972"});
  CHECK(table.rows()[0].ids2 == std::vector<std::string>{"hsa3108", "hsa3109"});
  CHECK(table.rows()[0].score == 611);
  CHECK(table.rows()[0].line == 2);
  CHECK(table.rows()[1].ids1 == std::vector<std::string>{"hsa1"});
  CHECK(table.rows()[1].score == 700);
  CHECK(std::vector<std::uint32_t>(table.skipped_lines().begin(), table.skipped_lines().end()) ==
        std::vector<std::uint32_t>{4, 5, 6});
  CHECK(failure_code([] { solve::CoexpressionTable::parse("a,b,c\n1,2,3\n", "x.csv"); }) == "solve.coexpression_columns");
}

TEST_CASE("solve: the fingerprint follows every encoding input and nothing else", "[unit][solve][06.C18]") {
  Graph g = diamond();
  solve::CaseSpec spec;
  observe(spec, g.g, "A", 'u');
  solve::Settings settings;
  const auto print = [&](const solve::CaseGraph& graph, const solve::CaseSpec& s, const solve::Settings& st,
                         Case kase = Case::free) { return solve::fingerprint(problem_of(graph, s, st, kase)).hex(); };
  const std::string base = print(g.g, spec, settings);
  CHECK(base == print(g.g, spec, settings));
  solve::Settings limits = settings;                         // limits, box, threads, solutions, retry: no
  limits.timeout = std::chrono::milliseconds(1000);
  limits.rlimit = 5;
  limits.box = {{0, 0}, {1, 1}};
  limits.threads = 1;
  limits.solutions = 3;
  limits.retry = solve::Retry::none;
  limits.budget = std::chrono::milliseconds(60'000);
  CHECK(print(g.g, spec, limits) == base);
  solve::Settings seeded = settings;                         // the seed: yes (S5.22)
  seeded.seed = 7;
  CHECK(print(g.g, spec, seeded) != base);
  solve::CaseSpec listed = spec;                             // a list: yes
  listed.inactive = ids({"B"});
  CHECK(print(g.g, listed, settings) != base);
  solve::CaseSpec observed = spec;                           // an observation: yes
  observe(observed, g.g, "B", 'd');
  CHECK(print(g.g, observed, settings) != base);
  Graph scored = diamond();                                  // a score: yes
  scored.g.edges[0].score = 1;
  CHECK(print(scored.g, spec, settings) != base);
  solve::Settings positional = settings;                     // the form: yes
  positional.positional = true;
  CHECK(print(g.g, spec, positional) != base);
  solve::CaseSpec fsc = spec;
  fsc.fsc = ids({"B"});
  CHECK(print(g.g, fsc, settings, Case::in) != print(g.g, fsc, settings, Case::out));
}

TEST_CASE("solve: unknown answers are labelled from the reason read before pop", "[unit][solve][06.C2]") {
  using solve::Outcome;
  CHECK(solve::unknown_outcome("timeout", false, false) == Outcome::timeout);
  CHECK(solve::unknown_outcome("canceled (timeout)", false, false) == Outcome::timeout);
  CHECK(solve::unknown_outcome("max. resource limit exceeded", true, false) == Outcome::timeout);
  CHECK(solve::unknown_outcome("canceled", true, false) == Outcome::timeout);   // 4.8.12's exhausted rlimit
  CHECK(solve::unknown_outcome("canceled", false, false) == Outcome::undefined);
  CHECK(solve::unknown_outcome("unknown", false, false) == Outcome::undefined);
  CHECK(solve::unknown_outcome("timeout", false, true) == Outcome::stopped);   // this solve interrupted it
}

TEST_CASE("solve: names, cases and the settings registry", "[unit][solve][1.6][1.16]") {
  CHECK(solve::name(Case::in) == "in");
  CHECK(solve::name(CaseStatus::budget_exhausted) == "budget_exhausted");
  CHECK(solve::name(Role::removed_with_fsc) == "removed_with_fsc");
  CHECK(solve::name(Phase::smallest_e) == "smallest_e");
  CHECK(solve::name(solve::Outcome::undefined) == "undefined");
  CHECK(solve::name(solve::Source::model) == "model");
  CHECK(solve::parse_case("out") == Case::out);
  CHECK_FALSE(solve::parse_case("both"));                    // only FuSION 1's positional pex keeps it
  const auto registry = solve::settings();
  const auto find = [&](std::string_view n) {
    const auto it = std::find_if(registry.begin(), registry.end(), [&](const auto& s) { return s.name == n; });
    REQUIRE(it != registry.end());
    return *it;
  };
  for (const auto& s : registry) CHECK(s.module == "solve");
  CHECK(find("node_max").default_text == "8");
  CHECK(find("edge_max").default_text == "8");
  CHECK(find("timeout").default_text == "4h");
  CHECK(find("incremental_timeout").default_text == "1h");
  CHECK(find("seed").default_text == "42");
  CHECK(find("rlimit").default_text == "0");
  CHECK(find("solutions").min == 1);
  CHECK_FALSE(find("threads").affects_results);
  CHECK(find("timeout").on_resume == fusion::core::OnResume::may_change);
  CHECK(find("fsc").on_resume == fusion::core::OnResume::must_match);
  CHECK(registry.size() == 31);
}
