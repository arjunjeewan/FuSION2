// The encoding on Z3 (S5.05 to S5.17, 06.C1), checked against FuSION 1's own frontiers on the toy
// cases, against an exhaustive frontier on random tiny cases, and family by family on hand-made
// graphs; the TIMEOUT label (06.C2, 2B.8), interrupts, extra solutions (06.C11) and determinism.
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <set>

#include "fusion/testkit/catch.hpp"
#include "kit.hpp"

using namespace solvekit;
using fusion::expression::Regulation;
using solve::Case;
using solve::Outcome;

namespace {

bool present(const solve::Witness& w, const std::string& key) {
  const auto has = [&](const std::vector<NodeKey>& list) {
    return std::any_of(list.begin(), list.end(), [&](const NodeKey& k) { return k.value == key; });
  };
  return has(w.active) || has(w.inactive);
}

bool active(const solve::Witness& w, const std::string& key) {
  return std::any_of(w.active.begin(), w.active.end(), [&](const NodeKey& k) { return k.value == key; });
}

bool selects(const solve::Witness& w, const std::string& source, const std::string& target) {
  return std::any_of(w.selected.begin(), w.selected.end(),
                     [&](const EdgeKey& k) { return k.source.value == source && k.target.value == target; });
}

// Every witness of a solve passes the kit's independent checker and costs exactly its counts (R1), and
// each frontier point's witness costs the point (06.C20).
void check_witnesses(const CaseRef& c, const Solved& s) {
  for (const solve::Witness& w : s.witnesses) {
    const testkit::Verdict v = kit_check(c, w);
    INFO((v.violations.empty() ? std::string() : v.violations.front()));
    CHECK(v.valid);
  }
  for (const solve::FrontierPoint& f : s.frontier.points) CHECK(s.witnesses[f.witness].counts == f.point);
}

std::vector<const solve::Witness*> frontier_witnesses(const Solved& s) {
  std::vector<const solve::Witness*> out;
  for (const solve::FrontierPoint& f : s.frontier.points) out.push_back(&s.witnesses[f.witness]);
  return out;
}

}  // namespace

TEST_CASE("solve: the toy cases' positional solves give FuSION 1's frontiers", "[unit][solve][S5.18][S5.25]") {
  // Each FuSION 1 pathz3 line is one free case with its lists as written (06, "FuSION 1 positional
  // lines"), here on FuSION 1's own corridor, which has no self-loop for S5.13 to drop.
  solve::Settings settings;
  settings.positional = true;
  for (const std::string& name : toy_names()) {
    const ToyCase toy = read_toy(name);
    settings.box = toy.box;
    for (const ToyRun* run : {&toy.with, &toy.without}) {
      INFO(name << (run == &toy.with ? " with" : " without") << " the FSC");
      const auto v = solve::make_case(toy.graph, run->spec, settings, Case::free);
      REQUIRE(std::holds_alternative<solve::Problem>(v));
      const solve::Problem& p = std::get<solve::Problem>(v);
      const Solved s = solve_problem(p, settings);
      CHECK(points_of(s.frontier) == run->dat);
      for (const solve::FrontierPoint& f : s.frontier.points) CHECK(f.certified);
      check_witnesses({toy.graph, run->spec, settings, Case::free}, s);
    }
  }
}

TEST_CASE("solve: the toy cases' named FSC cases: out as FuSION 1, in fixed for a down-regulated FSC", "[unit][solve][S5.17][06.C10]") {
  solve::Settings settings;
  int down_regulated = 0;
  for (const std::string& name : toy_names()) {
    const ToyCase toy = read_toy(name);
    settings.box = toy.box;
    solve::CaseSpec spec = toy.observations;
    spec.fsc = ids({toy.fsc});
    INFO(name);
    const solve::Problem out = problem_of(toy.graph, spec, settings, Case::out);
    const Solved so = solve_problem(out, settings);
    CHECK(points_of(so.frontier) == toy.without.dat);
    check_witnesses({toy.graph, spec, settings, Case::out}, so);
    for (const solve::Witness& w : so.witnesses) CHECK_FALSE(present(w, toy.fsc));

    const solve::Problem in = problem_of(toy.graph, spec, settings, Case::in);
    const Solved si = solve_problem(in, settings);
    check_witnesses({toy.graph, spec, settings, Case::in}, si);
    if (in.regulation[at(in, toy.fsc)] == Regulation::down) {
      // FuSION 1 forced it active and not relaxed, so every point was UNSAT (S5.17's trap): the
      // with-FSC .dat is empty. 2.0 holds it present and inactive.
      ++down_regulated;
      CHECK(toy.with.dat.empty());
      CHECK_FALSE(si.frontier.points.empty());
      for (const solve::Witness* w : frontier_witnesses(si)) {
        CHECK(present(*w, toy.fsc));
        CHECK_FALSE(active(*w, toy.fsc));
      }
    } else {
      CHECK(points_of(si.frontier) == toy.with.dat);
      for (const solve::Witness& w : si.witnesses) CHECK(active(w, toy.fsc));
    }
  }
  CHECK(down_regulated == 1);                                // hsa15's toy
}

