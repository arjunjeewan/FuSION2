// Module types as kit types (adapt.hpp).
#include "fusion/testkit/adapt.hpp"

#include <algorithm>
#include <map>

namespace fusion::testkit::adapt {

namespace {

using network::NodeIdx;

// Canonical nodes of a network, made on demand (a complex needs its components' keys).
class NetworkKeys {
 public:
  explicit NetworkKeys(const network::Network& n) : net_(n), done_(n.nodes().size()), made_(n.nodes().size(), false) {}
  const CanonNode& operator()(NodeIdx v) {
    if (made_[v]) return done_[v];
    CanonNode c;
    if (net_.kind(v) == network::NodeKind::complex) {
      std::vector<std::string> parts, ids;
      for (NodeIdx p : net_.components(v)) {
        const CanonNode& part = (*this)(p);
        parts.push_back(part.key);
        ids.insert(ids.end(), part.members.begin(), part.members.end());
      }
      std::sort(ids.begin(), ids.end());
      ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
      c = {complex_key(parts), ids, std::string(net_.interner().str(net_.nodes()[v].type))};
    } else {
      std::vector<std::string> ids;
      for (core::GeneId m : net_.members(v)) ids.emplace_back(net_.interner().str(m));
      c = {node_key(ids), ids, std::string(net_.interner().str(net_.nodes()[v].type))};
      std::sort(c.members.begin(), c.members.end());
    }
    made_[v] = true;
    done_[v] = std::move(c);
    return done_[v];
  }

