// results/src/model.cpp: the names of the model's words, the display words and labels, and build(),
// which turns 06's solve::Results into the published SolveResult (section 07, "From solve::Results to
// SolveResult"; 07.C1, 07.C10, S5.19, S5.26, S5.27, cross-section item 1.5).
#include "fusion/results/model.hpp"

#include <algorithm>
#include <array>
#include <iterator>
#include <map>
#include <set>
#include <utility>

#include "text.hpp"

namespace fusion::results {

namespace {

template <class E, std::size_t N>
std::string_view name_in(const std::array<std::string_view, N>& names, E value) noexcept {
  const auto i = static_cast<std::size_t>(value);
  return i < N ? names[i] : std::string_view{};
}

template <class E, std::size_t N>
std::optional<E> parse_in(const std::array<std::string_view, N>& names, std::string_view word) noexcept {
  for (std::size_t i = 0; i < N; ++i) {
    if (names[i] == word) return static_cast<E>(i);
  }
  return std::nullopt;
}

constexpr std::array<std::string_view, 3> case_names{"in", "out", "free"};
constexpr std::array<std::string_view, 7> status_names{
    "complete", "no_point", "infeasible", "no_de_gene", "stopped", "budget_exhausted", "not_requested"};
constexpr std::array<std::string_view, 3> observed_names{"none", "up", "down"};
constexpr std::array<std::string_view, 7> observation_status_names{
    "counted", "forced", "exempt_endpoint", "removed_with_fsc", "outside_encoding", "outside_corridor",
    "not_in_network"};
constexpr std::array<std::string_view, 4> enumeration_names{
    "not_requested", "complete", "limit_reached", "stopped_unknown"};
constexpr std::array<std::string_view, 4> kind_names{"gene", "complex", "compound", "other"};
constexpr std::array<std::string_view, 2> effect_names{"activation", "inhibition"};
constexpr std::array<std::string_view, 5> outcome_names{"sat", "unsat", "timeout", "undefined", "stopped"};

std::string plural(std::size_t count, std::string_view one, std::string_view many) {
  return std::to_string(count) + " " + std::string(count == 1 ? one : many);
}

}  // namespace

// ---------------------------------------------------------------- names

std::string_view case_name(Case c) noexcept { return name_in(case_names, c); }
std::string_view status_name(CaseStatus s) noexcept { return name_in(status_names, s); }
std::string_view observed_name(Observed o) noexcept { return name_in(observed_names, o); }
std::string_view observation_status_name(ObservationStatus s) noexcept {
  return name_in(observation_status_names, s);
}
std::string_view enumeration_name(Enumeration e) noexcept { return name_in(enumeration_names, e); }
std::string_view kind_name(network::NodeKind k) noexcept { return name_in(kind_names, k); }
std::string_view effect_name(network::Effect e) noexcept { return name_in(effect_names, e); }
std::string_view outcome_name(solve::Outcome o) noexcept { return name_in(outcome_names, o); }

std::optional<Case> parse_case(std::string_view w) noexcept { return parse_in<Case>(case_names, w); }
std::optional<CaseStatus> parse_status(std::string_view w) noexcept {
  return parse_in<CaseStatus>(status_names, w);
}
std::optional<Observed> parse_observed(std::string_view w) noexcept {
  return parse_in<Observed>(observed_names, w);
}
std::optional<ObservationStatus> parse_observation_status(std::string_view w) noexcept {
  return parse_in<ObservationStatus>(observation_status_names, w);
}
std::optional<Enumeration> parse_enumeration(std::string_view w) noexcept {
  return parse_in<Enumeration>(enumeration_names, w);
}
std::optional<network::NodeKind> parse_kind(std::string_view w) noexcept {
  return parse_in<network::NodeKind>(kind_names, w);
}
std::optional<network::Effect> parse_effect(std::string_view w) noexcept {
  return parse_in<network::Effect>(effect_names, w);
}
std::optional<solve::Outcome> parse_outcome(std::string_view w) noexcept {
  return parse_in<solve::Outcome>(outcome_names, w);
}

std::string_view unknown_label(solve::Outcome o) noexcept {
  // S5.19's two labels, plus 06.C2's interrupted check.
  switch (o) {
    case solve::Outcome::timeout: return "TIMEOUT";
    case solve::Outcome::undefined: return "UNDEFINED";
    case solve::Outcome::stopped: return "stopped";
    default: return {};
  }
}

std::string rule_sentence(std::string_view rule) {
  if (rule == rule_de_on_every_path) {
    return "Every selected path crosses a differentially expressed gene before the target (S5.14).";
  }
  if (rule == rule_absent_down) {
    return "An absent up-regulated gene counts as a node relaxation; an absent down-regulated gene does "
           "not (S5.12).";
  }
  if (rule == rule_endpoints_exempt) {
    return "The source and the target are exempt from node relaxation (S5.12).";
  }
  if (rule == rule_strict_scores) {
    return "Coexpression and frozen-edge constraints apply to scores strictly above their thresholds "
           "(S5.15, S5.16).";
  }
  return std::string(rule);
}

// ---------------------------------------------------------------- Witness

Point Witness::counts() const noexcept {
  Point p;
  p.n = static_cast<decltype(p.n)>(relaxed_present.size() + relaxed_absent.size());
  p.e = static_cast<decltype(p.e)>(relaxed_selected.size() + relaxed_unselected.size());
  return p;
}

// ---------------------------------------------------------------- display words and labels

std::string point_text(Point p) { return "n=" + std::to_string(p.n) + " e=" + std::to_string(p.e); }

std::string status_text(const CaseResult& c) {
  // Section 07's table of display words for 06's statuses.
  switch (c.status) {
    case CaseStatus::complete: {
      if (c.points.empty() && !c.requested_points.empty()) {
        // A points request (06.C9) has no frontier: say what each asked point answered.
        std::size_t sat = 0, unsat = 0, undecided = 0;
        for (const RequestedPoint& r : c.requested_points) {
          if (r.answer == solve::Outcome::sat) ++sat;
          else if (r.answer == solve::Outcome::unsat) ++unsat;
          else ++undecided;
        }
        std::string text = plural(c.requested_points.size(), "point asked", "points asked") + ": " +
                           std::to_string(sat) + " sat, " + std::to_string(unsat) + " unsat";
        if (undecided > 0) text += ", " + std::to_string(undecided) + " undecided";
        return text;
      }
      if (c.points.empty()) return "no Pareto point within the bounds";
      const auto uncertified = static_cast<std::size_t>(std::count_if(
          c.points.begin(), c.points.end(), [](const FrontierPoint& p) { return !p.certified; }));
      std::string text = plural(c.points.size(), "Pareto point", "Pareto points") + ", ";
      if (c.points.size() == 1) return text + (uncertified == 0 ? "certified" : "not certified");
      if (uncertified == 0) return text + "all certified";
      if (uncertified == c.points.size()) return text + "none certified";
      return text + std::to_string(uncertified) + " not certified";
    }
    case CaseStatus::no_point:
      if (c.undecided.empty()) return "no Pareto point within the bounds";
      return "no point found; " + plural(c.undecided.size(), "cell", "cells") + " undecided";
    case CaseStatus::infeasible:
      return c.message.empty() ? std::string("infeasible") : "infeasible: " + c.message;
    case CaseStatus::no_de_gene: return "no differentially expressed gene in the corridor";
    case CaseStatus::stopped: return "stopped: partial frontier";
    case CaseStatus::budget_exhausted: return "budget used up: partial frontier";
    case CaseStatus::not_requested: return "not solved in this run";
  }
  return std::string(status_name(c.status));
}

std::string case_label(Case kind, std::span<const std::string> fsc_labels, std::string_view solve_name) {
  // Labels are for display only (1.6). FuSION 1 wrote "with FSC" on every incl page, also for runs
  // without an FSC (plot_explanation.py:545, S6.17); 2.0 takes the case and the FSC from the run.
  const std::string fsc = fsc_labels.empty() ? std::string() : " (" + detail::join(fsc_labels, ", ") + ")";
  switch (kind) {
    case Case::in: return "with FSC" + fsc;
    case Case::out: return "without FSC" + fsc;
    case Case::free: break;
  }
  if (solve_name.empty() || solve_name == "solve") return "unconstrained";
  return "solve " + detail::base_name(solve_name);
}

// ---------------------------------------------------------------- build from a catalogue

namespace {

Observed observed_from(char c) noexcept {
  switch (c) {
    case 'u': case 'U': case '+': return Observed::up;
    case 'd': case 'D': case '-': return Observed::down;
    default: return Observed::none;
  }
}

ObservationStatus status_from(solve::Role r) noexcept {
  switch (r) {
    case solve::Role::counted: return ObservationStatus::counted;
    case solve::Role::forced: return ObservationStatus::forced;
    case solve::Role::exempt_endpoint: return ObservationStatus::exempt_endpoint;
    case solve::Role::removed_with_fsc: return ObservationStatus::removed_with_fsc;
    case solve::Role::outside_encoding: return ObservationStatus::outside_encoding;
    case solve::Role::outside_corridor: return ObservationStatus::outside_corridor;
    case solve::Role::not_in_network: return ObservationStatus::not_in_network;
  }
  return ObservationStatus::not_in_network;
}

bool in_corridor(ObservationStatus s) noexcept {
  return s != ObservationStatus::outside_corridor && s != ObservationStatus::not_in_network;
}

template <class T>
void sort_unique(std::vector<T>& v) {
  std::sort(v.begin(), v.end());
  v.erase(std::unique(v.begin(), v.end()), v.end());
}

class Indexer {
 public:
  Indexer(const std::vector<NodeRecord>& nodes, const std::vector<EdgeRecord>& edges)
      : nodes_(nodes), edges_(edges) {}
  NodeIx node(const network::NodeKey& key) const {
    const auto it = std::lower_bound(nodes_.begin(), nodes_.end(), key,
                                     [](const NodeRecord& r, const network::NodeKey& k) { return r.key < k; });
    return static_cast<NodeIx>(it - nodes_.begin());   // always found: the tables hold every key used
  }
  EdgeIx edge(const network::EdgeKey& key) const {
    const auto it = std::lower_bound(edges_.begin(), edges_.end(), key,
                                     [](const EdgeRecord& r, const network::EdgeKey& k) { return r.key < k; });
    return static_cast<EdgeIx>(it - edges_.begin());
  }

