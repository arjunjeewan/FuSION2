// results/src/analyse.cpp: the checks run on every SolveResult before anything is written (shape,
// polarity, counts, draw cases, FSC), the sign-consistent routes (S5.13) and the FSC comparison (07.C7)
// (section 07, "Checks and analysis").
#include "fusion/results/analyse.hpp"

#include <algorithm>
#include <array>
#include <climits>
#include <deque>
#include <map>
#include <set>
#include <utility>

#include "text.hpp"

namespace fusion::results {

namespace {

constexpr std::array<std::string_view, 7> verdict_names{
    "identical", "with_cheaper", "with_costlier", "mixed", "essential", "with_impossible", "no_points"};

std::string node_name(const SolveResult& r, NodeIx i) {
  if (i >= r.tables.nodes.size()) return "node #" + std::to_string(i);
  const NodeRecord& n = r.tables.nodes[i];
  return n.label == n.key.value ? n.key.value : n.label + " (" + n.key.value + ")";
}

std::string edge_name(const SolveResult& r, EdgeIx i) {
  if (i >= r.tables.edges.size()) return "edge #" + std::to_string(i);
  return r.tables.edges[i].key.text();
}

std::string_view state_name(network::EndState s) {
  switch (s) {
    case network::EndState::absent: return "absent";
    case network::EndState::inactive: return "inactive";
    case network::EndState::active: return "active";
  }
  return "?";
}

template <class T>
bool strictly_increasing(const std::vector<T>& v) {
  return std::adjacent_find(v.begin(), v.end(), [](const T& a, const T& b) { return !(a < b); }) == v.end();
}

template <class T>
bool contains(const std::vector<T>& sorted, T value) {
  return std::binary_search(sorted.begin(), sorted.end(), value);
}

class Checker {
 public:
  Checker(const SolveResult& r, std::vector<Issue>& issues) : r_(r), issues_(issues) {}

  void error(std::string code, std::string message) {
    issues_.push_back({Issue::Kind::internal_error, std::move(code), "internal error: " + std::move(message)});
  }

