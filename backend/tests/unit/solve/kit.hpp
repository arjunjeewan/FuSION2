// The solve tests' own kit (section 12's solve row; the shared kit waits for Phase 3): hand-built
// cases, an independent witness checker, an exhaustive frontier, a scripted oracle, and readers for
// FuSION 1's regenerated toy cases (booking 1). The checker and the frontier share no code with the
// solve: they read the spec's polarity table (S5.13), not 02's rule (12, "Independent oracles").
#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <initializer_list>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "fusion/solve/solve.hpp"

namespace solvekit {

namespace solve = fusion::solve;
using fusion::network::Effect;
using fusion::network::EdgeKey;
using fusion::network::NodeKey;
using solve::Phase;
using solve::Point;

// ---- the spec's polarity table (S5.13): states 0 absent, 1 inactive, 2 active ----
bool spec_relaxed(Effect, int source, int target);
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

// ---- the independent checker (06's witness checker; 12's rule R1) ----
struct Verdict {
  bool valid = true;
  Point cost;
  std::vector<std::string> violations;
};
// An assignment: per node 0 absent, 1 inactive, 2 active; per edge selected or not.
Verdict check(const solve::Problem&, const std::vector<int>& state, const std::vector<bool>& selected);
Verdict check(const solve::Problem&, const solve::Witness&);
// Every assignment, 3^|N| * 2^|E| (keep |N| <= 7 and |E| <= 9): the Pareto-minimal costs in the box.
std::vector<Point> exhaustive_frontier(const solve::Problem&);
std::vector<Point> points_of(const solve::Frontier&);

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

// ---- 12's scripted oracle: a monotone staircase with overrides ----
class ScriptedOracle final : public solve::Oracle {
public:
  explicit ScriptedOracle(std::vector<Point> staircase, std::map<Point, solve::Outcome> overrides = {});
  solve::Answer ask(Point, Phase) override;
  bool truth(Point) const;                                 // SAT under the staircase
  int calls(Point) const;
  std::function<std::optional<Point>(Point)> model;        // a SAT answer's model counts, if set
  std::vector<std::pair<Point, Phase>> asked;
  std::vector<solve::Witness> witnesses;                   // one per SAT answer, with its counts
private:
  std::vector<Point> staircase_;
  std::map<Point, solve::Outcome> overrides_;
  std::map<Point, int> calls_;
};

// ---- FuSION 1's toy cases (backend/tests/reference/synthetic/, booking 1) ----
struct Limits {                                            // one limits_timefile.txt
  solve::Box box;
  struct Query { Point point; Phase phase; bool z3; bool sat; };
  std::vector<Query> queries;                              // checks and cache hits, in order
  std::vector<Point> found;                                // its "Point 1:" and "Point 2:" lines
};
struct ToyRun {                                            // one pathz3 line: with or without the FSC
  solve::CaseSpec spec;
  std::vector<Point> dat;                                  // FuSION 1's frontier
  Limits limits;
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
Limits read_limits(const std::filesystem::path&);
std::vector<Point> read_dat(const std::filesystem::path&);

}  // namespace solvekit