 private:
  const std::vector<NodeRecord>& nodes_;
  const std::vector<EdgeRecord>& edges_;
};

Witness convert(const solve::Witness& w, const Indexer& ix) {
  // 06's witness names nodes and edges by key (S5.27, 02.C11); the published one by table index, its
  // relaxed nodes split by presence and its relaxed edges by selection (S5.13, S6.15).
  Witness out;
  for (const auto& k : w.active) out.active.push_back(ix.node(k));
  for (const auto& k : w.inactive) out.inactive.push_back(ix.node(k));
  for (const auto& [k, present] : w.relaxed_nodes) {
    (present ? out.relaxed_present : out.relaxed_absent).push_back(ix.node(k));
  }
  for (const auto& k : w.selected) out.selected.push_back(ix.edge(k));
  for (const auto& [k, selected] : w.relaxed_edges) {
    (selected ? out.relaxed_selected : out.relaxed_unselected).push_back(ix.edge(k));
  }
  sort_unique(out.active);
  sort_unique(out.inactive);
  sort_unique(out.relaxed_present);
  sort_unique(out.relaxed_absent);
  sort_unique(out.selected);
  sort_unique(out.relaxed_selected);
  sort_unique(out.relaxed_unselected);
  return out;
}

// Which witnesses of a case are published: per frontier point, the search's own model and the extra
// solutions (06.C11); per requested point (06.C9), the witnesses of its sat answers.
struct CasePlan {
  std::vector<std::vector<std::uint32_t>> frontier;   // per frontier point
  std::vector<Point> requested;                        // requested points, in the order first asked
  std::vector<const solve::Answer*> requested_answer; // the last answer at each requested point
  std::vector<std::vector<std::uint32_t>> requested_witnesses;
};

CasePlan plan_case(const solve::CaseResult& c) {
  CasePlan plan;
  const auto n_witnesses = static_cast<std::uint32_t>(c.witnesses.size());
  for (const solve::FrontierPoint& fp : c.frontier.points) {
    std::vector<std::uint32_t> list;
    if (fp.witness < n_witnesses) list.push_back(fp.witness);
    for (const std::uint32_t m : fp.more) {
      if (m < n_witnesses) list.push_back(m);
    }
    plan.frontier.push_back(std::move(list));
  }
  for (const solve::Answer& a : c.log) {
    if (a.phase != solve::Phase::point) continue;
    auto it = std::find(plan.requested.begin(), plan.requested.end(), a.point);
    if (it == plan.requested.end()) {
      plan.requested.push_back(a.point);
      plan.requested_answer.push_back(&a);
      plan.requested_witnesses.emplace_back();
      it = plan.requested.end() - 1;
    }
    const auto i = static_cast<std::size_t>(it - plan.requested.begin());
    plan.requested_answer[i] = &a;
    if (a.witness && *a.witness < n_witnesses) plan.requested_witnesses[i].push_back(*a.witness);
  }
  // Extra solutions at a requested point are asked like a frontier point's (06.C9, 06.C11).
  for (const solve::Answer& a : c.log) {
    if (a.phase != solve::Phase::extra || !a.witness || *a.witness >= n_witnesses) continue;
    const auto it = std::find(plan.requested.begin(), plan.requested.end(), a.point);
    if (it == plan.requested.end()) continue;
    auto& list = plan.requested_witnesses[static_cast<std::size_t>(it - plan.requested.begin())];
    if (std::find(list.begin(), list.end(), *a.witness) == list.end()) list.push_back(*a.witness);
  }
  return plan;
}

Enumeration enumeration_of(const solve::CaseResult& c, Point at, std::size_t witnesses, int solutions) {
  // 06.C11: with solutions = k > 1 the point is asked again, blocking the witnesses so far, until there
  // are k, an unsat ("no more") or an unknown answer.
  if (solutions <= 1) return Enumeration::not_requested;
  const solve::Answer* last = nullptr;
  for (const solve::Answer& a : c.log) {
    if (a.phase == solve::Phase::extra && a.point == at) last = &a;
  }
  if (last != nullptr && last->outcome == solve::Outcome::unsat) return Enumeration::complete;
  if (witnesses >= static_cast<std::size_t>(solutions)) return Enumeration::limit_reached;
  return Enumeration::stopped_unknown;
}

std::string undecided_label(const solve::CaseResult& c, Point at) {
  // The last answer at the cell names why it is undecided (S5.19); a cell never asked says so.
  std::string label = "not asked";
  for (const solve::Answer& a : c.log) {
    if (a.phase == solve::Phase::extra || !(a.point == at)) continue;
    const auto word = unknown_label(a.outcome);
    if (!word.empty()) label = std::string(word);
  }
  return label;
}

CheckCounts count_checks(const solve::CaseResult& c) {
  // The search's Z3 checks, as FuSION 1's limits file counted them (S5.24); cache and model answers
  // (06.C5) and the extra-solution checks (06.C11) are not search checks.
  CheckCounts counts;
  for (const solve::Answer& a : c.log) {
    if (a.source != solve::Source::z3 || a.phase == solve::Phase::extra) continue;
    if (a.outcome == solve::Outcome::sat) ++counts.sat;
    else if (a.outcome == solve::Outcome::unsat) ++counts.unsat;
    else ++counts.unknown;
  }
  return counts;
}

}  // namespace

SolveResult build(const solve::Results& results, const SolveSpec& spec, const Catalogue& catalogue) {
  // Tables hold every node and edge that a witness, an observation or the header refers to, so the
  // source, the target and every FSC node are always there (07.C1).
  std::vector<network::NodeKey> node_keys{spec.source, spec.target};
  node_keys.insert(node_keys.end(), spec.fsc.begin(), spec.fsc.end());
  std::vector<network::EdgeKey> edge_keys;

  std::vector<CasePlan> plans;
  plans.reserve(results.cases.size());
  for (const solve::CaseResult& c : results.cases) plans.push_back(plan_case(c));

  const auto use_witness = [&](const solve::Witness& w) {
    for (const auto& k : w.active) node_keys.push_back(k);
    for (const auto& k : w.inactive) node_keys.push_back(k);
    for (const auto& [k, present] : w.relaxed_nodes) node_keys.push_back(k);
    for (const auto& k : w.selected) edge_keys.push_back(k);
    for (const auto& [k, selected] : w.relaxed_edges) edge_keys.push_back(k);
  };
  for (std::size_t ci = 0; ci < results.cases.size(); ++ci) {
    const solve::CaseResult& c = results.cases[ci];
    for (const auto& list : plans[ci].frontier) {
      for (const std::uint32_t w : list) use_witness(c.witnesses[w]);
    }
    for (const auto& list : plans[ci].requested_witnesses) {
      for (const std::uint32_t w : list) use_witness(c.witnesses[w]);
    }
    for (const solve::ObservationRow& row : c.observations) {
      if (!in_corridor(status_from(row.role))) continue;
      if (auto node = catalogue.node_of(row.gene)) node_keys.push_back(std::move(*node));
    }
  }
  sort_unique(edge_keys);
  for (const network::EdgeKey& k : edge_keys) {
    node_keys.push_back(k.source);
    node_keys.push_back(k.target);
  }
  sort_unique(node_keys);

  SolveResult out;
  Tables& tables = out.tables;
  tables.nodes.reserve(node_keys.size());
  for (const network::NodeKey& key : node_keys) {
    NodeRecord record;
    if (auto found = catalogue.node(key)) {
      record = std::move(*found);
    } else {
      record.label = key.value;                 // a key the catalogue lacks still gets a row
      record.kind = network::NodeKind::other;
    }
    record.key = key;
    std::sort(record.members.begin(), record.members.end(), [](const Member& a, const Member& b) {
      return core::stable_compare(a.id, b.id) < 0;
    });
    tables.nodes.push_back(std::move(record));
  }
  tables.edges.reserve(edge_keys.size());
  for (const network::EdgeKey& key : edge_keys) {
    EdgeRecord record;
    if (auto found = catalogue.edge(key)) {
      record = std::move(*found);
    } else {
      record.type = key.type;
      record.subtypes = key.subtypes;
      record.effect = network::effect_of(key.subtypes);
    }
    record.key = key;
    std::sort(record.pathways.begin(), record.pathways.end(),
              [](const std::string& a, const std::string& b) { return core::stable_compare(a, b) < 0; });
    record.pathways.erase(std::unique(record.pathways.begin(), record.pathways.end()), record.pathways.end());
    tables.edges.push_back(std::move(record));
  }
  const Indexer ix(tables.nodes, tables.edges);
  for (EdgeRecord& e : tables.edges) {
    e.source = ix.node(e.key.source);
    e.target = ix.node(e.key.target);
    for (const std::string& p : e.pathways) {
      if (!tables.pathway_names.contains(p)) tables.pathway_names.emplace(p, catalogue.pathway_name(p));
    }
  }

  Header& h = out.header;
  h.name = spec.name;
  h.step = spec.step;
  h.source = ix.node(spec.source);
  h.target = ix.node(spec.target);
  for (const network::NodeKey& k : spec.fsc) h.fsc.push_back(ix.node(k));
  sort_unique(h.fsc);
  h.bound = spec.bound;
  h.seed = spec.seed;
  h.coexpression_threshold = spec.coexpression_threshold;
  h.frozen_threshold = spec.frozen_threshold;
  h.rules = spec.rules;
  h.corridor = spec.corridor;

  std::vector<std::string> fsc_labels;
  for (const NodeIx f : h.fsc) fsc_labels.push_back(tables.nodes[f].label);

  for (std::size_t ci = 0; ci < results.cases.size(); ++ci) {
    const solve::CaseResult& c = results.cases[ci];
    const CasePlan& plan = plans[ci];
    CaseResult r;
    r.kind = c.kase;
    r.status = c.status;
    r.message = c.message;
    r.label = case_label(c.kase, fsc_labels, spec.name);
    // The box's upper corners as asked and as 06 clamped them (S5.07, 06.C14).
    r.requested = c.problem ? c.problem->requested.hi : spec.requested;
    r.searched = c.problem ? c.problem->box.hi : r.requested;
    for (std::size_t pi = 0; pi < c.frontier.points.size(); ++pi) {
      const solve::FrontierPoint& fp = c.frontier.points[pi];
      FrontierPoint p;
      p.at = fp.point;
      p.certified = fp.certified;
      p.undecided_below = fp.undecided_below;
      for (const std::uint32_t w : plan.frontier[pi]) p.witnesses.push_back(convert(c.witnesses[w], ix));
      p.enumeration = enumeration_of(c, fp.point, p.witnesses.size(), spec.solutions);
      r.points.push_back(std::move(p));
    }
    std::sort(r.points.begin(), r.points.end(), [](const FrontierPoint& a, const FrontierPoint& b) {
      return a.at.n != b.at.n ? a.at.n < b.at.n : a.at.e < b.at.e;
    });
    for (std::size_t qi = 0; qi < plan.requested.size(); ++qi) {
      RequestedPoint q;
      q.at = plan.requested[qi];
      q.answer = plan.requested_answer[qi]->outcome;
      q.label = std::string(unknown_label(q.answer));
      for (const std::uint32_t w : plan.requested_witnesses[qi]) q.witnesses.push_back(convert(c.witnesses[w], ix));
      r.requested_points.push_back(std::move(q));
    }
    for (const Point cell : c.frontier.undecided) r.undecided.push_back({cell, undecided_label(c, cell)});
    std::sort(r.undecided.begin(), r.undecided.end(), [](const UndecidedCell& a, const UndecidedCell& b) {
      return a.at.n != b.at.n ? a.at.n < b.at.n : a.at.e < b.at.e;
    });
    r.checks = count_checks(c);
    for (const solve::ObservationRow& row : c.observations) {
      Observation o;
      o.gene = row.gene;
      o.observed = observed_from(row.observed);
      o.status = status_from(row.role);
      if (in_corridor(o.status)) {
        if (auto node = catalogue.node_of(row.gene)) o.node = ix.node(*node);
      }
      r.observations.push_back(std::move(o));
    }
    std::sort(r.observations.begin(), r.observations.end(), [](const Observation& a, const Observation& b) {
      return core::stable_compare(a.gene, b.gene) < 0;
    });
    r.status_text = status_text(r);
    out.cases.push_back(std::move(r));
  }
  // Cases in, out, free (section 07, "Determinism").
  std::stable_sort(out.cases.begin(), out.cases.end(),
                   [](const CaseResult& a, const CaseResult& b) { return a.kind < b.kind; });
  return out;
}

// ---------------------------------------------------------------- build from the run's objects

namespace {

// The catalogue a real solve publishes through: 02's network, labels and pathway names, 04's values and
// 05's corridor distances and observation match.
class CorridorCatalogue final : public Catalogue {
 public:
  CorridorCatalogue(const PublishInputs& in, std::optional<double> frozen_threshold)
      : in_(in), net_(in.corridor.network()), frozen_(frozen_threshold) {
    // A network keeps an interner of its own, so the run's gene ids (observations, values, the match)
    // are turned into text only through the run's interner, never the network's.
    if (in.interner != nullptr) {
      for (const expression::ObservationMatch& m : in.match) {
        if (m.group == expression::MatchGroup::in_corridor && m.node) {
          genes_.emplace(std::string(in.interner->str(m.id)), *m.node);
        }
      }
    }
    for (const solve::CaseResult& c : in.results.cases) {
      for (const solve::ObservationRow& row : c.observations) observed_.emplace(row.gene, observed_from(row.observed));
      // The class the solve used, per encoded node and by key (S5.12, S5.14): up wins (S3.07), and the
      // out case's FSC, removed from its observations, keeps the class the in case gives it.
      if (!c.problem) continue;
      const solve::Problem& p = *c.problem;
      for (std::size_t i = 0; i < p.nodes.size() && i < p.regulation.size(); ++i) {
        const Observed o = p.regulation[i] == expression::Regulation::up     ? Observed::up
                           : p.regulation[i] == expression::Regulation::down ? Observed::down
                                                                             : Observed::none;
        auto [it, added] = solved_.emplace(p.nodes[i].value, o);
        if (!added && (o == Observed::up || it->second == Observed::none)) it->second = o;
      }
    }
  }

