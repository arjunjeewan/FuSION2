// The checker's case on one corridor as plain indices (check.cpp builds it), shared by check() and
// exhaustive_frontier(). Internal to the kit.
#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "fusion/testkit/check.hpp"

namespace fusion::testkit::detail {

// An id's node in a graph: by canonical key, by module key (S2.07, S2.09), or as a member of a node that
// is not a complex (S2.03's family binding). The first node in key order wins.
class Resolver {
 public:
  explicit Resolver(const CanonGraph&);
  std::optional<std::uint32_t> operator()(const std::string& id) const;
  std::vector<std::uint32_t> all(const std::vector<std::string>& ids) const;   // sorted, unique

 private:
  std::map<std::string, std::uint32_t> by_key_, by_module_key_, by_member_;
};
bool is_complex(const CanonNode&);
// Whether complex c contains the node (or, when node is null, the gene id), nested complexes included.
bool contains(const CanonNode& c, const CanonNode* node, const std::string& id);

// Node states: 0 absent, 1 present and inactive, 2 present and active.
struct Model {
  const CanonGraph* graph = nullptr;
  std::size_t nodes = 0, edges = 0;
  std::vector<std::uint32_t> src, tgt;           // per edge of the graph
  std::vector<bool> inhibition;                  // per edge: S5.13's family
  std::vector<bool> source, target;              // per node
  std::vector<bool> in_n, in_e;                  // the encoded sets (S5.10)
  std::vector<std::uint32_t> encoded_nodes, encoded_edges;
  std::vector<std::vector<std::uint32_t>> out_edges, in_edges;   // over the encoded edges
  int bound = 0;
  std::vector<std::uint8_t> regulation;          // per node: 0 none, 1 up, 2 down (the case's sets)
  std::vector<std::uint32_t> counted;            // S5.12
  struct Need { std::uint32_t index; std::string why; };
  std::vector<Need> present, absent, active, inactive, consistent;      // nodes
  std::vector<Need> edge_selected, edge_avoided, edge_nonrelaxed, frozen;   // encoded edges
  std::vector<std::pair<std::uint32_t, std::uint32_t>> coexpressed;      // encoded nodes, u < v
  std::vector<std::string> problems;             // each makes every assignment invalid
};

Model build_model(const CanonCorridor&, const CaseSpec&, Semantics);

bool relaxed_edge(const Model&, std::uint32_t edge, const std::vector<std::uint8_t>& state);
bool relaxed_node(const Model&, std::uint32_t node, const std::vector<std::uint8_t>& state);
Point cost_of(const Model&, const std::vector<std::uint8_t>& state);   // independent of the selection (S5.13)

// The rules on the node states alone: encoded, present, absent, active, inactive, consistent and
// coexpressed nodes, and the non-relaxed and frozen edges, whose relaxation the states fix. Stops at the
// first violation when `why` is null.
bool states_ok(const Model&, const std::vector<std::uint8_t>& state, std::vector<std::string>* why);
// The rules on the selection: S5.11's paths, S5.14's DE rule and S5.06's essential and avoided edges.
bool selection_ok(const Model&, const std::vector<std::uint8_t>& state, const std::vector<bool>& selected,
                  std::vector<std::string>* why);

}  // namespace fusion::testkit::detail
