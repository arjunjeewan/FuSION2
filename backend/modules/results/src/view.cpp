// results/src/view.cpp: the one explanation renderer's view model (S6.26), the point selectors (S6.03)
// and psp's path filter (S6.04), ported from FuSION 1's two renderers: pex's grouping, draw cases,
// levels and parked table (plot_explanation.py:197-393) and psp's distance filter
// (GraphManagerNew.cc:8982-9071), with S6.05, S6.06, S6.13 and S6.15's fixes.
#include "fusion/results/view.hpp"

#include <algorithm>
#include <charconv>
#include <deque>
#include <limits>
#include <map>
#include <set>
#include <utility>

#include "fusion/results/analyse.hpp"
#include "text.hpp"

namespace fusion::results {

namespace {

constexpr std::int32_t unreached = std::numeric_limits<std::int32_t>::max();

std::optional<int> whole_number(std::string_view text) {
  // Digits only: no sign, no space, no trailing text (S6.03: "7,1x" is an error, as in pex).
  if (text.empty() || text.size() > 9) return std::nullopt;
  int value = 0;
  for (const char c : text) {
    if (c < '0' || c > '9') return std::nullopt;
    value = value * 10 + (c - '0');
  }
  return value;
}

template <class T>
bool contains(const std::vector<T>& sorted, T value) {
  return std::binary_search(sorted.begin(), sorted.end(), value);
}

bool by_point(Point a, Point b) { return a.n != b.n ? a.n < b.n : a.e < b.e; }

std::optional<NodeIx> find_node(const SolveResult& r, const network::NodeKey& key) {
  const auto& nodes = r.tables.nodes;
  const auto it = std::lower_bound(nodes.begin(), nodes.end(), key,
                                   [](const NodeRecord& n, const network::NodeKey& k) { return n.key < k; });
  if (it == nodes.end() || !(it->key == key)) return std::nullopt;
  return static_cast<NodeIx>(it - nodes.begin());
}

// Breadth-first hop distances over pairs, from `from` (forward) or to `from` (backward).
std::vector<std::int32_t> hops(std::size_t n, const std::vector<std::pair<NodeIx, NodeIx>>& pairs, NodeIx from,
                               bool backward) {
  std::vector<std::vector<NodeIx>> adj(n);
  for (const auto& [u, v] : pairs) {
    if (backward) adj[v].push_back(u);
    else adj[u].push_back(v);
  }
  for (auto& list : adj) {
    std::sort(list.begin(), list.end());   // neighbours in key order (node indices follow keys)
    list.erase(std::unique(list.begin(), list.end()), list.end());
  }
  std::vector<std::int32_t> dist(n, unreached);
  if (from >= n) return dist;
  std::deque<NodeIx> queue{from};
  dist[from] = 0;
  while (!queue.empty()) {
    const NodeIx u = queue.front();
    queue.pop_front();
    for (const NodeIx v : adj[u]) {
      if (dist[v] == unreached) {
        dist[v] = dist[u] + 1;
        queue.push_back(v);
      }
    }
  }
  return dist;
}

std::string label(const SolveResult& r, NodeIx i) { return r.tables.nodes[i].label; }

std::string title_of(const SolveResult& r, const CaseResult& c, Point at) {
  // S6.17: from the run's real case and from labels, not "with FSC" on every page and rep ids.
  return label(r, r.header.source) + " → " + label(r, r.header.target) + " · " + c.label + " · n " +
         std::to_string(at.n) + ", e " + std::to_string(at.e);
}

std::string subtitle_of(Enumeration e, std::size_t witnesses) {
  // S6.17: "of several" only when it is known. FuSION 1 hard-coded it (plot_explanation.py:541).
  const std::string k = std::to_string(witnesses);
  switch (e) {
    case Enumeration::not_requested:
      return "One explanation found at this point; the solve did not look for others.";
    case Enumeration::complete:
      return witnesses <= 1 ? "The only explanation at this point: the search for others found none."
                            : "Explanation 1 of " + k + "; there are exactly " + k + ".";
    case Enumeration::limit_reached:
      return "Explanation 1 of " + k + " shown; the solve stopped at " + k + ", so there may be more.";
    case Enumeration::stopped_unknown:
      return "Explanation 1 of " + k + " shown; the search for more ended without an answer.";
  }
  return {};
}

std::string certification_of(const CaseResult& c, const FrontierPoint& p) {
  // 06.C6: certified, or the undecided cells that block the point (S5.19).
  if (p.certified) return "Certified Pareto-optimal.";
  if (p.undecided_below.empty()) return "Not certified.";
  std::vector<std::string> cells;
  for (const Point b : p.undecided_below) {
    std::string text = point_text(b);
    for (const UndecidedCell& u : c.undecided) {
      if (u.at == b) text += " (" + u.label + ")";
    }
    cells.push_back(std::move(text));
  }
  std::string list;
  for (std::size_t i = 0; i < cells.size(); ++i) {
    if (i > 0) list += i + 1 == cells.size() ? " and " : ", ";
    list += cells[i];
  }
  return std::string("Not certified: blocked by the undecided ") + (cells.size() == 1 ? "cell " : "cells ") + list +
         ".";
}

std::string route_note_of(const SolveResult& r, const CaseResult& c, const Route& route) {
  // S5.13: sign-consistent routes are reported apart from presence.
  if (!route.exists) {
    return "No sign-consistent route from " + label(r, r.header.source) + " to " + label(r, r.header.target) +
           " in this explanation.";
  }
  std::string text = "Sign-consistent route: ";
  for (std::size_t i = 0; i < route.nodes.size(); ++i) {
    if (i > 0) text += " → ";
    text += label(r, route.nodes[i]);
  }
  const std::size_t steps = route.nodes.empty() ? 0 : route.nodes.size() - 1;
  text += " (" + std::to_string(steps) + (steps == 1 ? " step)" : " steps)");
  if (c.kind == Case::in) {
    for (const NodeIx f : r.header.fsc) {
      text += "; " + label(r, f);
      if (std::find(route.nodes.begin(), route.nodes.end(), f) != route.nodes.end()) text += " is on it";
      else if (contains(route.fsc_on_route, f)) text += " is on another sign-consistent route";
      else text += " is on no sign-consistent route";
    }
  }
  return text + ".";
}

std::string parked_reason(Observed observed) {
  // S6.13: the table holds the relaxed nodes outside the witness and says why each is there.
  switch (observed) {
    case Observed::up: return "absent from this explanation; up-regulated, so it counts as one node relaxation";
    case Observed::down: return "absent from this explanation; down-regulated, counted as one node relaxation";
    case Observed::none: break;
  }
  return "absent from this explanation; counted as one node relaxation";
}

}  // namespace

// ---------------------------------------------------------------- selectors and filters

PointSelector parse_point_selector(std::string_view text) {
  // S6.03, strict: FuSION 1's psp read "%d,%d" with sscanf and so took "7,1x" as 7,1
  // (GraphManagerNew.cc:8935-8973); pex rejected it (plot_explanation.py:519-523), as 2.0 does.
  PointSelector s;
  if (text == "all") return s;
  if (text == "least") {
    s.kind = PointSelector::Kind::least;
    return s;
  }
  const auto comma = text.find(',');
  if (comma != std::string_view::npos) {
    const auto n = whole_number(text.substr(0, comma));
    const auto e = whole_number(text.substr(comma + 1));
    if (n && e) {
      s.kind = PointSelector::Kind::one;
      s.at.n = *n;
      s.at.e = *e;
      return s;
    }
  }
  throw core::Failure("batch.bad_value", "points '" + std::string(text) + "' is not all, least or <n>,<e>");
}

PathFilter parse_path_filter(std::string_view text) {
  // S6.01, S6.04: FuSION 1 parsed alt's window with atoi, so "alt:x" gave 0 and "alt:-1" passed
  // (GraphManagerNew.cc:8828-8850); 2.0 rejects both.
  PathFilter f;
  if (text == "subgraph") return f;
  if (text == "all") {
    f.mode = PathMode::all;
    return f;
  }
  if (text == "shortest") {
    f.mode = PathMode::shortest;
    return f;
  }
  if (text == "alt") {
    f.mode = PathMode::alt;
    return f;
  }
  if (text.starts_with("alt:")) {
    const auto w = whole_number(text.substr(4));
    if (!w) {
      throw core::Failure("batch.bad_value", "path mode '" + std::string(text) +
                                                 "': the window must be a whole number of at least 0");
    }
    f.mode = PathMode::alt;
    f.window = *w;
    return f;
  }
  throw core::Failure("batch.bad_value",
                      "path mode '" + std::string(text) + "' is not subgraph, all, shortest, alt or alt:W");
}

std::string_view path_mode_name(PathMode m) noexcept {
  switch (m) {
    case PathMode::subgraph: return "subgraph";
    case PathMode::all: return "all";
    case PathMode::shortest: return "shortest";
    case PathMode::alt: return "alt";
  }
  return {};
}

std::string path_filter_text(const PathFilter& f) {
  std::string text(path_mode_name(f.mode));
  if (f.mode == PathMode::alt) text += ":" + std::to_string(f.window);
  return text;
}

std::string_view draw_name(Draw d) noexcept {
  switch (d) {
    case Draw::honoured: return "honoured";
    case Draw::removed: return "removed";
    case Draw::flipped: return "flipped";
  }
  return {};
}

bool selects_requested(const CaseResult& c) noexcept { return c.points.empty() && !c.requested_points.empty(); }

std::vector<std::size_t> select_points(const CaseResult& c, const PointSelector& s) {
  // FuSION 1: all in ascending (n, e); least by (n + e, n) (plot_explanation.py:513-528). A requested
  // point (06.C9) counts for least only when it is sat.
  struct Item { std::size_t index; Point at; bool sat; };
  std::vector<Item> items;
  if (selects_requested(c)) {
    for (std::size_t i = 0; i < c.requested_points.size(); ++i) {
      items.push_back({i, c.requested_points[i].at, c.requested_points[i].answer == solve::Outcome::sat});
    }
  } else {
    for (std::size_t i = 0; i < c.points.size(); ++i) items.push_back({i, c.points[i].at, true});
  }
  std::stable_sort(items.begin(), items.end(), [](const Item& a, const Item& b) { return by_point(a.at, b.at); });
  std::vector<std::size_t> out;
  switch (s.kind) {
    case PointSelector::Kind::all:
      for (const Item& it : items) out.push_back(it.index);
      break;
    case PointSelector::Kind::least: {
      const Item* best = nullptr;
      for (const Item& it : items) {
        if (!it.sat) continue;
        const auto key = [](Point p) { return std::pair{p.n + p.e, p.n}; };
        if (best == nullptr || key(it.at) < key(best->at)) best = &it;
      }
      if (best != nullptr) out.push_back(best->index);
      break;
    }
    case PointSelector::Kind::one:
      for (const Item& it : items) {
        if (it.at == s.at) out.push_back(it.index);
      }
      break;
  }
  return out;
}

// ---------------------------------------------------------------- the view model

WitnessView build_witness_view(const SolveResult& r, const CaseResult& c, const Witness& w) {
  WitnessView view;
  view.counts = w.counts();
  const auto n_nodes = r.tables.nodes.size();

  // 1. Nodes: every present node, with its activity, relaxation and roles (S6.06). FuSION 1 drew only
  // active nodes and the ends of relaxed edges (plot_explanation.py:208-252).
  std::vector<NodeIx> present = w.active;
  present.insert(present.end(), w.inactive.begin(), w.inactive.end());
  std::sort(present.begin(), present.end());

  // 2. Edges, grouped by ordered node pair into kept (selected, not relaxed) and relaxed records, matched
  // by edge key, never by (source, target, subtype) as pex did (plot_explanation.py:215-234, S6.15).
  struct Group { std::vector<EdgeIx> kept, relaxed; };
  std::map<std::pair<NodeIx, NodeIx>, Group> groups;
  for (const EdgeIx e : w.selected) {
    const EdgeRecord& rec = r.tables.edges[e];
    auto& g = groups[{rec.source, rec.target}];
    (contains(w.relaxed_selected, e) ? g.relaxed : g.kept).push_back(e);
  }
  for (const EdgeIx e : w.relaxed_unselected) {
    const EdgeRecord& rec = r.tables.edges[e];
    groups[{rec.source, rec.target}].relaxed.push_back(e);
  }
  const auto effects = [&](const std::vector<EdgeIx>& list) {
    std::set<network::Effect> out;
    for (const EdgeIx e : list) out.insert(r.tables.edges[e].effect);
    return out;
  };
  const auto with_effect = [&](const std::vector<EdgeIx>& list, network::Effect eff) {
    std::vector<EdgeIx> out;
    for (const EdgeIx e : list) {
      if (r.tables.edges[e].effect == eff) out.push_back(e);
    }
    return out;
  };
  const auto add = [&](NodeIx u, NodeIx v, network::Effect eff, std::optional<network::Effect> replaced, Draw draw,
                       std::vector<EdgeIx> members) {
    ViewEdge edge{u, v, eff, replaced, draw, false, false, false, std::nullopt, std::move(members)};
    std::sort(edge.members.begin(), edge.members.end());
    for (const EdgeIx e : edge.members) {
      edge.in_witness = edge.in_witness || contains(w.selected, e);
      edge.ecrel = edge.ecrel || r.tables.edges[e].type == "ECrel";   // S2.31
    }
    view.edges.push_back(std::move(edge));
  };
  for (const auto& [pair, g] : groups) {
    const auto [u, v] = pair;
    const auto K = effects(g.kept);
    const auto R = effects(g.relaxed);
    if (R.empty()) {
      // Case 1: one honoured edge per effect.
      for (const network::Effect eff : K) add(u, v, eff, std::nullopt, Draw::honoured, with_effect(g.kept, eff));
    } else if (K.empty()) {
      // Case 2: one removed edge per effect.
      for (const network::Effect eff : R) add(u, v, eff, std::nullopt, Draw::removed, with_effect(g.relaxed, eff));
    } else if (K.size() == 1 && R.size() == 1 && *K.begin() != *R.begin()) {
      // Case 3: one flipped edge with the surviving effect's head, holding both records.
      std::vector<EdgeIx> members = g.kept;
      members.insert(members.end(), g.relaxed.begin(), g.relaxed.end());
      add(u, v, *K.begin(), *R.begin(), Draw::flipped, std::move(members));
    } else {
      // Case 4, the twin, cannot come from a consistent witness (S6.15); analyse() reports it as an
      // internal error. It is drawn as its kept and its removed edges, never dropped.
      for (const network::Effect eff : K) add(u, v, eff, std::nullopt, Draw::honoured, with_effect(g.kept, eff));
      for (const network::Effect eff : R) add(u, v, eff, std::nullopt, Draw::removed, with_effect(g.relaxed, eff));
    }
  }

  // 3. Levels (S6.13, kept): hop distance from the source over every drawn pair, selected or relaxed, in
  // the witness or not; a node no pair reaches goes one level past the deepest (plot_explanation.py:176-194).
  std::vector<std::pair<NodeIx, NodeIx>> all_pairs, selected_pairs;
  for (const auto& [pair, g] : groups) all_pairs.push_back(pair);
  for (const EdgeIx e : w.selected) selected_pairs.emplace_back(r.tables.edges[e].source, r.tables.edges[e].target);
  const bool source_present = contains(present, r.header.source);
  std::vector<std::int32_t> level(n_nodes, unreached);
  if (source_present) level = hops(n_nodes, all_pairs, r.header.source, false);
  std::int32_t deepest = 0;
  for (const NodeIx v : present) {
    if (level[v] != unreached) deepest = std::max(deepest, level[v]);
  }

  // 4. Slack (S6.04, psp's distance filter at GraphManagerNew.cc:8982-9071): through-length minus L over
  // the selected edges, for groups in the witness.
  const auto d_s = hops(n_nodes, selected_pairs, r.header.source, false);
  const auto d_t = hops(n_nodes, selected_pairs, r.header.target, true);
  if (source_present && d_t[r.header.source] != unreached) view.shortest = d_t[r.header.source];
  for (ViewEdge& e : view.edges) {
    if (!e.in_witness || !view.shortest || d_s[e.source] == unreached || d_t[e.target] == unreached) continue;
    e.slack = d_s[e.source] + 1 + d_t[e.target] - *view.shortest;
  }

  // 5. Routes (S5.13).
  const Route route = sign_consistent_route(r, c.kind, w);
  std::set<std::pair<NodeIx, NodeIx>> route_pairs;
  for (std::size_t i = 1; i < route.nodes.size(); ++i) route_pairs.emplace(route.nodes[i - 1], route.nodes[i]);
  for (ViewEdge& e : view.edges) {
    e.on_route = e.draw != Draw::removed && route_pairs.contains({e.source, e.target});
  }
  view.route_note = route_note_of(r, c, route);

  for (const NodeIx v : present) {
    std::uint8_t roles = 0;
    if (v == r.header.source) roles |= role_source;
    if (v == r.header.target) roles |= role_target;
    if (contains(r.header.fsc, v)) roles |= role_fsc;
    view.nodes.push_back(ViewNode{v, true, contains(w.active, v), contains(w.relaxed_present, v),
                                  level[v] == unreached ? deepest + 1 : level[v], roles});
  }

  // 6. The parked table: relaxed nodes outside the witness, by label (S6.13).
  for (const NodeIx v : w.relaxed_absent) view.parked.push_back({v, parked_reason(r.tables.nodes[v].observed)});
  std::stable_sort(view.parked.begin(), view.parked.end(), [&](const ParkedNode& a, const ParkedNode& b) {
    const auto order = core::stable_compare(label(r, a.node), label(r, b.node));
    return order != 0 ? order < 0 : a.node < b.node;
  });
  return view;
}

PointView build_view(const SolveResult& r, const CaseResult& c, const FrontierPoint& p) {
  PointView view;
  view.solve = r.header.name;
  view.kind = c.kind;
  view.at = p.at;
  view.enumeration = p.enumeration;
  view.certified = p.certified;
  view.title = title_of(r, c, p.at);
  view.subtitle = subtitle_of(p.enumeration, p.witnesses.size());
  view.certification_note = certification_of(c, p);
  // S6.02: a point without a witness is marked, not drawn empty (FuSION 1's pex drew 0 nodes).
  if (p.witnesses.empty()) view.missing_reason = "No witness was recorded at this point.";
  for (const Witness& w : p.witnesses) view.witnesses.push_back(build_witness_view(r, c, w));
  return view;
}

PointView build_view(const SolveResult& r, const CaseResult& c, const RequestedPoint& q) {
  PointView view;
  view.solve = r.header.name;
  view.kind = c.kind;
  view.at = q.at;
  view.enumeration = Enumeration::not_requested;
  view.certified = false;
  view.title = title_of(r, c, q.at);
  view.certification_note = "A requested point (points=), not a searched Pareto point.";
  if (q.answer == solve::Outcome::sat) {
    view.subtitle = q.witnesses.size() <= 1
                        ? std::string("One explanation found at this requested point.")
                        : "Explanation 1 of " + std::to_string(q.witnesses.size()) + " at this requested point.";
    if (q.witnesses.empty()) view.missing_reason = "No witness was recorded at this point.";
  } else if (q.answer == solve::Outcome::unsat) {
    view.missing_reason = "unsat at this point: no explanation fits these bounds.";
  } else {
    view.missing_reason = "undecided: " + (q.label.empty() ? std::string(unknown_label(q.answer)) : q.label);
  }
  for (const Witness& w : q.witnesses) view.witnesses.push_back(build_witness_view(r, c, w));
  return view;
}

std::vector<bool> kept_by(const SolveResult& r, const WitnessView& view, const PathFilter& filter) {
  // S6.04's four modes over the groups in the witness; subgraph keeps every group, those outside the
  // witness included.
  std::vector<bool> kept(view.edges.size(), filter.mode == PathMode::subgraph);
  if (filter.mode == PathMode::subgraph) return kept;
  const auto present = [&](NodeIx v) {
    return std::any_of(view.nodes.begin(), view.nodes.end(), [&](const ViewNode& n) { return n.node == v; });
  };
  NodeIx from = r.header.source, to = r.header.target;
  if (filter.from) {
    const auto found = find_node(r, *filter.from);
    if (!found) return kept;                    // an endpoint that is not present keeps nothing (psp)
    from = *found;
  }
  if (filter.to) {
    const auto found = find_node(r, *filter.to);
    if (!found) return kept;
    to = *found;
  }
  if (!present(from) || !present(to)) return kept;
  std::vector<std::optional<std::int32_t>> slack;
  if (from == r.header.source && to == r.header.target) {
    for (const ViewEdge& e : view.edges) slack.push_back(e.slack);
  } else {
    // psp let its src and tgt differ from the solve's: the two searches are repeated from them.
    std::vector<std::pair<NodeIx, NodeIx>> pairs;
    for (const ViewEdge& e : view.edges) {
      if (e.in_witness) pairs.emplace_back(e.source, e.target);
    }
    const auto n = r.tables.nodes.size();
    const auto d_s = hops(n, pairs, from, false);
    const auto d_t = hops(n, pairs, to, true);
    const std::int32_t L = d_t[from];
    for (const ViewEdge& e : view.edges) {
      if (!e.in_witness || L == unreached || d_s[e.source] == unreached || d_t[e.target] == unreached) {
        slack.emplace_back();
      } else {
        slack.emplace_back(d_s[e.source] + 1 + d_t[e.target] - L);
      }
    }
  }
  for (std::size_t i = 0; i < view.edges.size(); ++i) {
    if (!slack[i]) continue;
    switch (filter.mode) {
      case PathMode::all: kept[i] = true; break;
      case PathMode::shortest: kept[i] = *slack[i] == 0; break;
      case PathMode::alt: kept[i] = *slack[i] <= filter.window; break;
      case PathMode::subgraph: kept[i] = true; break;
    }
  }
  return kept;
}

std::vector<NodeIx> drawn_nodes(const SolveResult& r, const WitnessView& view, const PathFilter& filter,
                                const std::vector<bool>& kept) {
  // psp: in subgraph mode every present node; otherwise the ends of the kept edges plus the endpoints,
  // and nothing at all when no path is left (GraphManagerNew.cc:9036-9070).
  std::vector<NodeIx> out;
  if (filter.mode == PathMode::subgraph) {
    for (const ViewNode& n : view.nodes) out.push_back(n.node);
    return out;
  }
  for (std::size_t i = 0; i < view.edges.size(); ++i) {
    if (!kept[i]) continue;
    out.push_back(view.edges[i].source);
    out.push_back(view.edges[i].target);
  }
  if (out.empty()) return out;
  const auto endpoint = [&](const std::optional<network::NodeKey>& key, NodeIx fallback) {
    if (!key) return std::optional<NodeIx>(fallback);
    return find_node(r, *key);
  };
  for (const auto& end : {endpoint(filter.from, r.header.source), endpoint(filter.to, r.header.target)}) {
    if (end) out.push_back(*end);
  }
  std::sort(out.begin(), out.end());
  out.erase(std::unique(out.begin(), out.end()), out.end());
  return out;
}

}  // namespace fusion::results
