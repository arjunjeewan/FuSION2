// The independent witness checker (check.hpp), written from S5.04 to S5.17 and section 12's two
// semantics. It shares no code with the solve: the encoded sets, the observed sets, the counted genes,
// the FSC cases and the complex rule are all derived here, from the corridor and the case's ids.
#include "fusion/testkit/check.hpp"

#include <algorithm>
#include <deque>
#include <limits>
#include <map>
#include <set>

#include "model.hpp"
#include "util.hpp"

namespace fusion::testkit {

namespace detail {

namespace {

constexpr int kFar = std::numeric_limits<int>::max() / 4;

bool has_subtype(const CanonEdge& e, std::string_view name) {
  return std::find(e.key.subtypes.begin(), e.key.subtypes.end(), name) != e.key.subtypes.end();
}

}  // namespace

// The ids of a corridor's nodes, as check.hpp describes the resolution.
Resolver::Resolver(const CanonGraph& g) {
  for (std::uint32_t v = 0; v < g.nodes.size(); ++v) {
    by_key_.emplace(g.nodes[v].key, v);
    by_module_key_.emplace(module_key(g.nodes[v]), v);
    if (is_complex(g.nodes[v])) continue;
    for (const std::string& m : g.nodes[v].members) by_member_.emplace(m, v);
  }
}

std::optional<std::uint32_t> Resolver::operator()(const std::string& id) const {
  for (const auto* table : {&by_key_, &by_module_key_, &by_member_}) {
    const auto it = table->find(id);
    if (it != table->end()) return it->second;
  }
  const std::string bare = strip_colons(id);
  if (bare != id) return (*this)(bare);
  return std::nullopt;
}

std::vector<std::uint32_t> Resolver::all(const std::vector<std::string>& ids) const {
  std::vector<std::uint32_t> out;
  for (const std::string& id : ids)
    if (auto v = (*this)(id)) out.push_back(*v);
  sort_unique(out);
  return out;
}

bool is_complex(const CanonNode& n) { return n.type == "group" || is_complex_key(n.key); }

// Whether complex c contains the gene or node: a gene id among its members, a node's members among its
// members, or a complex among its components, nested ones included (S4.06; entries 170 and 229).
bool contains(const CanonNode& c, const CanonNode* node, const std::string& id) {
  if (!is_complex(c)) return false;
  const auto has = [&](const std::string& m) { return std::binary_search(c.members.begin(), c.members.end(), m); };
  if (!node) return has(strip_colons(id));
  if (node->key == c.key) return false;
  if (!is_complex(*node)) return std::any_of(node->members.begin(), node->members.end(), has);
  if (has(node->key)) return true;                           // a complex as canon(CaseGraph) writes it
  std::vector<std::string> stack = components_of(c.key);
  while (!stack.empty()) {
    const std::string k = stack.back();
    stack.pop_back();
    if (k == node->key) return true;
    for (const std::string& part : components_of(k)) stack.push_back(part);
  }
  return false;
}

Model build_model(const CanonCorridor& corridor, const CaseSpec& input, Semantics semantics) {
  const CanonGraph& g = corridor.graph;
  const bool f2 = semantics == Semantics::fusion2;
  Model m;
  m.graph = &g;
  m.nodes = g.nodes.size();
  m.edges = g.edges.size();
  m.bound = corridor.bound;
  const Resolver resolve(g);
  std::map<std::string, std::uint32_t> index;
  for (std::uint32_t v = 0; v < m.nodes; ++v) index.emplace(g.nodes[v].key, v);
  for (const CanonEdge& e : g.edges) {
    const auto s = index.find(e.key.src), t = index.find(e.key.tgt);
    if (s == index.end() || t == index.end()) fail("an edge names a node the graph lacks: " + e.key.text());
    m.src.push_back(s->second);
    m.tgt.push_back(t->second);
    m.inhibition.push_back(has_subtype(e, "inhibition") || has_subtype(e, "repression"));   // S5.13
  }
  m.source.assign(m.nodes, false);
  m.target.assign(m.nodes, false);
  for (const std::string& s : corridor.sources) {
    if (auto v = resolve(s)) m.source[*v] = true;
    else m.problems.push_back("the source " + s + " is not in the corridor");
  }
  for (const std::string& t : corridor.targets) {
    if (auto v = resolve(t)) m.target[*v] = true;
    else m.problems.push_back("the target " + t + " is not in the corridor");
  }

  // FuSION 1 has no FSC argument: its batches put the FSC on the lists (S5.17).
  CaseSpec spec = input;
  if (!f2 && !spec.fsc.empty()) {
    if (spec.solve_case == CaseSpec::Case::in)
      for (const std::string& id : spec.fsc) {
        spec.essential.push_back(id);
        spec.active.push_back(id);
        spec.nonrelaxed.push_back(id);
      }
    if (spec.solve_case == CaseSpec::Case::out) spec.avoid.insert(spec.avoid.end(), spec.fsc.begin(), spec.fsc.end());
  }

  // ---- the encoded sets (S5.10), and for fusion2 first the trim of 05.C2
  m.in_n.assign(m.nodes, false);
  m.in_e.assign(m.edges, false);
  std::vector<bool> in_corridor(m.nodes, true);              // where observations and lists bind
  if (f2) {
    std::vector<std::vector<std::uint32_t>> out(m.nodes), in(m.nodes);
    for (std::uint32_t j = 0; j < m.edges; ++j) {
      const std::uint32_t u = m.src[j], v = m.tgt[j];
      if (u == v || m.source[v] || m.target[u]) continue;    // trimmed during the search (S4.11, 05.C2)
      out[u].push_back(j);
      in[v].push_back(j);
    }
    const auto bfs = [&](const std::vector<bool>& start, bool forward) {
      std::vector<int> d(m.nodes, kFar);
      std::deque<std::uint32_t> queue;
      for (std::uint32_t v = 0; v < m.nodes; ++v)
        if (start[v]) {
          d[v] = 0;
          queue.push_back(v);
        }
      while (!queue.empty()) {
        const std::uint32_t u = queue.front();
        queue.pop_front();
        for (std::uint32_t j : forward ? out[u] : in[u]) {
          const std::uint32_t w = forward ? m.tgt[j] : m.src[j];
          if (d[w] == kFar) {
            d[w] = d[u] + 1;
            queue.push_back(w);
          }
        }
      }
      return d;
    };
    const std::vector<int> ds = bfs(m.source, true), dt = bfs(m.target, false);
    in_corridor.assign(m.nodes, false);
    for (std::uint32_t u = 0; u < m.nodes; ++u)
      for (std::uint32_t j : out[u]) {
        const std::uint32_t v = m.tgt[j];
        if (ds[u] < kFar && dt[v] < kFar && ds[u] + 1 + dt[v] <= m.bound) {   // S4.09's keep rule
          m.in_e[j] = true;
          in_corridor[u] = in_corridor[v] = true;
        }
      }
    for (std::uint32_t v = 0; v < m.nodes; ++v)
      m.in_n[v] = in_corridor[v] || m.source[v] || m.target[v];   // the endpoints are always encoded
  } else {
    // FuSION 1's walk over (node, target) pairs: every out-edge of an expanded node, never expanding the target.
    std::vector<std::vector<std::uint32_t>> all_out(m.nodes);
    for (std::uint32_t j = 0; j < m.edges; ++j) all_out[m.src[j]].push_back(j);
    for (std::uint32_t t = 0; t < m.nodes; ++t) {
      if (!m.target[t]) continue;
      std::vector<bool> seen(m.nodes, false);
      std::deque<std::uint32_t> queue;
      for (std::uint32_t s = 0; s < m.nodes; ++s)
        if (m.source[s] && s != t) {
          seen[s] = true;
          queue.push_back(s);
        }
      while (!queue.empty()) {
        const std::uint32_t u = queue.front();
        queue.pop_front();
        for (std::uint32_t j : all_out[u]) {
          m.in_e[j] = true;
          m.in_n[u] = m.in_n[m.tgt[j]] = true;
          const std::uint32_t w = m.tgt[j];
          if (w != t && !seen[w]) {
            seen[w] = true;
            queue.push_back(w);
          }
        }
      }
    }
    for (std::uint32_t v : resolve.all(spec.essential)) m.in_n[v] = true;   // whether reached or not
  }
  m.out_edges.assign(m.nodes, {});
  m.in_edges.assign(m.nodes, {});
  for (std::uint32_t j = 0; j < m.edges; ++j)
    if (m.in_e[j]) {
      m.encoded_edges.push_back(j);
      m.out_edges[m.src[j]].push_back(j);
      m.in_edges[m.tgt[j]].push_back(j);
    }
  for (std::uint32_t v = 0; v < m.nodes; ++v)
    if (m.in_n[v]) m.encoded_nodes.push_back(v);

  // ---- the case's observed sets (S5.07, S5.17)
  const auto bound_in_corridor = [&](const std::vector<std::string>& ids) {
    std::set<std::uint32_t> out;
    for (std::uint32_t v : resolve.all(ids))
      if (in_corridor[v]) out.insert(v);
    return out;
  };
  const std::vector<std::uint32_t> avoid = resolve.all(spec.avoid);
  const std::vector<std::uint32_t> fsc = f2 ? resolve.all(spec.fsc) : std::vector<std::uint32_t>{};
  const std::set<std::uint32_t> confirmed_up = bound_in_corridor(spec.confirmed_up);
  const std::set<std::uint32_t> confirmed_down = bound_in_corridor(spec.confirmed_down);
  std::set<std::uint32_t> up = bound_in_corridor(spec.up), down = bound_in_corridor(spec.down);
  for (std::uint32_t v : avoid) up.erase(v);                 // only the up set loses avoided nodes (S5.07)
  for (std::uint32_t v : confirmed_down) up.erase(v);
  up.insert(confirmed_up.begin(), confirmed_up.end());
  for (std::uint32_t v : confirmed_up) down.erase(v);
  down.insert(confirmed_down.begin(), confirmed_down.end());
  const bool case_in = f2 && spec.solve_case == CaseSpec::Case::in;
  const bool case_out = f2 && spec.solve_case == CaseSpec::Case::out;
  if (f2 && spec.solve_case != CaseSpec::Case::free && spec.fsc.empty())
    m.problems.push_back("the cases in and out need an FSC (06.C10)");
  if (case_in)
    for (const std::string& id : spec.fsc)
      if (!resolve(id)) m.problems.push_back("the FSC " + id + " of the in case is not in the corridor");
  if (case_out)
    for (std::uint32_t v : fsc) up.erase(v), down.erase(v);  // the out case removes the FSC (S5.17)
  for (std::uint32_t v : up) down.erase(v);                  // a node in both is up (S3.07)
  m.regulation.assign(m.nodes, 0);
  for (std::uint32_t v : up) m.regulation[v] = 1;
  for (std::uint32_t v : down) m.regulation[v] = 2;

  const auto label = [&](std::uint32_t v) { return g.nodes[v].key; };
  const auto need = [&](std::vector<Model::Need>& list, std::uint32_t v, const std::string& why) {
    list.push_back({v, why});
  };
  const auto is_fsc = [&](std::uint32_t v) { return std::binary_search(fsc.begin(), fsc.end(), v); };

  // ---- node requirements (S5.05, S5.17, 06.C16)
  for (std::uint32_t v : resolve.all(spec.essential)) need(m.present, v, "essential");
  for (std::uint32_t v : avoid) need(m.absent, v, "avoided");
  for (std::uint32_t v : resolve.all(spec.active)) need(m.active, v, "on the active list");
  for (std::uint32_t v : resolve.all(spec.inactive)) need(m.inactive, v, "on the inactive list");
  for (std::uint32_t v = 0; v < m.nodes; ++v) {
    if (!m.source[v] && !m.target[v]) continue;
    const std::string what = m.source[v] ? "the source" : "the target";
    if (f2 && spec.named) {                                  // 06.C16
      need(m.present, v, what);
      need(m.active, v, what);
    } else if (m.source[v]) {
      need(m.active, v, "the source (S5.04)");               // FuSION 1 forces the source active
    }
  }
  if (f2) {
    // Every avoided gene, on the list or as the out case's FSC, forbids every complex that holds it.
    std::vector<std::pair<const CanonNode*, std::string>> forbidding;
    for (const std::string& id : spec.avoid) {
      const auto v = resolve(id);
      forbidding.emplace_back(v ? &g.nodes[*v] : nullptr, id);
    }
    if (case_out)
      for (const std::string& id : spec.fsc) {
        const auto v = resolve(id);
        forbidding.emplace_back(v ? &g.nodes[*v] : nullptr, id);
      }
    for (std::uint32_t c = 0; c < m.nodes; ++c)
      for (const auto& [node, id] : forbidding)
        if (contains(g.nodes[c], node, id)) {
          need(m.absent, c, "a complex holding the avoided " + id);
          break;
        }
    if (case_in)
      for (std::uint32_t v : fsc) {
        need(m.present, v, "the FSC of the in case");
        if (m.regulation[v] == 2) need(m.inactive, v, "the down-regulated FSC of the in case");
        else need(m.active, v, "the FSC of the in case");
      }
    if (case_out)
      for (std::uint32_t v : fsc) need(m.absent, v, "the FSC of the out case");
  }
  const std::vector<std::uint32_t> nonrelaxed = resolve.all(spec.nonrelaxed);
  const auto endpoint = [&](std::uint32_t v) { return m.source[v] || m.target[v]; };
  for (std::uint32_t v : nonrelaxed) {
    if (endpoint(v) || m.regulation[v] == 0 || !m.in_n[v]) continue;
    if (f2 && is_fsc(v) && spec.solve_case != CaseSpec::Case::free) continue;   // the FSC rule holds it
    need(m.consistent, v, "non-relaxed");
  }
  for (const auto& [a, b] : spec.coexpressed) {              // S5.15
    const auto u = resolve(a), v = resolve(b);
    if (!u || !v || *u == *v || !m.in_n[*u] || !m.in_n[*v]) continue;
    m.coexpressed.emplace_back(std::min(*u, *v), std::max(*u, *v));
  }
  sort_unique(m.coexpressed);

  // ---- the counted observations (S5.12)
  for (std::uint32_t v : m.encoded_nodes) {
    if (m.regulation[v] == 0 || endpoint(v)) continue;
    if (f2) {
      if (std::binary_search(nonrelaxed.begin(), nonrelaxed.end(), v)) continue;
      if (std::binary_search(avoid.begin(), avoid.end(), v)) continue;
      if (case_in && is_fsc(v)) continue;
    }
    m.counted.push_back(v);
  }

  // ---- edge lists (S5.06, as sets of subtypes) and frozen edges (S5.16)
  const auto matching = [&](const EdgeSpec& line) {
    std::vector<std::uint32_t> out;
    const auto s = resolve(line.src), t = resolve(line.tgt);
    if (!s || !t) return out;
    std::vector<std::string> wanted = line.subtypes;
    wanted.erase(std::remove(wanted.begin(), wanted.end(), "dummy_u_to_d"), wanted.end());
    sort_unique(wanted);
    for (std::uint32_t j = 0; j < m.edges; ++j) {
      if (m.src[j] != *s || m.tgt[j] != *t) continue;
      std::vector<std::string> have = g.edges[j].key.subtypes;
      sort_unique(have);
      if (have == wanted) out.push_back(j);
    }
    return out;
  };
  for (const EdgeSpec& line : spec.essential_edges)
    for (std::uint32_t j : matching(line)) {
      if (m.in_e[j]) need(m.edge_selected, j, "essential");
      else if (f2) m.problems.push_back("the essential edge " + g.edges[j].key.text() + " is not encoded");
    }
  for (const EdgeSpec& line : spec.avoid_edges)
    for (std::uint32_t j : matching(line))
      if (m.in_e[j]) need(m.edge_avoided, j, "avoided");
  for (const EdgeSpec& line : spec.nonrelaxed_edges)
    for (std::uint32_t j : matching(line))
      if (m.in_e[j]) need(m.edge_nonrelaxed, j, "non-relaxed");
  if (spec.frozen_threshold)
    for (std::uint32_t j : m.encoded_edges)
      if (g.edges[j].score > *spec.frozen_threshold) need(m.frozen, j, "frozen");   // strict > (S5.16)

  // A node that must be present but lies outside the encoding makes every assignment invalid (S5.11).
  for (const Model::Need& n : m.present)
    if (!m.in_n[n.index]) m.problems.push_back(label(n.index) + " (" + n.why + ") is outside the encoding");
  return m;
}

bool relaxed_edge(const Model& m, std::uint32_t j, const std::vector<std::uint8_t>& state) {
  const std::uint8_t a = state[m.src[j]], b = state[m.tgt[j]];
  if (a == 0 || b == 0) return false;                       // an edge with an absent end is never relaxed
  if (m.inhibition[j]) return a == b;                       // both active or both inactive
  return !(a == 2 && b == 2);                               // not both active
}

bool relaxed_node(const Model& m, std::uint32_t v, const std::vector<std::uint8_t>& state) {
  const bool on = state[v] == 2;                            // present and active
  return m.regulation[v] == 1 ? !on : m.regulation[v] == 2 ? on : false;
}

Point cost_of(const Model& m, const std::vector<std::uint8_t>& state) {
  Point c;
  for (std::uint32_t v : m.counted) c.n += relaxed_node(m, v, state) ? 1 : 0;
  for (std::uint32_t j : m.encoded_edges) c.e += relaxed_edge(m, j, state) ? 1 : 0;
  return c;
}

bool states_ok(const Model& m, const std::vector<std::uint8_t>& state, std::vector<std::string>* why) {
  bool ok = true;
  const auto fail = [&](std::string text) {
    ok = false;
    if (why) why->push_back(std::move(text));
    return why == nullptr;                                  // stop unless explaining
  };
  const auto& g = *m.graph;
  for (const std::string& p : m.problems)
    if (fail(p)) return false;
  for (std::uint32_t v = 0; v < m.nodes; ++v)
    if (state[v] != 0 && !m.in_n[v] && fail(g.nodes[v].key + " is present but outside the encoding")) return false;
  for (const auto& n : m.present)
    if (state[n.index] == 0 && fail(g.nodes[n.index].key + " must be present (" + n.why + ")")) return false;
  for (const auto& n : m.absent)
    if (state[n.index] != 0 && fail(g.nodes[n.index].key + " must be absent (" + n.why + ")")) return false;
  for (const auto& n : m.active)
    if (state[n.index] == 1 && fail(g.nodes[n.index].key + " must be active (" + n.why + ")")) return false;
  for (const auto& n : m.inactive)
    if (state[n.index] == 2 && fail(g.nodes[n.index].key + " must be inactive (" + n.why + ")")) return false;
  for (const auto& n : m.consistent)
    if (relaxed_node(m, n.index, state) && fail(g.nodes[n.index].key + " must not be relaxed (" + n.why + ")"))
      return false;
  for (const auto& [u, v] : m.coexpressed)
    if (state[u] != 0 && state[v] != 0 && state[u] != state[v] &&
        fail("coexpressed " + g.nodes[u].key + " and " + g.nodes[v].key + " differ in activity (S5.15)"))
      return false;
  for (const auto& n : m.edge_nonrelaxed)
    if (relaxed_edge(m, n.index, state) && fail("the non-relaxed edge " + g.edges[n.index].key.text() + " is relaxed"))
      return false;
  for (const auto& n : m.frozen)
    if (relaxed_edge(m, n.index, state) && fail("the frozen edge " + g.edges[n.index].key.text() + " is relaxed (S5.16)"))
      return false;
  return ok;
}

bool selection_ok(const Model& m, const std::vector<std::uint8_t>& state, const std::vector<bool>& selected,
                  std::vector<std::string>* why) {
  bool ok = true;
  const auto fail = [&](std::string text) {
    ok = false;
    if (why) why->push_back(std::move(text));
    return why == nullptr;
  };
  const auto& g = *m.graph;
  std::vector<std::vector<std::uint32_t>> sel_in(m.nodes), sel_out(m.nodes);   // selected edges per node
  for (std::uint32_t j = 0; j < m.edges; ++j) {
    if (!selected[j]) continue;
    if (!m.in_e[j] && fail("the selected edge " + g.edges[j].key.text() + " is outside the encoding")) return false;
    if ((state[m.src[j]] == 0 || state[m.tgt[j]] == 0) &&
        fail("the selected edge " + g.edges[j].key.text() + " has an absent end (S5.11)"))
      return false;
    sel_out[m.src[j]].push_back(j);
    sel_in[m.tgt[j]].push_back(j);
  }
  for (std::uint32_t v = 0; v < m.nodes; ++v) {                 // S5.11: present nodes lie on selected paths
    if (state[v] == 0) continue;
    if (!m.source[v] && sel_in[v].empty() && fail(g.nodes[v].key + " is present with no selected in-edge (S5.11)"))
      return false;
    if (!m.target[v] && sel_out[v].empty() && fail(g.nodes[v].key + " is present with no selected out-edge (S5.11)"))
      return false;
  }
  // Distances: the selected edges form a DAG, and the longest selected path into a target is at most b.
  std::vector<int> indegree(m.nodes, 0), depth(m.nodes, 0);
  for (std::uint32_t v = 0; v < m.nodes; ++v) indegree[v] = static_cast<int>(sel_in[v].size());
  std::vector<std::uint32_t> order;
  order.reserve(m.nodes);
  for (std::uint32_t v = 0; v < m.nodes; ++v)
    if (indegree[v] == 0) order.push_back(v);
  for (std::size_t k = 0; k < order.size(); ++k) {
    const std::uint32_t u = order[k];
    for (std::uint32_t j : sel_out[u]) {
      const std::uint32_t w = m.tgt[j];
      depth[w] = std::max(depth[w], depth[u] + 1);
      if (--indegree[w] == 0) order.push_back(w);
    }
  }
  if (order.size() != m.nodes && fail("the selected edges have a cycle (S5.11)")) return false;
  for (std::uint32_t t = 0; t < m.nodes; ++t)
    if (m.target[t] && depth[t] > m.bound &&
        fail("a selected path into " + g.nodes[t].key + " is longer than the bound " + std::to_string(m.bound)))
      return false;
  // S5.14: the flag of a selected edge is forced clear when its tail is not DE and is a source or has a
  // selected in-edge forced clear; a selected in-edge of a target must have its flag set.
  std::vector<bool> clean(m.edges, false);
  for (std::uint32_t u : order) {
    if (m.regulation[u] != 0) continue;
    bool from_clean = m.source[u];
    for (std::uint32_t j : sel_in[u]) from_clean = from_clean || clean[j];
    if (!from_clean) continue;
    for (std::uint32_t j : sel_out[u]) clean[j] = true;
  }
  for (std::uint32_t j = 0; j < m.edges; ++j)
    if (selected[j] && m.target[m.tgt[j]] && clean[j] &&
        fail("a selected path reaches " + g.nodes[m.tgt[j]].key + " without crossing a DE gene (S5.14)"))
      return false;
  for (const auto& n : m.edge_selected)
    if (!selected[n.index] && fail("the essential edge " + g.edges[n.index].key.text() + " is not selected")) return false;
  for (const auto& n : m.edge_avoided)
    if (selected[n.index] && fail("the avoided edge " + g.edges[n.index].key.text() + " is selected")) return false;
  return ok;
}

}  // namespace detail

Verdict check(const CanonCorridor& corridor, const CaseSpec& spec, const Assignment& a, Semantics semantics) {
  const detail::Model m = detail::build_model(corridor, spec, semantics);
  const CanonGraph& g = corridor.graph;
  Verdict v;
  std::vector<std::uint8_t> state(m.nodes, 0);
  std::vector<bool> selected(m.edges, false);
  for (const auto& [key, active] : a.present) {
    const CanonNode* n = g.node(key);
    if (!n) {
      v.violations.push_back("the node " + key + " is not in the corridor");
      continue;
    }
    state[static_cast<std::size_t>(n - g.nodes.data())] = active ? 2 : 1;
  }
  for (const CanonEdgeKey& key : a.selected) {
    struct ByKey {
      bool operator()(const CanonEdge& e, const CanonEdgeKey& k) const { return e.key < k; }
      bool operator()(const CanonEdgeKey& k, const CanonEdge& e) const { return k < e.key; }
    };
    const auto range = std::equal_range(g.edges.begin(), g.edges.end(), key, ByKey{});
    if (range.first == range.second) v.violations.push_back("the edge " + key.text() + " is not in the corridor");
    for (auto it = range.first; it != range.second; ++it) selected[static_cast<std::size_t>(it - g.edges.begin())] = true;
  }
  const bool states = detail::states_ok(m, state, &v.violations);
  const bool paths = detail::selection_ok(m, state, selected, &v.violations);
  v.cost = detail::cost_of(m, state);
  v.valid = states && paths && v.violations.empty();
  return v;
}

Assignment from_f1(const CanonCorridor& corridor, const f1::Witness& w) {
  const CanonGraph& g = corridor.graph;
  std::map<std::string, std::string> by_rep;                // FuSION 1's key -> canonical key
  for (const CanonNode& n : g.nodes) by_rep.emplace(module_key(n), n.key);
  const auto key_of = [&](const std::string& rep) {
    const auto it = by_rep.find(rep);
    return it == by_rep.end() ? "f1:" + rep : it->second;
  };
  Assignment a;
  for (const f1::NodeState& s : w.present_nodes)
    if (s.present) a.present.emplace_back(key_of(s.rep), s.active);
  for (const f1::EdgeKey& e : w.selected_edges) {
    const std::string src = key_of(e.src_rep), tgt = key_of(e.tgt_rep);
    std::vector<std::string> subtypes = e.subtypes;
    std::sort(subtypes.begin(), subtypes.end());
    bool found = false;
    for (const CanonEdge& c : g.edges)
      if (c.key.src == src && c.key.tgt == tgt && c.key.subtypes == subtypes) {
        a.selected.push_back(c.key);
        found = true;
      }
    if (!found) a.selected.push_back({src, tgt, "f1:untyped", subtypes});
  }
  detail::sort_unique(a.selected);
  return a;
}

}  // namespace fusion::testkit
