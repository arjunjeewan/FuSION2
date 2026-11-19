// fusion network: sz's counts (S2.28) and the labels query (2B.6, 10.C15, 3A.8).
#include "detail.hpp"

namespace fusion::network {

// FuSION 1's sz (Linux/src/main.cc:1581-1643): nodes by type (gene, group, compound, any other),
// isolated nodes (no edge in or out, GraphManagerNew.cc:4266-4277) and edges; S2.28 adds the gene
// nodes holding two or more ids and the largest of them.
Summary summarize(const GraphView& view) {
  Summary s;
  const Network& net = view.network();
  view.for_each_node([&](NodeIdx n) {
    ++s.nodes;
    const NodeKind kind = net.kind(n);
    switch (kind) {
      case NodeKind::gene: ++s.genes; break;
      case NodeKind::complex: ++s.complexes; break;
      case NodeKind::compound: ++s.compounds; break;
      case NodeKind::other: ++s.others; break;
    }
    bool isolated = true;
    view.for_each_out_edge(n, [&](EdgeIdx) { isolated = false; });
    view.for_each_in_edge(n, [&](EdgeIdx) { isolated = false; });
    if (isolated) ++s.isolated;
    const auto size = static_cast<std::uint32_t>(net.members(n).size());
    if (kind == NodeKind::gene && size >= 2) {
      ++s.multi_gene;
      if (size > s.largest_size) {                           // ties: the first in key order
        s.largest = n;
        s.largest_size = size;
      }
    }
  });
  view.for_each_edge([&](EdgeIdx e) {
    const Edge& edge = net.edges()[e];
    if (view.has_node(edge.src) && view.has_node(edge.tgt)) ++s.edges;
  });
  return s;
}

std::vector<std::string> summary_lines(const Summary& s, const GraphView& view, const Maps& maps) {
  (void)maps;                                                // the largest node is named by its key
  std::vector<std::string> out;
  out.push_back("Number of nodes: " + std::to_string(s.nodes) + " (" + std::to_string(s.genes) + " genes, " +
                std::to_string(s.complexes) + " complexes, " + std::to_string(s.compounds) + " compounds, " +
                std::to_string(s.others) + " others) " + std::to_string(s.isolated) + " isolated");
  out.push_back("Number of edges: " + std::to_string(s.edges));
  if (s.multi_gene == 0) {
    out.push_back("Multi-gene nodes: 0");
  } else {
    out.push_back("Multi-gene nodes: " + std::to_string(s.multi_gene) + "; largest " +
                  view.network().node_key(s.largest).value + " (" + std::to_string(s.largest_size) + " genes)");
  }
  return out;
}

// The labels query (2B.6): the map's id-label pairs in byte order of id, with the map's path and
// SHA-256, read with the same reader as every symbol map. Writes no file.
core::Json labels_answer(const LabelMap& symbols, const SourceRef& map_file) {
  core::Json labels = core::Json::array();
  for (const auto& row : symbols.rows())
    labels.push_back(core::Json::array({std::string(row.id), std::string(row.name.empty() ? row.id : row.name)}));
  const core::Sha256 sha = map_file.sha256 == core::Sha256{} ? symbols.digest() : map_file.sha256;
  core::Json map{{"path", map_file.path ? core::Json(core::to_utf8(*map_file.path)) : core::Json(nullptr)},
                 {"sha256", sha.hex()}};
  return core::Json{{"map", std::move(map)}, {"labels", std::move(labels)}};
}

}  // namespace fusion::network
