// fusion/results/model.hpp: the published form of a solve, results::SolveResult, built from 06's
// solve::Results (section 07, "From solve::Results to SolveResult"; 07.C1, cross-section item 1.5).
// The declarations up to build() are section 07's outline; everything marked "Added" extends it.
#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "fusion/core/core.hpp"
#include "fusion/corridor/corridor.hpp"
#include "fusion/expression/expression.hpp"
#include "fusion/network/network.hpp"
#include "fusion/solve/solve.hpp"

namespace fusion::results {

using solve::Case;                              // in, out, free (06.C10)
using solve::CaseStatus;                        // complete, no_point, infeasible, no_de_gene,
                                                // stopped, budget_exhausted, not_requested (1.5)
using solve::Point;                             // {n, e}: n node relaxations, e edge relaxations
using NodeIx = std::uint32_t;                   // index into Tables::nodes
using EdgeIx = std::uint32_t;                   // index into Tables::edges

enum class Observed : std::uint8_t { none, up, down };

struct Member {
  std::string id, symbol;
  std::optional<double> lfc;
  Observed observed = Observed::none;
  bool operator==(const Member&) const = default;
};

struct NodeRecord {
  network::NodeKey key;                         // the rep id (S4.14)
  std::string label;                            // 07.C10: "EGFR", "RRAS2+5", "[STAT1+6]"
  network::NodeKind kind = network::NodeKind::gene;   // gene, complex, compound, other
  std::vector<Member> members;                  // sorted by id; values the solve used (04, 4A.13)
  Observed observed = Observed::none;           // the class the solve used
  std::optional<std::int32_t> from_source, to_target;   // corridor distances (S4.07)
  bool operator==(const NodeRecord&) const = default;
};

struct EdgeRecord {
  network::EdgeKey key;                         // source, target, type, ordered subtypes (02.C11)
  NodeIx source = 0, target = 0;
  std::string type;                             // "PPrel", "ECrel", "GErel", "PCrel" (S2.31)
  std::vector<std::string> subtypes;            // stored order
  network::Effect effect = network::Effect::activation;   // the one polarity rule, fixed at solve time (S6.14)
  std::vector<std::string> pathways;            // sorted ids
  bool frozen = false;                          // cannot be relaxed in this solve (S5.16)
  bool operator==(const EdgeRecord&) const = default;
};

struct Tables {
  std::vector<NodeRecord> nodes;                // sorted by key (core::stable_compare)
  std::vector<EdgeRecord> edges;                // sorted by key
  std::map<std::string, std::string> pathway_names;   // referenced ids only (S1.19, entry 205)
  bool operator==(const Tables&) const = default;
};

struct Route {                                  // S5.13: over selected edges whose effect is honoured
  bool exists = false;
  std::vector<NodeIx> nodes;                    // the first shortest route, neighbours taken in key order
  std::vector<NodeIx> fsc_on_route;             // the FSC nodes on some such route; in case only
  bool operator==(const Route&) const = default;
};

struct Witness {                                // sorted, unique lists (06's Witness, re-indexed)
  std::vector<NodeIx> active, inactive;         // present nodes only (S5.27)
  std::vector<NodeIx> relaxed_present, relaxed_absent;   // relaxed nodes, by presence
  std::vector<EdgeIx> selected;
  std::vector<EdgeIx> relaxed_selected, relaxed_unselected;   // S5.13; never a self-loop
  Route route;                                  // filled by analyse()
  Point counts() const noexcept;                // relaxed nodes, relaxed edges
  bool operator==(const Witness&) const = default;
};

enum class Enumeration : std::uint8_t { not_requested, complete, limit_reached, stopped_unknown };

struct FrontierPoint {
  Point at;                                     // equals witnesses[0].counts() (06.C20)
  bool certified = false;                       // 06.C6
  std::vector<Point> undecided_below;           // the cells that block it
  std::vector<Witness> witnesses;               // [0] is the search's own model; more on request (06.C11)
  Enumeration enumeration = Enumeration::not_requested;
  bool operator==(const FrontierPoint&) const = default;
};

struct RequestedPoint {                         // 06.C9: points=n,e;… asks without a search
  Point at;
  solve::Outcome answer = solve::Outcome::undefined;   // sat, unsat, or an unknown with its label
  std::string label;                            // TIMEOUT, UNDEFINED or stopped (S5.19)
  std::vector<Witness> witnesses;               // empty unless sat
  bool operator==(const RequestedPoint&) const = default;
};

struct UndecidedCell {                          // last answer there: TIMEOUT, UNDEFINED, stopped, or not asked
  Point at;
  std::string label;
  bool operator==(const UndecidedCell&) const = default;
};

struct CheckCounts {
  std::uint32_t sat = 0, unsat = 0, unknown = 0;
  bool operator==(const CheckCounts&) const = default;
};

enum class ObservationStatus : std::uint8_t {   // one vocabulary, also in 06's observations.tsv (1.5)
  counted,            // in the relaxation count (S5.12)
  forced,             // an FSC gene in the in case: held consistent, never relaxed (S5.17)
  exempt_endpoint,    // the source or the target (S5.12)
  removed_with_fsc,   // an FSC gene in the out case (S5.17)
  outside_encoding,   // in the corridor but not in the encoding (S5.10)
  outside_corridor,   // in the network but not in the corridor (S4.16, 05.C9)
  not_in_network      // in no node of the network (S3.12, 05.C9)
};

struct Observation {
  std::string gene;
  Observed observed = Observed::none;
  std::optional<NodeIx> node;
  ObservationStatus status = ObservationStatus::counted;
  bool operator==(const Observation&) const = default;
};

struct CaseResult {
  Case kind = Case::free;
  CaseStatus status = CaseStatus::not_requested;   // 06's, as given
  std::string label;                            // display only: "with FSC (JUN)" (1.6)
  std::string status_text;                      // display words for the status (below)
  std::string message;                          // 06's, for infeasible and the like
  Point requested, searched;                    // upper corners of the box, before and after 06's clamp (S5.07)
  std::vector<FrontierPoint> points;            // ascending n; empty for a points request
  std::vector<RequestedPoint> requested_points; // 06.C9
  std::vector<UndecidedCell> undecided;         // 06.C6
  CheckCounts checks;
  std::vector<Observation> observations;        // sorted by gene
  bool operator==(const CaseResult&) const = default;
};

struct CorridorSummary {
  std::uint32_t nodes = 0, edges = 0;
  std::int32_t bound = 0;
  std::string artefact;
  bool operator==(const CorridorSummary&) const = default;
};

struct Header {
  std::string name = "solve";                   // "solve" by default, or a positional prefix (07.C9)
  std::string step = "pathz3";                  // "pathz3", "pathz3-2" (09.C2)
  NodeIx source = 0, target = 0;                // always in Tables
  std::vector<NodeIx> fsc;                      // the FSC set (06.C10); empty without one
  std::int32_t bound = 0;
  std::uint64_t seed = 0;                       // S5.22
  std::optional<double> coexpression_threshold, frozen_threshold;   // only the families used (S5.03)
  std::vector<std::string> rules;               // modelling statements (S5.12, S5.14, S5.16)
  CorridorSummary corridor;                     // from 05
  bool operator==(const Header&) const = default;
};

struct SolveResult {
  Header header;
  Tables tables;
  std::vector<CaseResult> cases;
  bool operator==(const SolveResult&) const = default;
};

struct PublishInputs {                          // what the batch layer hands over after the solve step
  const solve::Results& results;                // 06 (1.5)
  const solve::Inputs& inputs;                  // endpoints, lists and the coexpression table (06)
  const corridor::Corridor& corridor;           // node and edge attributes, distances, summary (05)
  std::span<const expression::ObservationMatch> match;   // 05.C9's three groups. The outline's
                                                // `const ObservationMatch&` names one gene's record;
                                                // every observed gene's record is needed.
  const network::Maps& maps;                    // labels and pathway names (02, entry 205)
  std::string name = "solve";                   // the solve's name (07.C9)
  // Added: what solve::Inputs does not carry. The FSC set, the box, the seed, `solutions` and the
  // constraint families are the solve's settings (06); the values are the ones the solve's
  // observations came from, absent when they came from lists (S3.09, 4A.13).
  const solve::Settings& settings;
  const expression::GeneValues* values = nullptr;
  std::string corridor_artefact;                // the corridor's saved file, e.g. "fb_rch/corridor.json"
  // The interner of the run's observations, values and observation match (the batch's). A network
  // keeps an interner of its own, so a gene's id in the one means nothing in the other: genes are
  // matched by text. Without it, members get no value and are not matched to the observations.
  const core::Interner* interner = nullptr;
};

SolveResult build(const PublishInputs&);        // O(V + E + Σ witness sizes)

// ---- Added beyond the outline ----

// The modelling statements of S5.12, S5.14 and S5.16, as results.json names them, and the
// sentence the pages and the run page print for each.
inline constexpr std::string_view rule_de_on_every_path = "every-path-crosses-a-de-node";
inline constexpr std::string_view rule_absent_down = "absent-down-is-consistent";
inline constexpr std::string_view rule_endpoints_exempt = "endpoints-exempt";
inline constexpr std::string_view rule_strict_scores = "scores-strictly-above-threshold";
std::string rule_sentence(std::string_view rule);

// Names in files and JSON (1.6); parse_* return nothing for an unknown word.
std::string_view case_name(Case) noexcept;                       // "in", "out", "free"
std::string_view status_name(CaseStatus) noexcept;               // "complete", "no_point", ...
std::string_view observed_name(Observed) noexcept;               // "none", "up", "down"
std::string_view observation_status_name(ObservationStatus) noexcept;   // "counted", ...
std::string_view enumeration_name(Enumeration) noexcept;         // "not_requested", ...
std::string_view kind_name(network::NodeKind) noexcept;          // "gene", "complex", "compound", "other"
std::string_view effect_name(network::Effect) noexcept;          // "activation", "inhibition"
std::string_view outcome_name(solve::Outcome) noexcept;          // "sat", "unsat", "timeout", ...
std::optional<Case> parse_case(std::string_view) noexcept;
std::optional<CaseStatus> parse_status(std::string_view) noexcept;
std::optional<Observed> parse_observed(std::string_view) noexcept;
std::optional<ObservationStatus> parse_observation_status(std::string_view) noexcept;
std::optional<Enumeration> parse_enumeration(std::string_view) noexcept;
std::optional<network::NodeKind> parse_kind(std::string_view) noexcept;
std::optional<network::Effect> parse_effect(std::string_view) noexcept;
std::optional<solve::Outcome> parse_outcome(std::string_view) noexcept;

// The label of an unknown answer (S5.19): "TIMEOUT", "UNDEFINED" or "stopped"; empty otherwise.
std::string_view unknown_label(solve::Outcome) noexcept;

// Display words (section 07's table): "6 Pareto points, all certified", "no Pareto point within the
// bounds", "infeasible: …", "not solved in this run" and so on.
std::string status_text(const CaseResult&);

// Display labels (1.6, S6.17): "with FSC (JUN)", "without FSC (RAC1, RAC2+1)", "unconstrained", or,
// for a positional line's free case, "solve temp_w" from the solve's name.
std::string case_label(Case, std::span<const std::string> fsc_labels, std::string_view solve_name);

// "n=2 e=6", the console's and the messages' way of naming a point (S5.31).
std::string point_text(Point);

// What build() takes from 02, 04 and 05 for each node and edge it publishes. build(PublishInputs)
// reads it from the corridor, the maps and the values; tests script it.
class Catalogue {
 public:
  virtual ~Catalogue() = default;
  // The record without its index fields: label, kind, members, observed class and distances.
  virtual std::optional<NodeRecord> node(const network::NodeKey&) const = 0;
  // The record without source and target indices: type, subtypes, effect, pathways, frozen.
  virtual std::optional<EdgeRecord> edge(const network::EdgeKey&) const = 0;
  // The corridor node an observed gene binds to (S2.03, 05.C9); nothing outside the corridor.
  virtual std::optional<network::NodeKey> node_of(std::string_view gene) const = 0;
  virtual std::string pathway_name(std::string_view pathway) const = 0;   // KGML title, map, else the id
};

struct SolveSpec {                              // the solve's settings that results.json records
  std::string name = "solve";
  std::string step = "pathz3";
  network::NodeKey source, target;
  std::vector<network::NodeKey> fsc;            // the FSC set's nodes
  std::int32_t bound = 0;
  std::uint64_t seed = 42;
  std::optional<double> coexpression_threshold, frozen_threshold;   // only the families used
  std::vector<std::string> rules;
  CorridorSummary corridor;
  Point requested{8, 8};                        // the box's upper corner as asked (06.C17)
  int solutions = 1;                            // 06.C11: witnesses asked per point
};

SolveResult build(const solve::Results&, const SolveSpec&, const Catalogue&);

}  // namespace fusion::results