TEST_CASE("solve: random tiny cases: Z3's frontier is the exhaustive frontier", "[unit][solve][property][S5.11][S5.12][S5.13][S5.14][12.C9]") {
  // Section 12's solve row: 200 random tiny cases of 6 nodes and 8 edges per run, drawn by the kit's
  // generator on core::Rng (12.C9), each solved by Z3 and by the kit's exhaustive frontier, which
  // re-derives the case from the corridor and the ids (C2.1). Every witness passes the kit's checker.
  FUSION_SEED(2026);
  auto rng = fusion::core::Rng::stream(fusion_seed, 0);
  int solved = 0, skipped = 0, failed = 0;
  for (int round = 0; round < 1000 && solved < 200; ++round) {
    const testkit::TinyCase tc = testkit::tiny_case(rng, 6, 8, 2 + static_cast<int>(rng.below(3)));
    const TinyInputs in = inputs_of(tc);
    const CaseRef c{in.graph, in.spec, in.settings, in.kase};
    std::optional<std::variant<solve::Problem, solve::Skipped>> v;
    try {
      v = solve::make_case(in.graph, in.spec, in.settings, in.kase);
    } catch (const fusion::core::Failure&) {
      continue;                                              // contradictory lists (S5.05)
    }
    INFO("round " << round << "\n" << describe(tc));
    if (auto* sk = std::get_if<solve::Skipped>(&*v)) {
      ++skipped;
      if (sk->status == solve::CaseStatus::no_de_gene && sk->problem->named)
        CHECK(kit_frontier(c).empty());                      // S5.14 leaves no point (06.C15)
      continue;
    }
    const solve::Problem& p = std::get<solve::Problem>(*v);
    const Solved s = solve_problem(p, in.settings);
    const std::vector<Point> exhaustive = kit_frontier(c);
    CHECK(points_of(s.frontier) == exhaustive);
    check_witnesses(c, s);
    if (points_of(s.frontier) != exhaustive && failed++ == 0)   // 12.C9: the failing input, kept as a fixture
      testkit::keep_failing_input("solve-tiny", "case-" + std::to_string(round) + ".txt", describe(tc));
    ++solved;
  }
  CHECK(solved == 200);
  CHECK(skipped > 0);
}

TEST_CASE("solve: an out-case FSC and an avoided gene forbid the complexes that hold them", "[unit][solve][entry 170][entry 229]") {
  // F is the cheap route. Without F, the complex [F+X] would be free too; the rule leaves only Y,
  // whose route relaxes one edge.
  Graph g(10);
  g.node("CX", {"F", "X"});
  g.edge("S", "CX");
  g.edge("CX", "T");
  g.edge("S", "F");
  g.edge("F", "T");
  g.edge("S", "Y", "inhibition");
  g.edge("Y", "T");
  g.ends("S", "T");
  solve::CaseSpec spec;
  observe(spec, g.g, "T", 'u');
  observe(spec, g.g, "S", 'u');                              // the source is DE: any route is fine
  observe(spec, g.g, "F", 'u');
  solve::Settings settings;
  const Solved free = solve_problem(problem_of(g.g, spec, settings, Case::free), settings);
  CHECK(points_of(free.frontier) == std::vector<Point>{{0, 0}});

  solve::CaseSpec fsc = spec;
  fsc.fsc = ids({"F"});
  const solve::Problem out = problem_of(g.g, fsc, settings, Case::out);
  const Solved so = solve_problem(out, settings);
  CHECK(points_of(so.frontier) == std::vector<Point>{{0, 1}});   // only through Y now
  for (const solve::Witness& w : so.witnesses) {
    CHECK_FALSE(present(w, "CX"));
    CHECK_FALSE(present(w, "F"));
  }
  check_witnesses({g.g, fsc, settings, Case::out}, so);

  solve::CaseSpec avoid = spec;                              // avoid_nodes, named form
  avoid.avoid = ids({"F"});
  const Solved sa = solve_problem(problem_of(g.g, avoid, settings, Case::free), settings);
  CHECK(points_of(sa.frontier) == std::vector<Point>{{0, 1}});
  for (const solve::Witness& w : sa.witnesses) CHECK_FALSE(present(w, "CX"));

  solve::Settings positional = settings;                     // a positional line's avoid list alike
  positional.positional = true;
  avoid.essential = ids({"S", "T"});
  avoid.active = ids({"S", "T"});
  const Solved sp = solve_problem(problem_of(g.g, avoid, positional, Case::free), positional);
  CHECK(points_of(sp.frontier) == std::vector<Point>{{0, 1}});
  for (const solve::Witness& w : sp.witnesses) CHECK_FALSE(present(w, "CX"));
}