  // One witness of one point: shape, polarity and draw cases; counts and the FSC are the caller's.
  bool witness(const CaseResult& c, Point at, const Witness& w) {
    const std::string where = " (" + c.label + ", " + point_text(at) + ")";
    const auto n_nodes = r_.tables.nodes.size();
    const auto n_edges = r_.tables.edges.size();
    // Shape: sorted unique lists, valid indices, disjoint activity, present ends (section 07, checks).
    const std::array<const std::vector<NodeIx>*, 4> node_lists{&w.active, &w.inactive, &w.relaxed_present,
                                                                &w.relaxed_absent};
    const std::array<const std::vector<EdgeIx>*, 3> edge_lists{&w.selected, &w.relaxed_selected,
                                                                &w.relaxed_unselected};
    for (const auto* list : node_lists) {
      if (!strictly_increasing(*list) || (!list->empty() && list->back() >= n_nodes)) {
        error("results.shape", "a node list of the witness is not sorted, unique and in the table" + where);
        return false;
      }
    }
    for (const auto* list : edge_lists) {
      if (!strictly_increasing(*list) || (!list->empty() && list->back() >= n_edges)) {
        error("results.shape", "an edge list of the witness is not sorted, unique and in the table" + where);
        return false;
      }
    }
    bool ok = true;
    for (const NodeIx v : w.active) {
      if (contains(w.inactive, v)) {
        error("results.shape", node_name(r_, v) + " is both active and inactive" + where);
        ok = false;
      }
    }
    const auto present = [&](NodeIx v) { return contains(w.active, v) || contains(w.inactive, v); };
    for (const NodeIx v : w.relaxed_present) {
      if (!present(v)) {
        error("results.shape", node_name(r_, v) + " is relaxed and present but has no activity" + where);
        ok = false;
      }
    }
    for (const NodeIx v : w.relaxed_absent) {
      if (present(v)) {
        error("results.shape", node_name(r_, v) + " is relaxed as absent but is present" + where);
        ok = false;
      }
    }
    for (const EdgeIx e : w.relaxed_selected) {
      if (!contains(w.selected, e)) {
        error("results.shape", "edge " + edge_name(r_, e) + " is relaxed as selected but not selected" + where);
        ok = false;
      }
    }
    for (const EdgeIx e : w.relaxed_unselected) {
      if (contains(w.selected, e)) {
        error("results.shape", "edge " + edge_name(r_, e) + " is relaxed as unselected but selected" + where);
        ok = false;
      }
    }
    const auto check_ends = [&](EdgeIx e) {
      const EdgeRecord& rec = r_.tables.edges[e];
      if (rec.source == rec.target) {
        // S5.11 forbids selecting a self-loop; S5.13 leaves self-loops out of the count.
        error("results.shape", "edge " + edge_name(r_, e) + " is a self-loop in the witness" + where);
        ok = false;
      } else if (!present(rec.source) || !present(rec.target)) {
        error("results.shape", "edge " + edge_name(r_, e) + " has an end that is not present" + where);
        ok = false;
      }
    };
    for (const EdgeIx e : w.selected) check_ends(e);
    for (const EdgeIx e : w.relaxed_unselected) check_ends(e);
    if (!ok) return false;

    // Polarity (S6.14): the one rule, network::is_relaxed, must give the witness's own flag.
    const auto state = [&](NodeIx v) {
      if (contains(w.active, v)) return network::EndState::active;
      if (contains(w.inactive, v)) return network::EndState::inactive;
      return network::EndState::absent;
    };
    const auto polarity = [&](EdgeIx e) {
      const EdgeRecord& rec = r_.tables.edges[e];
      const bool flagged = contains(w.relaxed_selected, e) || contains(w.relaxed_unselected, e);
      const auto s = state(rec.source);
      const auto t = state(rec.target);
      const bool rule = network::is_relaxed(rec.effect, s, t);
      if (rule != flagged) {
        error("results.polarity",
              "edge " + rec.key.text() + " is " + (flagged ? "relaxed" : "kept") + " in the witness, but its ends (" +
                  std::string(state_name(s)) + ", " + std::string(state_name(t)) + ") " +
                  (flagged ? "honour it" : "break it") + where);
        ok = false;
      }
    };
    for (const EdgeIx e : w.selected) polarity(e);
    for (const EdgeIx e : w.relaxed_unselected) polarity(e);

    // Draw cases (S6.15): no pair has one effect both kept and relaxed, the twin of case 4.
    std::map<std::pair<NodeIx, NodeIx>, std::pair<std::set<network::Effect>, std::set<network::Effect>>> pairs;
    for (const EdgeIx e : w.selected) {
      const EdgeRecord& rec = r_.tables.edges[e];
      auto& slot = pairs[{rec.source, rec.target}];
      (contains(w.relaxed_selected, e) ? slot.second : slot.first).insert(rec.effect);
    }
    for (const EdgeIx e : w.relaxed_unselected) {
      const EdgeRecord& rec = r_.tables.edges[e];
      pairs[{rec.source, rec.target}].second.insert(rec.effect);
    }
    for (const auto& [pair, effects] : pairs) {
      for (const network::Effect eff : effects.first) {
        if (effects.second.contains(eff)) {
          error("results.twin", "the pair " + node_name(r_, pair.first) + " -> " + node_name(r_, pair.second) +
                                    " has an " + std::string(effect_name(eff)) +
                                    " both kept and relaxed (S6.15's twin)" + where);
          ok = false;
        }
      }
    }
    return ok;
  }

