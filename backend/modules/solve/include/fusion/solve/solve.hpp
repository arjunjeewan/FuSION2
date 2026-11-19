// fusion/solve/solve.hpp: the pathz3 step: building each FSC case from one corridor, the Z3
// encoding, the Pareto search with its answer cache, certification, witnesses and the case files
// (section 06, "Interface"; spec stage 5, S5.01 to S5.31).
// The declarations down to solve() are section 06's outline. Everything marked "Added" extends it.
// z3++.h is included only by this module's sources, so no other module sees Z3 (11.C17).
#pragma once

#include <array>
#include <chrono>
#include <compare>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "fusion/core/core.hpp"
#include "fusion/expression/expression.hpp"
#include "fusion/network/network.hpp"

namespace fusion::corridor { class Corridor; }   // section 05; solve.cpp includes its header

namespace fusion::solve {

// ---- settings: registry keys solve.<name> (1.16), recorded in run.json (E1) ----
enum class Case : std::uint8_t { in, out, free };                     // 06.C10; names in files and JSON (1.6)

struct Point {                                                         // (n, e), n always first
  int n = 0;                                                           // node relaxations
  int e = 0;                                                           // edge relaxations
  friend constexpr auto operator<=>(const Point&, const Point&) = default;
};
constexpr bool below_or_equal(Point a, Point b) noexcept {             // a.n <= b.n && a.e <= b.e
  return a.n <= b.n && a.e <= b.e;
}

struct Box { Point lo{0, 0}; Point hi{8, 8}; };                        // node_min..edge_max; 06.C17

enum class Retry : std::uint8_t { automatic, unknown, none };          // 06.C8

struct Families {                                                      // S5.03, S5.15, S5.16
  std::optional<bool> enabled;                                         // the constraints flag, if given
  std::optional<double> coexpression_threshold;                        // strict >
  std::optional<double> frozen_threshold;                              // strict >
};

struct Settings {                                                      // filled from core::Settings
  std::vector<std::string> fsc;                                        // gene ids; empty = none
  std::vector<Case> cases;                                             // empty = {in, out} or {free}
  Box box;
  std::vector<Point> points;                                           // non-empty = these points only
  std::chrono::milliseconds timeout{14'400'000};                       // 4h (06.C2)
  std::chrono::milliseconds incremental_timeout{3'600'000};            // 1h (06.C2)
  std::chrono::milliseconds budget{0};                                 // 0 = none (06.C3)
  std::uint64_t rlimit = 0;                                            // Z3 steps per check; 0 = none (2B.8)
  std::uint32_t seed = 42;                                             // S5.22
  int solutions = 1;                                                   // 06.C11
  unsigned threads = 0;                                                // 0 = one per case (06.C4)
  Retry retry = Retry::automatic;                                      // 06.C8
  std::optional<std::filesystem::path> answers_from;                   // 06.C12
  Families families;
  bool dumps = false;                                                  // the debug option (S1.29)
  // Added: what a FuSION 1 positional line (S5.01) brings besides its lists. It passes its lists
  // as written, with only the source forced active (06.C16), and its bound must equal the
  // corridor's (05.C7).
  bool positional = false;
  std::optional<std::uint32_t> path_bound;
};

// ---- inputs ----
struct NodeLists {                                                     // S5.05, ids as written
  std::vector<std::string> essential, avoid, active, inactive, non_relaxed,
                           confirmed_up, confirmed_down;
};
struct EdgeLine {                                                      // S5.06
  std::string source, target;
  std::vector<std::string> subtypes;                                   // required; compared as a set
  int line = 0;
};
struct EdgeLists { std::vector<EdgeLine> essential, avoid, non_relaxed; };

class CoexpressionTable {                                              // parsed once per run (S5.15)
public:
  struct Row {
    std::vector<std::string> ids1, ids2;                               // each cell's ids, ':' removed
    double score = 0;
    std::uint32_t line = 0;
  };
  // Added: the parser behind read_coexpression(), for text already in memory. Columns are found
  // by their header names (S5.15's fix); a row without a usable score is skipped and counted.
  static CoexpressionTable parse(std::string_view text, std::string source_name);
  std::span<const Row> rows() const noexcept { return rows_; }
  std::span<const std::uint32_t> skipped_lines() const noexcept { return skipped_; }
  const std::string& source_name() const noexcept { return source_; }
private:
  std::vector<Row> rows_;
  std::vector<std::uint32_t> skipped_;
  std::string source_;
};
CoexpressionTable read_coexpression(const std::filesystem::path&, core::Context&);  // throws core::Failure

struct Inputs {
  const corridor::Corridor& corridor;                                  // 05; read only (S4.20)
  const expression::Observations& observations;                        // 04; up and down sets
  // Changed from the outline's single `const expression::ObservationMatch&`: 05.C9's match is one
  // row per observed gene, as match_observations() returns it.
  std::span<const expression::ObservationMatch> match;                 // 05.C9: outside_corridor, not_in_network
  network::BindingTable& bindings;                                     // 02; the run's bindings.tsv (1.22)
  NodeLists nodes;
  EdgeLists edges;
  const CoexpressionTable* coexpression = nullptr;
  // Added: the run's maps, for symbols in observations.tsv and labels in messages (07.C10);
  // and a positional line's connect pair, which must match the corridor (S5.04, 05.C7).
  const network::Maps* maps = nullptr;
  std::optional<std::pair<std::string, std::string>> connect_pair;
  // Added: the interner the observations' ids belong to (the run's, 02.C3). A Network's handles are
  // its own, so match[i].id is read here, never through the network; left out, the network's is used.
  const core::Interner* interner = nullptr;
};

// ---- one case ----
enum class Role : std::uint8_t {                                       // 07's seven words (1.5)
  counted, forced, exempt_endpoint, removed_with_fsc,
  outside_encoding, outside_corridor, not_in_network
};
enum class CaseStatus : std::uint8_t {                                 // owned here (1.5); not Reports (1.8)
  complete, no_point, infeasible, no_de_gene, stopped, budget_exhausted, not_requested
};

struct ObservationRow { std::string gene, symbol; char observed; Role role; };   // observed: 'u' or 'd'

// The case as the encoder sees it (06, "Building a case"): plain data in corridor index order
// (S4.14), immutable once built. Node and edge indices are positions in `nodes` and `edges`.
struct Problem {
  struct Edge {
    std::uint32_t src = 0, tgt = 0;                                    // indices into nodes
    network::EdgeKey key;                                              // 02.C11
    network::Effect effect = network::Effect::activation;              // S5.13
    double score = 0;                                                  // S5.16
    bool unscored = false;                                             // 02's flag (1.25)
  };
  struct FamilyUse {                                                   // S5.03: what was used, and why
    bool coexpression = false, frozen = false;
    std::optional<double> coexpression_threshold, frozen_threshold;
    std::uint32_t coexpression_rows = 0;                               // table rows above the threshold
  };