  std::optional<NodeRecord> node(const network::NodeKey& key) const override {
    const auto idx = net_.find_node(key);
    if (!idx) return std::nullopt;
    NodeRecord r;
    r.key = key;
    r.label = net_.label(*idx, in_.maps);       // 07.C10: "RRAS2+5", "[STAT1+6]"
    r.kind = net_.kind(*idx);
    std::vector<core::GeneId> ids(net_.members(*idx).begin(), net_.members(*idx).end());
    if (ids.empty()) {
      for (const network::NodeIdx part : net_.components(*idx)) {
        ids.insert(ids.end(), net_.members(part).begin(), net_.members(part).end());
      }
    }
    std::set<std::string> seen;
    bool up = false, down = false;
    for (const core::GeneId id : ids) {
      Member m;
      m.id = std::string(net_.interner().str(id));   // the network's id, in the network's interner
      if (!seen.insert(m.id).second) continue;
      m.symbol = std::string(in_.maps.symbols.label_or_id(m.id));
      // The run's values and observations are keyed in the run's interner: matched by text (4A.13).
      const std::optional<core::GeneId> run_id =
          in_.interner != nullptr ? in_.interner->find_id(m.id) : std::optional<core::GeneId>();
      if (in_.values != nullptr && run_id) {
        if (const auto i = in_.values->find(*run_id)) m.lfc = in_.values->value(*i);
      }
      m.observed = observed_of(m.id, run_id);
      up = up || m.observed == Observed::up;
      down = down || m.observed == Observed::down;
      r.members.push_back(std::move(m));
    }
    // The class the solve used for an encoded node; otherwise up when a member is (S3.07: up wins),
    // else down when a member is.
    if (const auto it = solved_.find(key.value); it != solved_.end()) {
      r.observed = it->second;
      if (r.members.size() == 1) r.members.front().observed = it->second;
    } else {
      r.observed = up ? Observed::up : down ? Observed::down : Observed::none;
    }
    if (in_.corridor.view().has_node(*idx)) {
      r.from_source = static_cast<std::int32_t>(in_.corridor.ds(*idx));   // S4.07
      r.to_target = static_cast<std::int32_t>(in_.corridor.dt(*idx));
    }
    return r;
  }

