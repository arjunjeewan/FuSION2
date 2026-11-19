// build_case's first half (06, "Building a case", steps 1 and 3): the corridor read once into a
// CaseGraph, and the lists, the FSC, the observations and the coexpression rows bound to node keys
// once per solve, with a bindings.tsv row for each list id and edge line (1.22).
#include "fusion/solve/solve.hpp"

#include <algorithm>
#include <map>
#include <set>

#include "fusion/corridor/corridor.hpp"
#include "internal.hpp"

namespace fusion::solve {

namespace {

using network::NodeIdx;

// A complex's components, closed over nested complexes (entry 170). Legacy networks may lack the
// component table; the complex key then names them (c_s_<keys joined by _>_c_e, S2.09).
std::vector<network::NodeKey> components_of(const network::Network& net, NodeIdx complex) {
  std::vector<network::NodeKey> out;
  std::set<NodeIdx> seen{complex};
  std::vector<NodeIdx> stack(net.components(complex).begin(), net.components(complex).end());
  while (!stack.empty()) {
    const NodeIdx v = stack.back();
    stack.pop_back();
    if (!seen.insert(v).second) continue;
    out.push_back(net.node_key(v));
    for (NodeIdx w : net.components(v)) stack.push_back(w);
  }
  if (out.empty()) {
    const std::string key = net.node_key(complex).value;
    if (key.size() > 8 && key.starts_with("c_s_") && key.ends_with("_c_e")) {
      const std::string inner = key.substr(4, key.size() - 8);
      std::size_t start = 0;
      while (start <= inner.size()) {
        const std::size_t end = std::min(inner.find('_', start), inner.size());
        if (end > start) out.push_back({inner.substr(start, end - start)});
        start = end + 1;
      }
    }
  }
  std::sort(out.begin(), out.end());
  return out;
}

// The id rule FuSION 1's readers used: the node keyed by the id, else the node holding it, with
// ':' removed (GraphManagerNew.cc:18624-18650, 18739-18819, 28645-28658). No bindings.tsv row.
std::optional<network::NodeKey> lookup(const network::Network& net, std::string_view raw) {
  std::string id(raw);
  id.erase(std::remove(id.begin(), id.end(), ':'), id.end());
  if (auto v = net.find_node(network::NodeKey{id})) return net.node_key(*v);
  if (auto gene = net.interner().find_id(id)) {
    const auto holders = net.ids().holders(*gene);
    if (!holders.empty()) return net.node_key(holders.front());
  }
  return std::nullopt;
}

std::string line_text(const EdgeLine& line) {
  std::string text = line.source + " " + line.target + " ";
  if (line.subtypes.empty()) return text + "-";
  for (std::size_t k = 0; k < line.subtypes.size(); ++k) text += (k ? "_" : "") + line.subtypes[k];
  return text;
}

}  // namespace

CaseGraph read_corridor(const corridor::Corridor& corridor, const network::Maps* maps) {
  CaseGraph graph;
  const network::GraphView& view = corridor.view();
  const network::Network& net = corridor.network();
  std::vector<std::uint32_t> index(net.nodes().size(), 0);
  view.for_each_node([&](NodeIdx v) {
    index[v] = static_cast<std::uint32_t>(graph.nodes.size());
    CaseGraph::Node node;
    node.key = net.node_key(v);
    node.label = maps ? net.label(v, *maps) : node.key.value;
    if (net.kind(v) == network::NodeKind::complex) node.contains = components_of(net, v);
    graph.nodes.push_back(std::move(node));
  });
  view.for_each_edge([&](network::EdgeIdx j) {
    const network::Edge& e = net.edges()[j];
    if (!view.has_node(e.src) || !view.has_node(e.tgt)) return;
    graph.edges.push_back({index[e.src], index[e.tgt], net.edge_key(j), net.effect(j), e.score,
                           (e.flags & network::bit(network::EdgeFlag::unscored)) != 0});
  });
  graph.source = index[corridor.source()];
  graph.target = index[corridor.target()];
  graph.bound = corridor.bound();
  // 02's one polarity rule (S6.14), tabulated for the encoder.
  using network::EndState;
  for (network::Effect effect : {network::Effect::activation, network::Effect::inhibition})
    for (EndState from : {EndState::absent, EndState::inactive, EndState::active})
      for (EndState to : {EndState::absent, EndState::inactive, EndState::active})
        if (network::is_relaxed(effect, from, to))
          graph.relaxed_states[static_cast<std::size_t>(effect)] |=
              static_cast<std::uint16_t>(1U << (3 * static_cast<unsigned>(from) + static_cast<unsigned>(to)));
  return graph;
}

CaseSpec bind_inputs(const Inputs& inputs, const CaseGraph& graph, const Settings& settings, core::Context& ctx) {
  CaseSpec spec;
  const network::Network& net = inputs.corridor.network();

  // S5.04, 05.C7: a positional line's connect pair must be the corridor's source and target.
  if (inputs.connect_pair) {
    const auto source = lookup(net, inputs.connect_pair->first), target = lookup(net, inputs.connect_pair->second);
    if (!source || !target || *source != graph.nodes[graph.source].key || *target != graph.nodes[graph.target].key)
      throw core::Failure("solve.bound_mismatch", "the connect pair " + inputs.connect_pair->first + " " +
                                                      inputs.connect_pair->second + " differs from the corridor's " +
                                                      graph.nodes[graph.source].key.value + " -> " +
                                                      graph.nodes[graph.target].key.value + " (05.C7)");
  }

  // S5.05: each id binds through the run's table, which writes its bindings.tsv row (1.22). An id
  // that binds to nothing is dropped, as FuSION 1 dropped it (GraphManagerNew.cc:18688-18708), but
  // with a warning.
  const auto bind_list = [&](std::string_view role, const std::vector<std::string>& ids,
                             std::vector<CaseSpec::Id>& out) {
    for (const std::string& id : ids) {
      const network::Binding& b = inputs.bindings.bind(role, id);
      if (b.status == network::BindStatus::absent) {
        ctx.log.warning("solve.list_id_unresolved", std::string(role) + ": " + id + " binds to no node; dropped (S5.05)");
        continue;
      }
      out.push_back({id, net.node_key(b.node)});
    }
  };
  for (const std::string& id : settings.fsc) {             // S5.17, S8.12: an FSC must bind
    const network::Binding& b = inputs.bindings.bind("fsc", id);
    if (b.status == network::BindStatus::absent)
      throw core::Failure("solve.fsc_unresolved", "the FSC " + id + " binds to no node (S5.17)",
                          detail::where({}, 0, "fsc"));
    spec.fsc.push_back({id, net.node_key(b.node)});
  }
  bind_list("essential_nodes", inputs.nodes.essential, spec.essential);
  bind_list("avoid_nodes", inputs.nodes.avoid, spec.avoid);
  bind_list("active_nodes", inputs.nodes.active, spec.active);
  bind_list("inactive_nodes", inputs.nodes.inactive, spec.inactive);
  bind_list("nonrelaxed_nodes", inputs.nodes.non_relaxed, spec.non_relaxed);
  bind_list("confirmed_up", inputs.nodes.confirmed_up, spec.confirmed_up);
  bind_list("confirmed_down", inputs.nodes.confirmed_down, spec.confirmed_down);

  // The run's observed genes as 05.C9 placed them, up then down.
  std::set<std::string> observed_nodes;
  const core::Interner& observed_ids = inputs.interner ? *inputs.interner : net.interner();
  for (const expression::ObservationMatch& m : inputs.match) {
    CaseSpec::Observation o;
    o.gene = std::string(observed_ids.str(m.id));
    o.symbol = inputs.maps ? std::string(inputs.maps->symbols.label_or_id(o.gene)) : o.gene;
    o.regulation = m.regulation;
    o.group = m.group;
    o.node = m.node;
    if (m.node) observed_nodes.insert(m.node->value);
    spec.observations.push_back(std::move(o));
  }
  for (const CaseSpec::Id& id : spec.non_relaxed) {
    const bool endpoint = id.node == graph.nodes[graph.source].key || id.node == graph.nodes[graph.target].key;
    if (endpoint || observed_nodes.contains(id.node.value)) continue;
    const auto confirmed = [&](const std::vector<CaseSpec::Id>& list) {
      return std::any_of(list.begin(), list.end(), [&](const CaseSpec::Id& c) { return c.node == id.node; });
    };
    if (!confirmed(spec.confirmed_up) && !confirmed(spec.confirmed_down))
      ctx.log.warning("solve.nonrelaxed_not_observed",
                      "nonrelaxed_nodes: " + id.given + " is not an observation, so the list holds nothing (S5.05)");
  }

  // S5.06: each edge line matches corridor edges by its ends and its subtype set; one row each.
  const auto bind_edges = [&](std::string_view role, const std::vector<EdgeLine>& lines,
                              std::vector<std::uint32_t>& out) {
    for (const EdgeLine& line : lines) {
      std::vector<std::string> subtypes;
      for (const std::string& s : line.subtypes) subtypes.emplace_back(network::normalise_subtype(s));
      const auto source = lookup(net, line.source), target = lookup(net, line.target);
      std::vector<std::uint32_t> matched;
      if (source && target) matched = match_edge_line(graph, *source, *target, subtypes);
      network::BindingRow row{std::string(role), line_text(line), std::nullopt,
                              static_cast<std::uint32_t>(std::max(line.line, 0)), {}};
      if (matched.empty()) {
        row.note = "matches no corridor edge";
        ctx.log.warning("solve.edge_line_unmatched",
                        std::string(role) + " line " + std::to_string(line.line) + " (" + row.given +
                            ") matches no corridor edge (S5.06)");
      } else if (matched.size() > 1) {
        row.note = "matches " + std::to_string(matched.size()) + " edges:";
        for (std::uint32_t j : matched) row.note += " " + graph.edges[j].key.type;
      }
      inputs.bindings.add_row(std::move(row));
      out.insert(out.end(), matched.begin(), matched.end());
    }
  };
  bind_edges("essential_edges", inputs.edges.essential, spec.essential_edges);
  bind_edges("avoid_edges", inputs.edges.avoid, spec.avoid_edges);
  bind_edges("nonrelaxed_edges", inputs.edges.non_relaxed, spec.nonrelaxed_edges);

  // S5.15: every cross pair of a row's ids, each id resolved by the id rule. The threshold and N
  // are applied per case.
  if (inputs.coexpression) {
    spec.coexpression_table = true;
    const auto& threshold = settings.families.coexpression_threshold;
    std::map<std::string, std::optional<network::NodeKey>, std::less<>> resolved;
    const auto resolve = [&](const std::string& id) -> const std::optional<network::NodeKey>& {
      auto it = resolved.find(id);
      if (it == resolved.end()) it = resolved.emplace(id, lookup(net, id)).first;
      return it->second;
    };
    for (const CoexpressionTable::Row& row : inputs.coexpression->rows()) {
      if (threshold && !(row.score > *threshold)) continue;
      for (const std::string& a : row.ids1)
        for (const std::string& b : row.ids2) {
          const auto& u = resolve(a);
          const auto& v = resolve(b);
          if (u && v && *u != *v) spec.coexpression.push_back({*u, *v, row.score});
        }
    }
  }
  return spec;
}

std::variant<Problem, Skipped> build_case(const Inputs& inputs, const Settings& settings, Case kase,
                                          core::Context& ctx) {
  const CaseGraph graph = read_corridor(inputs.corridor, inputs.maps);
  const CaseSpec spec = bind_inputs(inputs, graph, settings, ctx);
  std::variant<Problem, Skipped> built = make_case(graph, spec, settings, kase);
  const Problem& problem = std::holds_alternative<Problem>(built) ? std::get<Problem>(built)
                                                                  : *std::get<Skipped>(built).problem;
  for (const auto& [code, text] : problem.warnings) ctx.log.warning(code, text);
  return built;
}

}  // namespace fusion::solve