TEST_CASE("solve: a non-relaxed up-regulated observation is present and active in every witness", "[unit][solve][S5.05]") {
  Graph g(10);
  g.edge("S", "A");
  g.edge("A", "T");
  g.edge("S", "U", "inhibition");                            // through U costs an edge
  g.edge("U", "T");
  g.ends("S", "T");
  solve::CaseSpec spec;
  observe(spec, g.g, "A", 'u');
  observe(spec, g.g, "U", 'u');
  solve::Settings settings;
  const Solved loose = solve_problem(problem_of(g.g, spec, settings, Case::free), settings);
  CHECK(points_of(loose.frontier) == std::vector<Point>{{0, 1}, {1, 0}});   // U kept, or left out and relaxed
  spec.non_relaxed = ids({"U"});
  const solve::Problem held = problem_of(g.g, spec, settings, Case::free);
  const Solved s = solve_problem(held, settings);
  CHECK(points_of(s.frontier) == std::vector<Point>{{0, 1}});
  for (const solve::Witness& w : s.witnesses) CHECK(active(w, "U"));
  check_witnesses({g.g, spec, settings, Case::free}, s);
}

TEST_CASE("solve: a frozen self-loop constrains nothing", "[unit][solve][S5.13][S5.16][05.C2]") {
  // FuSION 1 froze an inhibitory self-loop, which forced its node absent (06, "Encoding").
  Graph g(10);
  g.edge("S", "A");
  g.edge("A", "A", "inhibition", 900);
  g.edge("A", "T");
  g.ends("S", "T");
  solve::CaseSpec spec;
  observe(spec, g.g, "A", 'u');
  solve::Settings settings;
  settings.families.frozen_threshold = 800;
  const solve::Problem p = problem_of(g.g, spec, settings, Case::free);
  CHECK(p.edges.size() == 2);
  CHECK(p.frozen.empty());
  CHECK(points_of(solve_problem(p, settings).frontier) == std::vector<Point>{{0, 0}});
}

TEST_CASE("solve: an edge bound above the encoded edges is clamped, not wrapped", "[unit][solve][06.C14][S5.09]") {
  // Three inhibitions between nodes that must all be active cost exactly three relaxations. In
  // FuSION 1, an edge bound of 4 in two bits wrapped to 0, and the point was UNSAT (S5.09).
  Graph g(10);
  g.edge("S", "A", "inhibition");
  g.edge("A", "B", "inhibition");
  g.edge("B", "T", "inhibition");
  g.ends("S", "T");
  solve::CaseSpec spec;
  observe(spec, g.g, "A", 'u');
  spec.active = ids({"A", "B"});
  solve::Settings settings;
  settings.box.hi.e = 4;
  const solve::Problem p = problem_of(g.g, spec, settings, Case::free);
  CHECK(p.box.hi.e == 3);
  const Solved s = solve_problem(p, settings);
  CHECK(points_of(s.frontier) == std::vector<Point>{{0, 3}});
  check_witnesses({g.g, spec, settings, Case::free}, s);
}