  Case kase = Case::free;
  bool named = true;                                                   // false: a positional line (06.C16)
  std::vector<network::NodeKey> nodes;                                 // N (S5.10)
  std::vector<std::string> labels;                                     // per node, for messages (07.C10)
  std::vector<Edge> edges;                                             // E, without self-loops (S5.13)
  std::uint32_t source = 0, target = 0;                                // indices into nodes
  std::uint32_t bound = 0;                                             // the corridor's (S4.21, 05.C7)

  std::vector<expression::Regulation> regulation;                      // per node: the DE set (S5.14)
  std::vector<std::uint32_t> counted;                                  // relaxable observations (S5.12)
  std::vector<std::uint32_t> present, absent, active, inactive;        // node constraints (S5.05, S5.17)
  std::vector<std::uint32_t> consistent;                               // non-relaxed observations (S5.05)
  std::vector<std::uint32_t> edge_selected, edge_avoided, edge_nonrelaxed;   // S5.06
  std::vector<std::uint32_t> frozen;                                   // S5.16
  std::vector<std::pair<std::uint32_t, std::uint32_t>> coexpressed;    // u < v (S5.15)
  // 02's polarity rule (S5.13, S6.14), per network::Effect: bit 3 * source + target, with
  // network::EndState's values, is set when an edge between ends in those states is relaxed.
  std::array<std::uint16_t, 2> relaxed_states{};