  // S5.17 as 06.C10 extends it: in the in case every FSC node is present, on a selected path, with its
  // activity matching its observation; in the out case every FSC node is absent.
  void fsc(const CaseResult& c, Point at, const Witness& w) {
    if (c.kind == Case::free) return;
    const std::string where = " (" + c.label + ", " + point_text(at) + ")";
    for (const NodeIx f : r_.header.fsc) {
      if (f >= r_.tables.nodes.size()) continue;
      const bool active = contains(w.active, f);
      const bool present = active || contains(w.inactive, f);
      if (c.kind == Case::out) {
        if (present) error("results.fsc", "FSC " + node_name(r_, f) + " is present in the out case" + where);
        continue;
      }
      if (!present) {
        error("results.fsc", "FSC " + node_name(r_, f) + " is absent in the in case" + where);
        continue;
      }
      bool in_edge = f == r_.header.source, out_edge = f == r_.header.target;
      for (const EdgeIx e : w.selected) {
        in_edge = in_edge || r_.tables.edges[e].target == f;
        out_edge = out_edge || r_.tables.edges[e].source == f;
      }
      if (!in_edge || !out_edge) {
        error("results.fsc", "FSC " + node_name(r_, f) + " is on no selected path in the in case" + where);
      }
      const bool want_active = r_.tables.nodes[f].observed != Observed::down;
      if (active != want_active) {
        error("results.fsc", "FSC " + node_name(r_, f) + " is " + (active ? "active" : "inactive") +
                                 " but is observed " + std::string(observed_name(r_.tables.nodes[f].observed)) +
                                 " in the in case" + where);
      }
    }
  }