TEST_CASE("solve: every selected path crosses a DE gene before the target", "[unit][solve][S5.14]") {
  Graph g(10);
  g.edge("S", "T");                                          // direct, from a non-DE source: never selectable
  g.edge("S", "A");
  g.edge("A", "T");
  g.ends("S", "T");
  solve::CaseSpec spec;
  observe(spec, g.g, "A", 'u');
  observe(spec, g.g, "T", 'u');                              // a DE target does not count
  solve::Settings settings;
  const solve::Problem p = problem_of(g.g, spec, settings, Case::free);
  const Solved s = solve_problem(p, settings);
  CHECK(points_of(s.frontier) == std::vector<Point>{{0, 0}});
  for (const solve::Witness& w : s.witnesses) {
    CHECK_FALSE(selects(w, "S", "T"));
    CHECK(selects(w, "S", "A"));
  }
  // With only the direct edge, no point exists.
  Graph direct(10);
  direct.edge("S", "T");
  direct.edge("S", "A");
  direct.ends("S", "T");
  solve::CaseSpec only_target;
  observe(only_target, direct.g, "T", 'u');
  const Solved none = solve_problem(problem_of(direct.g, only_target, settings, Case::free), settings);
  CHECK(none.frontier.points.empty());
  CHECK(none.frontier.undecided.empty());
}

TEST_CASE("solve: coexpressed genes share their activity when both are present", "[unit][solve][S5.15]") {
  // U is up and V down; on their own both are explained. Coexpressed, one of them must give.
  Graph g(10);
  g.edge("S", "U");
  g.edge("U", "V", "inhibition");
  g.edge("V", "T", "inhibition");
  g.ends("S", "T");
  solve::CaseSpec spec;
  observe(spec, g.g, "U", 'u');
  observe(spec, g.g, "V", 'd');
  solve::Settings settings;
  CHECK(points_of(solve_problem(problem_of(g.g, spec, settings, Case::free), settings).frontier) ==
        std::vector<Point>{{0, 0}});
  spec.coexpression_table = true;
  spec.coexpression = {{NodeKey{"U"}, NodeKey{"V"}, 900}};
  settings.families.coexpression_threshold = 700;
  const solve::Problem p = problem_of(g.g, spec, settings, Case::free);
  const Solved s = solve_problem(p, settings);
  check_witnesses({g.g, spec, settings, Case::free}, s);
  for (const solve::Witness& w : s.witnesses) CHECK(active(w, "U") == active(w, "V"));
  CHECK(points_of(s.frontier) != std::vector<Point>{{0, 0}});
}

TEST_CASE("solve: edge lists select, avoid and hold edges", "[unit][solve][S5.06]") {
  Graph g(10);
  g.edge("S", "A");
  g.edge("A", "T");
  g.edge("S", "B");
  g.edge("B", "T");
  g.ends("S", "T");
  solve::CaseSpec spec;
  observe(spec, g.g, "S", 'u');
  solve::Settings settings;
  solve::CaseSpec essential = spec;
  essential.essential_edges = {2};                           // S -> B
  essential.avoid_edges = {0};                               // S -> A
  const solve::Problem p = problem_of(g.g, essential, settings, Case::free);
  const Solved s = solve_problem(p, settings);
  REQUIRE_FALSE(s.witnesses.empty());
  for (const solve::Witness& w : s.witnesses) {
    CHECK(selects(w, "S", "B"));
    CHECK_FALSE(selects(w, "S", "A"));
  }
  solve::CaseSpec held = spec;                               // B must be active for S -> B to hold
  held.nonrelaxed_edges = {2};
  held.inactive = ids({"B"});
  held.essential_edges = {2};
  const Solved none = solve_problem(problem_of(g.g, held, settings, Case::free), settings);
  CHECK(none.frontier.points.empty());
}

