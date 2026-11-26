// The solve tests' own helpers: hand-built cases, solving in-process, FuSION 1's regenerated toy cases
// (booking 1), and the bridge to the test kit (section 12, "The test kit"; C2.1). Witnesses and
// frontiers are judged by the kit's independent checker and exhaustive frontier, through the kit's
// adapters, so they re-derive what make_case derives; the kit also brings ScriptedOracle and FuSION 1's
// readers.
#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

#include "fusion/solve/solve.hpp"
#include "fusion/testkit/adapt.hpp"
#include "fusion/testkit/check.hpp"
#include "fusion/testkit/fusion1.hpp"
#include "fusion/testkit/generate.hpp"
#include "fusion/testkit/oracles.hpp"
#include "fusion/testkit/scripted_oracle.hpp"

namespace solvekit {

namespace solve = fusion::solve;
namespace testkit = fusion::testkit;
using fusion::network::Effect;
using fusion::network::EdgeKey;
using fusion::network::NodeKey;
using solve::Phase;
using solve::Point;
using testkit::ScriptedOracle;

// ---- the spec's polarity table (S5.13), the input a corridor's CaseGraph carries (S6.14)
bool spec_relaxed(Effect, int source, int target);         // states 0 absent, 1 inactive, 2 active
std::array<std::uint16_t, 2> spec_polarity();             // as CaseGraph::relaxed_states

// ---- hand-built corridors and lists ----
class Graph {
public:
  explicit Graph(std::uint32_t bound = 10);
  std::uint32_t node(const std::string& key, std::vector<std::string> contains = {});
  // subtypes joined by ','; an edge between unknown keys adds them as nodes
  std::uint32_t edge(const std::string& source, const std::string& target, const std::string& subtypes = "activation",
                     double score = 0, const std::string& type = "PPrel", bool unscored = false);
  Graph& ends(const std::string& source, const std::string& target);
  std::uint32_t index(const std::string& key) const;
  solve::CaseGraph g;
};
std::vector<solve::CaseSpec::Id> ids(std::initializer_list<std::string> keys);
// An observed gene whose id is its node's key; outside the graph it is not in the network.
void observe(solve::CaseSpec&, const solve::CaseGraph&, const std::string& gene, char direction);
solve::Problem problem_of(const solve::CaseGraph&, const solve::CaseSpec&, const solve::Settings&, solve::Case);
const solve::Problem& built(const std::variant<solve::Problem, solve::Skipped>&);
std::uint32_t at(const solve::Problem&, const std::string& key);   // a node's index in N
std::vector<Point> points_of(const solve::Frontier&);

// ---- the kit's judgement of one case (rule R1, 06.C20)
struct CaseRef {                                           // a case as the solve receives it
  const solve::CaseGraph& graph;
  const solve::CaseSpec& spec;
  const solve::Settings& settings;
  solve::Case kase;
};
// The kit's verdict on a witness under 2.0's rules (Semantics::fusion2), plus what S5.26 and S5.27 ask of
// the witness itself: its counts equal the checker's cost, and its relaxation lists name exactly the
// relaxed nodes and edges, each with its presence or selection.
testkit::Verdict kit_check(const CaseRef&, const solve::Witness&);
// The exhaustive frontier of the case in the settings' box (06.C20: a lower witness counts at the corner).
std::vector<Point> kit_frontier(const CaseRef&);

// ---- solving in-process ----
struct Quiet final : solve::SearchObserver {
  void on_started(Point, Phase) override {}
  void on_answer(const solve::Answer&) override {}
  void on_point(Point) override {}
};
struct SolverOracle final : solve::Oracle {
  explicit SolverOracle(solve::CaseSolver& s) : solver(s) {}
  solve::Answer ask(Point p, Phase phase) override { return solver.check(p, phase); }
  solve::CaseSolver& solver;
};
struct Solved {
  std::vector<Point> search;
  solve::Frontier frontier;
  std::vector<solve::Answer> log;
  std::vector<solve::Witness> witnesses;
};
Solved solve_problem(const solve::Problem&, const solve::Settings& = {});

// ---- FuSION 1's toy cases (backend/tests/reference/synthetic/, booking 1), read with the kit's readers
struct ToyRun {                                            // one pathz3 line: with or without the FSC
  solve::CaseSpec spec;
  std::vector<Point> dat;                                  // FuSION 1's frontier
  testkit::f1::Limits limits;                              // its search: checks and cache hits, in order
  solve::Box box() const;                                  // the limits file's window
};
struct ToyCase {
  std::string name, fsc;
  solve::CaseGraph graph;
  solve::Box box;
  solve::CaseSpec observations;                            // up and down only
  ToyRun with, without;
};
std::filesystem::path tests_dir();
std::vector<std::string> toy_names();
ToyCase read_toy(const std::string& name);

// ---- a generated tiny case (testkit::tiny_case) as the solve's inputs
struct TinyInputs {
  solve::CaseGraph graph;
  solve::CaseSpec spec;
  solve::Settings settings;
  solve::Case kase = solve::Case::free;
};
TinyInputs inputs_of(const testkit::TinyCase&);
std::string describe(const testkit::TinyCase&);            // the case as text, for a kept failing input

}  // namespace solvekit