 private:
  const SolveResult& r_;
  std::vector<Issue>& issues_;
};

std::vector<std::vector<NodeIx>> kept_adjacency(const SolveResult& r, const Witness& w, bool reverse) {
  // Selected edges whose effect is honoured (S5.13), neighbours in key order: node indices follow keys.
  std::vector<std::vector<NodeIx>> adj(r.tables.nodes.size());
  for (const EdgeIx e : w.selected) {
    if (std::binary_search(w.relaxed_selected.begin(), w.relaxed_selected.end(), e)) continue;
    const EdgeRecord& rec = r.tables.edges[e];
    if (reverse) adj[rec.target].push_back(rec.source);
    else adj[rec.source].push_back(rec.target);
  }
  for (auto& list : adj) {
    std::sort(list.begin(), list.end());
    list.erase(std::unique(list.begin(), list.end()), list.end());
  }
  return adj;
}

std::vector<bool> reach(const std::vector<std::vector<NodeIx>>& adj, NodeIx from) {
  std::vector<bool> seen(adj.size(), false);
  if (from >= adj.size()) return seen;
  std::deque<NodeIx> queue{from};
  seen[from] = true;
  while (!queue.empty()) {
    const NodeIx u = queue.front();
    queue.pop_front();
    for (const NodeIx v : adj[u]) {
      if (!seen[v]) {
        seen[v] = true;
        queue.push_back(v);
      }
    }
  }
  return seen;
}

// The least n among a case's points with e' <= e, per e in [0, top]; INT_MAX for none (07.C7).
std::vector<int> least_n(const CaseResult& c, int top) {
  std::vector<int> f(static_cast<std::size_t>(top + 1), INT_MAX);
  const auto add = [&](Point p) {
    for (int e = std::max(p.e, 0); e <= top; ++e) {
      auto& slot = f[static_cast<std::size_t>(e)];
      slot = std::min(slot, p.n);
    }
  };
  for (const FrontierPoint& p : c.points) add(p.at);
  for (const RequestedPoint& q : c.requested_points) {
    if (q.answer == solve::Outcome::sat) add(q.at);
  }
  return f;
}

bool has_points(const CaseResult& c) {
  if (!c.points.empty()) return true;
  return std::any_of(c.requested_points.begin(), c.requested_points.end(),
                     [](const RequestedPoint& q) { return q.answer == solve::Outcome::sat; });
}

bool settled(const CaseResult& c) {
  // 07.C7: complete, or no_point with no undecided cell, and every point certified.
  const bool status_ok = c.status == CaseStatus::complete ||
                         (c.status == CaseStatus::no_point && c.undecided.empty());
  return status_ok && std::all_of(c.points.begin(), c.points.end(),
                                  [](const FrontierPoint& p) { return p.certified; });
}

const CaseResult* find_case(const SolveResult& r, Case kind) {
  for (const CaseResult& c : r.cases) {
    if (c.kind == kind) return &c;
  }
  return nullptr;
}

}  // namespace

std::string_view verdict_name(Verdict v) noexcept {
  const auto i = static_cast<std::size_t>(v);
  return i < verdict_names.size() ? verdict_names[i] : std::string_view{};
}

std::optional<Verdict> parse_verdict(std::string_view word) noexcept {
  for (std::size_t i = 0; i < verdict_names.size(); ++i) {
    if (verdict_names[i] == word) return static_cast<Verdict>(i);
  }
  return std::nullopt;
}

std::string_view issue_kind_name(Issue::Kind k) noexcept {
  return k == Issue::Kind::internal_error ? "internal_error" : "warning";
}

Route sign_consistent_route(const SolveResult& r, Case kind, const Witness& w) {
  // S5.13: a breadth-first search from the source over the selected edges that are not relaxed, with
  // neighbours in key order, gives the first shortest route to the target.
  Route route;
  const NodeIx s = r.header.source, t = r.header.target;
  const auto present = [&](NodeIx v) {
    return std::binary_search(w.active.begin(), w.active.end(), v) ||
           std::binary_search(w.inactive.begin(), w.inactive.end(), v);
  };
  if (s >= r.tables.nodes.size() || t >= r.tables.nodes.size() || !present(s)) return route;
  const auto forward = kept_adjacency(r, w, false);
  std::vector<std::optional<NodeIx>> parent(forward.size());
  std::vector<bool> seen(forward.size(), false);
  std::deque<NodeIx> queue{s};
  seen[s] = true;
  while (!queue.empty() && !seen[t]) {
    const NodeIx u = queue.front();
    queue.pop_front();
    for (const NodeIx v : forward[u]) {
      if (seen[v]) continue;
      seen[v] = true;
      parent[v] = u;
      queue.push_back(v);
    }
  }
  if (seen[t]) {
    route.exists = true;
    for (std::optional<NodeIx> v = t; v; v = parent[*v]) {
      route.nodes.push_back(*v);
      if (*v == s) break;
    }
    std::reverse(route.nodes.begin(), route.nodes.end());
  }
  // An FSC node is on a route when the source reaches it and it reaches the target over those edges;
  // in the in case only.
  if (kind == Case::in && !r.header.fsc.empty()) {
    const auto from_source = reach(forward, s);
    const auto to_target = reach(kept_adjacency(r, w, true), t);
    for (const NodeIx f : r.header.fsc) {
      if (f < from_source.size() && from_source[f] && to_target[f]) route.fsc_on_route.push_back(f);
    }
  }
  return route;
}

std::vector<Issue> analyse(SolveResult& r) {
  std::vector<Issue> issues;
  Checker check(r, issues);
  const auto n_nodes = r.tables.nodes.size();
  if (r.header.source >= n_nodes || r.header.target >= n_nodes ||
      std::any_of(r.header.fsc.begin(), r.header.fsc.end(), [&](NodeIx f) { return f >= n_nodes; })) {
    check.error("results.shape", "the source, the target or an FSC node is not in the node table");
    return issues;
  }
  for (CaseResult& c : r.cases) {
    for (FrontierPoint& p : c.points) {
      if (p.witnesses.empty()) {
        check.error("results.counts", point_text(p.at) + " of " + c.label + " has no witness (06.C20)");
        continue;
      }
      for (std::size_t i = 0; i < p.witnesses.size(); ++i) {
        Witness& w = p.witnesses[i];
        if (!check.witness(c, p.at, w)) continue;
        check.fsc(c, p.at, w);
        // Counts (06.C20): the point equals its first witness's counts; further witnesses, found under
        // the point's bounds, are no costlier.
        const Point counts = w.counts();
        if (i == 0 && !(counts == p.at)) {
          check.error("results.counts", point_text(p.at) + " of " + c.label + " has a witness with counts " +
                                            point_text(counts) + " (06.C20)");
        } else if (counts.n > p.at.n || counts.e > p.at.e) {
          check.error("results.counts", "witness " + std::to_string(i + 1) + " at " + point_text(p.at) + " of " +
                                            c.label + " has counts " + point_text(counts) + " above the point");
        }
        w.route = sign_consistent_route(r, c.kind, w);
      }
    }
    for (RequestedPoint& q : c.requested_points) {
      for (Witness& w : q.witnesses) {
        if (!check.witness(c, q.at, w)) continue;
        check.fsc(c, q.at, w);
        const Point counts = w.counts();
        if (counts.n > q.at.n || counts.e > q.at.e) {
          check.error("results.counts", "a witness at the requested " + point_text(q.at) + " of " + c.label +
                                            " has counts " + point_text(counts) + " above the point");
        }
        w.route = sign_consistent_route(r, c.kind, w);
      }
    }
    if (c.kind == Case::out) {
      // An observed FSC gene is removed from the out case's observations (S5.17).
      for (const Observation& o : c.observations) {
        if (o.node && std::binary_search(r.header.fsc.begin(), r.header.fsc.end(), *o.node) &&
            o.status != ObservationStatus::removed_with_fsc) {
          check.error("results.fsc", "FSC gene " + o.gene + " has the status " +
                                         std::string(observation_status_name(o.status)) +
                                         " in the out case, not removed_with_fsc");
        }
      }
    }
  }
  return issues;
}

std::optional<Comparison> compare_cases(const SolveResult& r) {
  const CaseResult* in = find_case(r, Case::in);
  const CaseResult* out = find_case(r, Case::out);
  if (in == nullptr || out == nullptr || in->status == CaseStatus::not_requested ||
      out->status == CaseStatus::not_requested) {
    return std::nullopt;
  }
  Comparison cmp;
  const bool in_has = has_points(*in), out_has = has_points(*out);
  int top = std::max({in->requested.e, out->requested.e, 0});
  for (const CaseResult* c : {in, out}) {
    for (const FrontierPoint& p : c->points) top = std::max(top, p.at.e);
    for (const RequestedPoint& q : c->requested_points) top = std::max(top, q.at.e);
  }
  const auto f_in = least_n(*in, top), f_out = least_n(*out, top);
  if (in_has && !out_has) {
    cmp.verdict = Verdict::essential;
  } else if (!in_has && out_has) {
    cmp.verdict = Verdict::with_impossible;
  } else if (!in_has) {
    cmp.verdict = Verdict::no_points;
  } else {
    bool cheaper = false, costlier = false;
    for (std::size_t e = 0; e < f_in.size(); ++e) {
      cheaper = cheaper || f_in[e] < f_out[e];
      costlier = costlier || f_in[e] > f_out[e];
    }
    cmp.verdict = cheaper && costlier ? Verdict::mixed
                  : cheaper           ? Verdict::with_cheaper
                  : costlier          ? Verdict::with_costlier
                                      : Verdict::identical;
  }
  if (f_in[0] != INT_MAX && f_out[0] != INT_MAX) cmp.delta_n_at_e0 = f_out[0] - f_in[0];
  cmp.certified = settled(*in) && settled(*out);
  return cmp;
}

std::string comparison_sentence(const SolveResult& r, const Comparison& cmp) {
  std::vector<std::string> labels;
  for (const NodeIx f : r.header.fsc) {
    if (f < r.tables.nodes.size()) labels.push_back(r.tables.nodes[f].label);
  }
  const std::string fsc = labels.empty() ? std::string("the FSC") : detail::join(labels, ", ");
  const auto nodes = [](int k) { return std::to_string(k) + (k == 1 ? " node" : " nodes"); };
  std::string text;
  const int delta = cmp.delta_n_at_e0.value_or(0);
  switch (cmp.verdict) {
    case Verdict::identical: text = "forcing " + fsc + " in changes nothing: both frontiers are identical"; break;
    case Verdict::with_cheaper:
      text = delta > 0 ? "forcing " + fsc + " in saves " + nodes(delta) + " at e=0"
                       : "forcing " + fsc + " in lowers the frontier";
      break;
    case Verdict::with_costlier:
      text = delta < 0 ? "forcing " + fsc + " in costs " + nodes(-delta) + " more at e=0"
                       : "forcing " + fsc + " in raises the frontier";
      break;
    case Verdict::mixed:
      text = "forcing " + fsc + " in is cheaper at some edge counts and costlier at others";
      break;
    case Verdict::essential: text = fsc + " is essential: there is no point without it"; break;
    case Verdict::with_impossible:
      text = "forcing " + fsc + " in leaves no point, while there are points without it";
      break;
    case Verdict::no_points: text = "neither case has a point"; break;
  }
  if (!cmp.certified) text += " (not certified)";
  return text;
}

}  // namespace fusion::results