  std::optional<EdgeRecord> edge(const network::EdgeKey& key) const override {
    const auto idx = net_.find_edge(key);
    if (!idx) return std::nullopt;
    EdgeRecord r;
    r.key = key;
    r.type = key.type;
    r.subtypes = key.subtypes;
    r.effect = net_.effect(*idx);               // the one polarity rule (S6.14)
    for (const core::Sym p : net_.pathways(*idx)) r.pathways.emplace_back(net_.interner().str(p));
    const network::Edge& e = net_.edges()[*idx];
    // S5.16: an edge scoring strictly above the frozen threshold cannot be relaxed.
    r.frozen = frozen_ && (e.flags & network::bit(network::EdgeFlag::unscored)) == 0 && e.score > *frozen_;
    return r;
  }

  std::optional<network::NodeKey> node_of(std::string_view gene) const override {
    const auto it = genes_.find(std::string(gene));
    if (it == genes_.end()) return std::nullopt;
    return it->second;
  }

  std::string pathway_name(std::string_view pathway) const override {
    return net_.pathway_label(pathway, in_.maps);   // KGML title, map, else the id (entry 205)
  }

 private:
  Observed observed_of(const std::string& gene, std::optional<core::GeneId> run_id) const {
    // A gene's class: the run's observations, through the run's interner; else 06's rows by text.
    if (run_id && !in_.inputs.observations.empty()) {
      switch (in_.inputs.observations.regulation(*run_id)) {
        case expression::Regulation::up: return Observed::up;
        case expression::Regulation::down: return Observed::down;
        case expression::Regulation::none: return Observed::none;
      }
    }
    if (const auto it = observed_.find(gene); it != observed_.end()) return it->second;
    return Observed::none;
  }