 private:
  const network::Network& net_;
  std::vector<CanonNode> done_;
  std::vector<bool> made_;
};

CanonEdge edge_of(const network::Network& n, network::EdgeIdx j, NetworkKeys& keys) {
  const network::Edge& e = n.edges()[j];
  CanonEdge c;
  c.key.src = keys(e.src).key;
  c.key.tgt = keys(e.tgt).key;
  c.key.type = std::string(n.interner().str(e.type));
  for (core::Sym s : n.subtypes(j)) c.key.subtypes.emplace_back(n.interner().str(s));
  c.dummy_tag = (e.flags & network::bit(network::EdgeFlag::dummy_u_to_d)) != 0;
  for (core::Sym p : n.pathways(j)) c.pathways.emplace_back(n.interner().str(p));
  c.score = e.score;
  return c;
}

CanonGraph canon_view(const network::GraphView& view) {
  const network::Network& n = view.network();
  NetworkKeys keys(n);
  CanonGraph g;
  view.for_each_node([&](NodeIdx v) { g.nodes.push_back(keys(v)); });
  view.for_each_edge([&](network::EdgeIdx j) {
    const network::Edge& e = n.edges()[j];
    if (view.has_node(e.src) && view.has_node(e.tgt)) g.edges.push_back(edge_of(n, j, keys));
  });
  normalise(g);
  return g;
}

CaseSpec::Case case_of(solve::Case c) {
  switch (c) {
    case solve::Case::in: return CaseSpec::Case::in;
    case solve::Case::out: return CaseSpec::Case::out;
    case solve::Case::free: return CaseSpec::Case::free;
  }
  return CaseSpec::Case::free;
}

std::vector<std::string> keys_of(const std::vector<solve::CaseSpec::Id>& ids) {
  std::vector<std::string> out;
  for (const auto& id : ids) out.push_back(id.node.value);
  return out;
}

}  // namespace

CanonGraph canon(const network::Network& n) {
  NetworkKeys keys(n);
  CanonGraph g;
  for (NodeIdx v = 0; v < n.nodes().size(); ++v) g.nodes.push_back(keys(v));
  for (network::EdgeIdx j = 0; j < n.edges().size(); ++j) g.edges.push_back(edge_of(n, j, keys));
  normalise(g);
  return g;
}

CanonGraph canon(const network::GraphView& view) { return canon_view(view); }

CanonCorridor canon(const corridor::Corridor& c) {
  CanonCorridor out;
  out.graph = canon_view(c.view());
  NetworkKeys keys(c.network());
  out.sources = {keys(c.source()).key};
  out.targets = {keys(c.target()).key};
  out.bound = static_cast<int>(c.bound());
  return out;
}

CanonCorridor canon(const solve::CaseGraph& graph) {
  CanonCorridor out;
  for (const solve::CaseGraph::Node& n : graph.nodes) {
    if (n.contains.empty()) {
      out.graph.nodes.push_back({n.key.value, {n.key.value}, "gene"});
    } else {
      std::vector<std::string> members;
      for (const network::NodeKey& k : n.contains) members.push_back(k.value);
      out.graph.nodes.push_back({n.key.value, members, "group"});
    }
  }
  for (const solve::CaseGraph::Edge& e : graph.edges) {
    CanonEdge c;
    c.key = {graph.nodes[e.src].key.value, graph.nodes[e.tgt].key.value, e.key.type, e.key.subtypes};
    c.score = e.score;
    out.graph.edges.push_back(std::move(c));
  }
  normalise(out.graph);
  out.sources = {graph.nodes[graph.source].key.value};
  out.targets = {graph.nodes[graph.target].key.value};
  out.bound = static_cast<int>(graph.bound);
  return out;
}

CaseSpec spec(const solve::CaseSpec& s, const solve::Settings& settings, solve::Case kase) {
  if (!s.essential_edges.empty() || !s.avoid_edges.empty() || !s.nonrelaxed_edges.empty())
    throw core::Failure("testkit.adapt", "a case spec with edge lists needs its graph: adapt::spec(graph, spec, ...)");
  return spec(solve::CaseGraph{}, s, settings, kase);
}

CaseSpec spec(const solve::CaseGraph& graph, const solve::CaseSpec& s, const solve::Settings& settings, solve::Case kase) {
  CaseSpec out;
  for (const auto& o : s.observations) {
    const std::string id = o.node ? o.node->value : o.gene;
    if (o.regulation == expression::Regulation::up) out.up.push_back(id);
    if (o.regulation == expression::Regulation::down) out.down.push_back(id);
  }
  out.confirmed_up = keys_of(s.confirmed_up);
  out.confirmed_down = keys_of(s.confirmed_down);
  out.essential = keys_of(s.essential);
  out.avoid = keys_of(s.avoid);
  out.active = keys_of(s.active);
  out.inactive = keys_of(s.inactive);
  out.nonrelaxed = keys_of(s.non_relaxed);
  out.fsc = keys_of(s.fsc);
  const auto lines = [&](const std::vector<std::uint32_t>& edges) {
    std::vector<EdgeSpec> v;
    for (std::uint32_t j : edges) {
      const auto& e = graph.edges.at(j);
      v.push_back({graph.nodes[e.src].key.value, graph.nodes[e.tgt].key.value, e.key.subtypes});
    }
    return v;
  };
  out.essential_edges = lines(s.essential_edges);
  out.avoid_edges = lines(s.avoid_edges);
  out.nonrelaxed_edges = lines(s.nonrelaxed_edges);
  // S5.03: a given flag decides; without one, the inputs given turn each family on. A negative
  // threshold is off, as FuSION 1 had it.
  const bool allowed = settings.families.enabled.value_or(true);
  const auto& co = settings.families.coexpression_threshold;
  if (allowed && s.coexpression_table && co && *co >= 0)
    for (const auto& pair : s.coexpression)
      if (pair.score > *co) out.coexpressed.emplace_back(pair.a.value, pair.b.value);
  const auto& fr = settings.families.frozen_threshold;
  if (allowed && fr && *fr >= 0) out.frozen_threshold = *fr;
  out.solve_case = case_of(kase);
  out.named = !settings.positional;
  return out;
}

Assignment assignment(const solve::Witness& w) {
  Assignment a;
  for (const network::NodeKey& k : w.active) a.present.emplace_back(k.value, true);
  for (const network::NodeKey& k : w.inactive) a.present.emplace_back(k.value, false);
  for (const network::EdgeKey& k : w.selected) {
    std::vector<std::string> subtypes = k.subtypes;
    std::sort(subtypes.begin(), subtypes.end());
    a.selected.push_back({k.source.value, k.target.value, k.type, subtypes});
  }
  return a;
}

Assignment assignment(const corridor::Corridor& c, const solve::Witness& w) {
  const network::Network& n = c.network();
  NetworkKeys keys(n);
  Assignment a;
  const auto node = [&](const network::NodeKey& k) {
    const auto v = n.find_node(k);
    return v ? keys(*v).key : "unknown:" + k.value;
  };
  for (const network::NodeKey& k : w.active) a.present.emplace_back(node(k), true);
  for (const network::NodeKey& k : w.inactive) a.present.emplace_back(node(k), false);
  for (const network::EdgeKey& k : w.selected) {
    if (const auto j = n.find_edge(k)) {
      a.selected.push_back(edge_of(n, *j, keys).key);
      std::sort(a.selected.back().subtypes.begin(), a.selected.back().subtypes.end());
    } else {
      a.selected.push_back({node(k.source), node(k.target), k.type, k.subtypes});
    }
  }
  return a;
}

}  // namespace fusion::testkit::adapt