TEST_CASE("solve: a small rlimit labels checks TIMEOUT; an interrupt gives stopped, never cached", "[unit][solve][06.C2][2B.8]") {
  const ToyCase toy = read_toy("toy-hsa5-wni-tsv-b10");
  solve::Settings settings;
  settings.positional = true;
  settings.box = toy.box;
  const solve::Problem p = problem_of(toy.graph, toy.with.spec, settings, Case::free);
  settings.rlimit = 1;
  solve::CaseSolver limited(p, settings);
  const solve::Answer a = limited.check({3, 3}, Phase::diagonal);
  CHECK(a.outcome == Outcome::timeout);
  CHECK(a.rlimit == 1);
  CHECK_FALSE(a.reason.empty());
  const Solved s = solve_problem(p, settings);               // every answer unknown: nothing proven
  CHECK(s.frontier.points.empty());
  CHECK(s.frontier.undecided.size() == 64);
  std::set<Point> asked;
  for (const solve::Answer& x : s.log)
    if (x.source == solve::Source::z3) CHECK(asked.insert(x.point).second);   // S5.20

  solve::Settings plain;
  plain.box = toy.box;
  solve::CaseSolver solver(p, plain);
  CHECK(solver.check({3, 3}, Phase::diagonal).outcome == Outcome::unsat);   // FuSION 1's first answer
  solver.interrupt();
  CHECK(solver.interrupted());
  const solve::Answer stopped = solver.check({5, 5}, Phase::diagonal);
  CHECK(stopped.outcome == Outcome::stopped);
  solve::AnswerCache cache(p.box);
  cache.record(stopped);
  CHECK_FALSE(cache.lookup({5, 5}));
  CHECK_FALSE(solver.next_solution({5, 5}, {}));
  CHECK(solver.last_answer().outcome == Outcome::stopped);
}

TEST_CASE("solve: extra solutions are distinct witnesses at their point", "[unit][solve][06.C11][S5.26]") {
  // Two routes, each free: at (0, 0) the witnesses are A's route, B's route and both, and no more.
  Graph g(10);
  g.edge("S", "A");
  g.edge("A", "T");
  g.edge("S", "B");
  g.edge("B", "T");
  g.ends("S", "T");
  solve::CaseSpec spec;
  observe(spec, g.g, "S", 'u');
  solve::Settings settings;
  const solve::Problem p = problem_of(g.g, spec, settings, Case::free);
  solve::CaseSolver solver(p, settings);
  solve::AnswerCache cache(p.box);
  SolverOracle oracle(solver);
  Quiet quiet;
  const auto found = solve::pareto_search(p.box, cache, oracle, quiet);
  const solve::Frontier frontier = solve::assemble(p.box, cache.answers(), solver.witnesses(), found);
  REQUIRE(points_of(frontier) == std::vector<Point>{{0, 0}});
  std::vector<solve::Witness> seen{solver.witnesses()[frontier.points.front().witness]};
  for (;;) {
    const std::optional<solve::Witness> next = solver.next_solution({0, 0}, seen);
    CHECK(solver.last_answer().phase == Phase::extra);
    if (!next) break;
    CHECK(kit_check({g.g, spec, settings, Case::free}, *next).valid);
    CHECK(next->counts == Point{0, 0});
    for (const solve::Witness& w : seen)                     // differs in presence, activity or selection
      CHECK((w.active != next->active || w.inactive != next->inactive || w.selected != next->selected));
    seen.push_back(*next);
    REQUIRE(seen.size() <= 3);
  }
  CHECK(solver.last_answer().outcome == Outcome::unsat);     // "no more solutions"
  CHECK(seen.size() == 3);
  // The blocking UNSAT is not a proof that (0, 0) is UNSAT.
  cache.record(solver.last_answer());
  CHECK(cache.proven_sat({0, 0}));
  CHECK_FALSE(cache.proven_unsat({0, 0}));
}

TEST_CASE("solve: the same problem solved twice gives the same answers and witnesses", "[unit][solve][S5.22][S5.29]") {
  const ToyCase toy = read_toy("toy-hsa13-wni-tsv-b15");
  solve::Settings settings;
  settings.positional = true;
  settings.box = toy.box;
  const solve::Problem p = problem_of(toy.graph, toy.without.spec, settings, Case::free);
  const Solved a = solve_problem(p, settings), b = solve_problem(p, settings);
  REQUIRE(a.log.size() == b.log.size());
  for (std::size_t k = 0; k < a.log.size(); ++k) {
    CHECK(a.log[k].point == b.log[k].point);
    CHECK(a.log[k].outcome == b.log[k].outcome);
    CHECK(a.log[k].model == b.log[k].model);
  }
  REQUIRE(a.witnesses.size() == b.witnesses.size());
  for (std::size_t k = 0; k < a.witnesses.size(); ++k) {
    CHECK(a.witnesses[k].active == b.witnesses[k].active);
    CHECK(a.witnesses[k].selected == b.witnesses[k].selected);
  }
}

TEST_CASE("solve: the linked Z3 is the one solve was built against", "[unit][solve][11.C17]") {
  CHECK_NOTHROW(solve::verify_z3());
  CHECK(solve::z3_version() == "4.8.12.0");
}