  const PublishInputs& in_;
  const network::Network& net_;
  std::optional<double> frozen_;
  std::map<std::string, network::NodeKey> genes_;
  std::map<std::string, Observed> observed_;    // 06's rows, by gene text
  std::map<std::string, Observed> solved_;      // 06's Problem, by node key
};

std::optional<network::NodeKey> resolve_gene(const network::Network& net, std::string id) {
  // The id rule of 02.C3: colons removed; the first claim binds it (S2.03).
  std::erase(id, ':');
  const auto gid = net.interner().find_id(id);
  if (!gid) return std::nullopt;
  const auto holders = net.ids().holders(*gid);
  if (holders.empty()) return std::nullopt;
  return net.node_key(holders.front());
}

}  // namespace

SolveResult build(const PublishInputs& in) {
  const solve::Settings& s = in.settings;
  const network::Network& net = in.corridor.network();
  // The case as 06 encoded it, when a case got that far: its FSC nodes and the families it used.
  const solve::Problem* problem = nullptr;
  for (const solve::CaseResult& c : in.results.cases) {
    if (c.problem) {
      problem = c.problem.get();
      break;
    }
  }
  SolveSpec spec;
  spec.name = in.name.empty() ? std::string("solve") : in.name;
  spec.source = net.node_key(in.corridor.source());
  spec.target = net.node_key(in.corridor.target());
  if (problem != nullptr) {
    for (const std::string& key : problem->fsc) spec.fsc.push_back(network::NodeKey{key});
  } else {
    for (const std::string& id : s.fsc) {
      if (auto key = resolve_gene(net, id)) spec.fsc.push_back(std::move(*key));
    }
  }
  spec.bound = static_cast<std::int32_t>(in.corridor.bound());
  spec.seed = s.seed;
  // Only the constraint families the solve used (S5.03): as 06 decided them when it built the case;
  // otherwise by S5.03's rule, a given flag deciding and, with none, the inputs given turning each
  // family on. FuSION 1 also took a negative threshold as off (GraphManagerNew.cc:28603-28604,
  // 28713-28714).
  if (problem != nullptr) {
    if (problem->families.coexpression) spec.coexpression_threshold = problem->families.coexpression_threshold;
    if (problem->families.frozen) spec.frozen_threshold = problem->families.frozen_threshold;
  } else {
    const bool families_on = s.families.enabled.value_or(true);
    if (families_on && in.inputs.coexpression != nullptr && s.families.coexpression_threshold &&
        *s.families.coexpression_threshold >= 0) {
      spec.coexpression_threshold = s.families.coexpression_threshold;
    }
    if (families_on && s.families.frozen_threshold && *s.families.frozen_threshold >= 0) {
      spec.frozen_threshold = s.families.frozen_threshold;
    }
  }
  spec.rules = {std::string(rule_de_on_every_path), std::string(rule_absent_down),
                std::string(rule_endpoints_exempt)};
  if (spec.coexpression_threshold || spec.frozen_threshold) spec.rules.emplace_back(rule_strict_scores);
  spec.corridor.nodes = static_cast<std::uint32_t>(in.corridor.view().node_count());
  spec.corridor.edges = static_cast<std::uint32_t>(in.corridor.view().edge_count());
  spec.corridor.bound = static_cast<std::int32_t>(in.corridor.bound());
  spec.corridor.artefact = in.corridor_artefact;
  spec.requested = s.box.hi;
  spec.solutions = s.solutions;
  const CorridorCatalogue catalogue(in, spec.frozen_threshold);
  return build(in.results, spec, catalogue);
}

}  // namespace fusion::results
