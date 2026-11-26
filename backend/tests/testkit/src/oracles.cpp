// walk_corridor and exhaustive_frontier (oracles.hpp).
#include "fusion/testkit/oracles.hpp"

#include <algorithm>
#include <functional>
#include <map>
#include <set>

#include "model.hpp"
#include "util.hpp"

namespace fusion::testkit {

using detail::sort_unique;

// ---------------------------------------------------------------- walk_corridor (S4.04 to S4.11, 05.C2)

CanonCorridor walk_corridor(const CanonGraph& g, std::span<const std::string> sources,
                            std::span<const std::string> targets, int bound, const WalkRules& rules) {
  const detail::Resolver resolve(g);
  const std::size_t n = g.nodes.size();
  std::vector<bool> source(n, false), target(n, false), excluded(n, false);
  CanonCorridor out;
  out.bound = bound;
  for (const std::string& s : sources)
    if (auto v = resolve(s)) {
      source[*v] = true;
      out.sources.push_back(g.nodes[*v].key);
    }
  for (const std::string& t : targets)
    if (auto v = resolve(t)) {
      target[*v] = true;
      out.targets.push_back(g.nodes[*v].key);
    }
  sort_unique(out.sources);
  sort_unique(out.targets);
  // S4.06: an excluded id removes its node and every complex that contains it (entry 170).
  for (const std::string& id : rules.exclude) {
    const auto v = resolve(id);
    if (v) excluded[*v] = true;
    for (std::size_t c = 0; c < n; ++c)
      if (detail::contains(g.nodes[c], v ? &g.nodes[*v] : nullptr, id)) excluded[c] = true;
  }
  const auto carries = [](const CanonEdge& e, std::initializer_list<std::string_view> names) {
    return std::any_of(e.key.subtypes.begin(), e.key.subtypes.end(),
                       [&](const std::string& s) { return std::find(names.begin(), names.end(), s) != names.end(); });
  };
  std::map<std::string, std::size_t> index;
  for (std::size_t v = 0; v < n; ++v) index.emplace(g.nodes[v].key, v);
  std::vector<std::vector<std::size_t>> out_edges(n);
  for (std::size_t j = 0; j < g.edges.size(); ++j) {
    const CanonEdge& e = g.edges[j];
    const std::size_t u = index.at(e.key.src), v = index.at(e.key.tgt);
    bool skipped = false;
    for (const std::string& s : e.key.subtypes)
      if (std::find(rules.skip_subtypes.begin(), rules.skip_subtypes.end(), s) != rules.skip_subtypes.end()) skipped = true;
    if (skipped || excluded[u] || excluded[v]) continue;                       // S4.04, S4.06
    if (u == v || source[v] || target[u]) continue;                             // trimmed (S4.11, 05.C2)
    if (target[v]) {                                                            // S4.05
      if (rules.to_target == WalkRules::ToTarget::activation && !carries(e, {"activation", "inhibition", "phosphorylation"}))
        continue;
      if (rules.to_target == WalkRules::ToTarget::expression && !carries(e, {"expression", "repression"})) continue;
    }
    out_edges[u].push_back(j);
  }
  // Every walk of at most `bound` edges from a source; one that reaches a target keeps all its edges.
  std::vector<bool> kept(g.edges.size(), false);
  std::vector<std::size_t> path;
  std::function<void(std::size_t)> walk = [&](std::size_t u) {
    if (target[u]) {
      for (std::size_t j : path) kept[j] = true;
      return;
    }
    if (static_cast<int>(path.size()) >= bound) return;
    for (std::size_t j : out_edges[u]) {
      path.push_back(j);
      walk(index.at(g.edges[j].key.tgt));
      path.pop_back();
    }
  };
  for (std::size_t s = 0; s < n; ++s)
    if (source[s] && !excluded[s]) walk(s);
  std::set<std::size_t> nodes;
  for (std::size_t j = 0; j < g.edges.size(); ++j)
    if (kept[j]) {
      out.graph.edges.push_back(g.edges[j]);
      nodes.insert(index.at(g.edges[j].key.src));
      nodes.insert(index.at(g.edges[j].key.tgt));
    }
  for (std::size_t v : nodes) out.graph.nodes.push_back(g.nodes[v]);
  normalise(out.graph);
  return out;
}

// ---------------------------------------------------------------- exhaustive_frontier

Frontier exhaustive_frontier(const CanonCorridor& corridor, const CaseSpec& spec, Semantics semantics, Point box_lo,
                             Point box_hi) {
  const detail::Model m = detail::build_model(corridor, spec, semantics);
  Frontier out;
  if (!m.problems.empty()) return out;
  const std::vector<std::uint32_t>& vars = m.encoded_nodes;
  if (vars.size() > 12) detail::fail("exhaustive_frontier: " + std::to_string(vars.size()) + " encoded nodes is too many");
  std::vector<std::uint8_t> state(m.nodes, 0);
  std::vector<bool> selected(m.edges, false);
  std::set<Point> found;
  const auto dominated = [&](Point q) {
    return std::any_of(found.begin(), found.end(), [&](Point f) { return f.n <= q.n && f.e <= q.e; });
  };
  for (;;) {
    if (detail::states_ok(m, state, nullptr)) {
      const Point c = detail::cost_of(m, state);
      const Point q{std::max(c.n, box_lo.n), std::max(c.e, box_lo.e)};   // a witness below the box counts at its corner
      if (q.n <= box_hi.n && q.e <= box_hi.e && !dominated(q)) {
        std::vector<std::uint32_t> usable;                     // encoded edges between present ends
        for (std::uint32_t j : m.encoded_edges)
          if (state[m.src[j]] != 0 && state[m.tgt[j]] != 0) usable.push_back(j);
        if (usable.size() > 20) detail::fail("exhaustive_frontier: too many edges between present nodes");
        for (std::uint64_t mask = 0; mask < (std::uint64_t{1} << usable.size()); ++mask) {
          for (std::size_t k = 0; k < usable.size(); ++k) selected[usable[k]] = (mask >> k) & 1U;
          if (detail::selection_ok(m, state, selected, nullptr)) {
            found.insert(q);
            break;
          }
        }
        for (std::uint32_t j : usable) selected[j] = false;
      }
    }
    std::size_t i = 0;                                         // the next node states, in base 3
    while (i < vars.size() && state[vars[i]] == 2) state[vars[i++]] = 0;
    if (i == vars.size()) break;
    ++state[vars[i]];
  }
  for (Point p : found)
    if (std::none_of(found.begin(), found.end(), [&](Point o) { return o != p && o.n <= p.n && o.e <= p.e; })) {
      out.points.push_back(p);
      out.certified.push_back(true);
    }
  return out;
}

}  // namespace fusion::testkit