  Box requested, box;                                                  // the box asked, and after the clamps
  std::uint32_t seed = 42;                                             // S5.22

  // For the case's files and console (S5.12, S5.16, S5.31), not for the encoding.
  std::vector<std::string> fsc;                                        // the FSC's node keys
  std::vector<ObservationRow> observations;                            // 07's seven roles (1.5)
  FamilyUse families;
  std::uint32_t unscored = 0;                                          // encoded edges with no score
  std::uint32_t outside_encoding = 0;                                  // corridor nodes not in N
  std::vector<std::pair<std::string, std::string>> warnings;           // (code, text), for ctx.log
};
struct Skipped {                                                       // infeasible or no_de_gene
  CaseStatus status;
  std::string message;
  std::shared_ptr<const Problem> problem;                              // Added: the case as built, for its files
};
std::variant<Problem, Skipped> build_case(const Inputs&, const Settings&, Case, core::Context&);
core::Sha256 fingerprint(const Problem&);

// ---- answers and witnesses ----
enum class Phase   : std::uint8_t { diagonal, smallest_n, smallest_e, point, extra };
enum class Outcome : std::uint8_t { sat, unsat, timeout, undefined, stopped };
enum class Source  : std::uint8_t { z3, cache, model, transfer, imported };

struct Answer {
  std::uint32_t check = 0;                                             // the case's counter (1.3)
  Point point;
  Phase phase;
  Source source;
  Outcome outcome;
  double wall_s = 0;
  double cpu_s = 0;
  std::chrono::milliseconds timeout{};                                 // the limits it ran under
  std::uint64_t rlimit = 0;
  std::string reason;                                                  // read before pop
  std::optional<Point> model;                                          // the model's own counts
  std::optional<std::uint32_t> witness;                                // index into witnesses
};

struct Witness {                                                       // S5.26, S5.27
  Point counts;
  std::vector<network::NodeKey> active, inactive;                      // present nodes only
  std::vector<network::EdgeKey> selected;                              // 02.C11 keys
  std::vector<std::pair<network::NodeKey, bool>> relaxed_nodes;        // bool: present
  std::vector<std::pair<network::EdgeKey, bool>> relaxed_edges;        // bool: selected
};

class Oracle {                                                         // Z3, or a script in tests
public:
  virtual ~Oracle() = default;
  virtual Answer ask(Point, Phase) = 0;
};

class SearchObserver {
public:
  virtual ~SearchObserver() = default;
  virtual void on_started(Point, Phase) = 0;                           // solve.check.started
  virtual void on_answer(const Answer&) = 0;                           // solve.check, journal
  virtual void on_point(Point) = 0;                                    // solve.point
};

class AnswerCache {                                                    // S5.20, 06.C5
public:
  explicit AnswerCache(Box);
  std::optional<Answer> lookup(Point) const;                           // proofs, then unknowns
  void record(const Answer&);
  std::span<const Answer> answers() const noexcept;
  // Added: what the answers prove about a point, directly or by monotonicity (06.C5, 06.C6).
  bool proven_sat(Point) const noexcept;
  bool proven_unsat(Point) const noexcept;
private:
  std::vector<Answer> answers_;                                        // every query, in order
};

// It returns at the first stopped answer (Stop or the budget; A1, 06.C3), with the points so far.
std::vector<Point> pareto_search(Box, AnswerCache&, Oracle&, SearchObserver&);   // S5.18

struct FrontierPoint {
  Point point;
  bool certified = false;                                              // 06.C6
  std::vector<Point> undecided_below;
  std::optional<Point> found_at;                                       // the search's point, if replaced (06.C20)
  std::uint32_t witness = 0;
  std::vector<std::uint32_t> more;                                     // solutions 2..k
};
struct Frontier {
  std::vector<FrontierPoint> points;                                   // ascending n
  std::vector<Point> undecided;
};
Frontier assemble(Box, std::span<const Answer>, std::span<const Witness>);
// Added: as above, with the search's own points, so a replaced point names the point it
// replaces in found_at (06.C20). The three-argument form names where the witness was found.
Frontier assemble(Box, std::span<const Answer>, std::span<const Witness>,
                  std::span<const Point> search_points);

// ---- Z3 ----
void verify_z3();                                                      // throws core::InstallError (11.C17)
std::string z3_version();                                              // "4.8.12.0", from Z3_get_version()

class CaseSolver {                                                     // one context, one thread
public:
  CaseSolver(const Problem&, const Settings&, core::Context&);         // context built lazily
  CaseSolver(const Problem&, const Settings&);                         // Added: without a run context
  ~CaseSolver();
  CaseSolver(const CaseSolver&) = delete;
  CaseSolver& operator=(const CaseSolver&) = delete;
  Answer check(Point, Phase);
  std::optional<Witness> next_solution(Point, std::span<const Witness> seen);
  void interrupt() noexcept;                                           // from another thread
  // Added: the witnesses of this solver's SAT answers, which Answer::witness indexes; the answer
  // of the last next_solution() (phase extra, S5.24); and whether interrupt() was called.
  std::span<const Witness> witnesses() const noexcept;
  const Answer& last_answer() const noexcept;
  bool interrupted() const noexcept;
  struct Impl;
private:
  std::unique_ptr<Impl> impl_;
};

// ---- the step ----
// ObservationRow is defined above Problem, which holds the rows.
struct CaseStats {                                                  // sizes, counts, times
  std::uint32_t nodes = 0, edges = 0, counted = 0, de = 0;             // the encoded case (S5.10)
  std::uint32_t coexpression_pairs = 0, frozen_edges = 0, unscored_edges = 0;   // S5.15, S5.16
  std::uint32_t z3 = 0, cache = 0, model = 0;                          // answers by source
  double wall_s = 0, cpu_s = 0;
  double cpu_clock_s = 0;                                              // the CPU clock's resolution
  std::string fingerprint;                                             // "sha256:<hex>"
};

struct CaseResult {
  Case kase;
  CaseStatus status;
  std::string message;
  Frontier frontier;                                                   // empty for points requests
  std::vector<Answer> log;                                             // every query, in order
  std::vector<Witness> witnesses;
  std::vector<ObservationRow> observations;
  CaseStats stats;
  std::shared_ptr<const Problem> problem;                              // Added: the case as encoded
};
struct Results { std::vector<CaseResult> cases; };                     // 07 builds SolveResult from it

Results load_saved(const std::filesystem::path& step_dir);             // journal replay, no Z3 (D1)

// Never carries a Report: a solve's outcomes are case statuses (1.8). On a stop it returns the
// partial Results; the pathz3 handler publishes them (1.5) and then throws core::Stopped.
core::Outcome<Results> solve(const Inputs&, const Settings&, core::Context&);

// ---- Added beyond the outline ----

// The settings registry for pathz3's names (1.7, 1.16): keys solve.<name>, with 06's defaults.
std::span<const core::SettingSpec> settings();

// Names in files, JSON and events (1.6), and the case names' parser.
std::string_view name(Case) noexcept;
std::string_view name(CaseStatus) noexcept;
std::string_view name(Role) noexcept;
std::string_view name(Phase) noexcept;
std::string_view name(Outcome) noexcept;
std::string_view name(Source) noexcept;
std::optional<Case> parse_case(std::string_view) noexcept;

// The list files of S5.05 and S5.06: whitespace-separated ids, and edge lines
// "<source> <target> <subtypes>", subtypes joined by '_' (FuSION 1's form) or ','; '-' is none.
// A line without the subtype field fails as solve.edge_line_no_subtype (S5.06).
std::vector<std::string> read_node_list(const std::filesystem::path&);
std::vector<EdgeLine> read_edge_list(const std::filesystem::path&);
EdgeLine parse_edge_line(std::string_view text, int line);

// build_case's core, which needs no network: the corridor read once into plain data (CaseGraph),
// and the run's lists, FSC and observations bound to node keys once per solve (CaseSpec), so
// that bindings.tsv gets one row per id (1.22). Tests build both directly.
struct CaseGraph {
  struct Node {
    network::NodeKey key;
    std::string label;                                                 // "EGFR"; the key if no symbol
    std::vector<network::NodeKey> contains;                            // a complex's components, closed
                                                                       // over nested complexes (entry 170)
  };
  struct Edge {
    std::uint32_t src = 0, tgt = 0;                                    // indices into nodes
    network::EdgeKey key;
    network::Effect effect = network::Effect::activation;
    double score = 0;
    bool unscored = false;
  };
  std::vector<Node> nodes;                                             // corridor index order (S4.14)
  std::vector<Edge> edges;
  std::uint32_t source = 0, target = 0;
  std::uint32_t bound = 0;
  std::array<std::uint16_t, 2> relaxed_states{};                       // as in Problem (S6.14)
};
struct CaseSpec {
  struct Id { std::string given; network::NodeKey node; };             // a list id and the node it binds to
  std::vector<Id> essential, avoid, active, inactive, non_relaxed, confirmed_up, confirmed_down, fsc;
  std::vector<std::uint32_t> essential_edges, avoid_edges, nonrelaxed_edges;   // CaseGraph edges (S5.06)
  struct Observation {                                                 // one observed gene of the run (05.C9)
    std::string gene, symbol;
    expression::Regulation regulation = expression::Regulation::none;
    expression::MatchGroup group = expression::MatchGroup::not_in_network;
    std::optional<network::NodeKey> node;
  };
  std::vector<Observation> observations;                               // up, then down
  bool coexpression_table = false;                                     // a CSV was given (S5.03)
  struct Pair { network::NodeKey a, b; double score = 0; };
  std::vector<Pair> coexpression;                                      // every resolved cross pair of a row
};
CaseGraph read_corridor(const corridor::Corridor&, const network::Maps*);
CaseSpec bind_inputs(const Inputs&, const CaseGraph&, const Settings&, core::Context&);   // bindings.tsv rows,
                                                                                          // warnings
std::variant<Problem, Skipped> make_case(const CaseGraph&, const CaseSpec&, const Settings&, Case);
std::vector<std::uint32_t> match_edge_line(const CaseGraph&, const network::NodeKey& source,
                                           const network::NodeKey& target,
                                           std::span<const std::string> subtypes);   // S5.06: as a set
std::vector<Case> requested_cases(const Settings&);                    // 06.C10's defaults

// solve()'s second half, for cases already built: each case in its own thread and context unless
// `threads` says otherwise (06.C4), under the budget (06.C3) and Stop (A1), with its files and
// events. solve() is bind_inputs(), then make_case() for each requested case, then this.
core::Outcome<Results> solve_cases(std::vector<std::variant<Problem, Skipped>> cases, const Settings&,
                                   core::Context&);

// The label of an unknown answer (06.C2): stopped if this solve interrupted the check; timeout
// if the reason, read before pop, contains "timeout", or if an rlimit is set and the reason names
// a resource limit or is "canceled"; undefined otherwise.
Outcome unknown_outcome(std::string_view reason, bool rlimit_set, bool interrupted) noexcept;

}  // namespace fusion::solve
