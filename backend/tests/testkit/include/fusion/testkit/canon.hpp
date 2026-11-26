// fusion/testkit/canon.hpp: the test kit's own data model (section 12, "The test kit"): canonical
// graphs, corridors and points as plain structs, so that oracles and checkers compare module outputs
// without sharing their code. Nodes and edges name each other by content, never by index or by a
// module's key: a node by its members, a complex by its components (XD-16: nodes are matched by
// member sets, never by key).
#pragma once

#include <compare>
#include <string>
#include <utility>
#include <vector>

namespace fusion::testkit {

struct CanonNode {
  std::string key;                      // members joined by '+'; a complex: '{' component keys joined by ',' '}'
  std::vector<std::string> members;     // sorted, unique; a complex: every id of its components, nested ones too
  std::string type;                     // the entry type: "gene", "compound", "group" for a complex, ...
  auto operator<=>(const CanonNode&) const = default;
};

struct CanonEdgeKey {
  std::string src, tgt;                 // CanonNode keys, never indices
  std::string type;
  std::vector<std::string> subtypes;    // sorted, without dummy_u_to_d
  auto operator<=>(const CanonEdgeKey&) const = default;
  std::string text() const;             // "src|tgt|type|a,b"
};

struct CanonEdge {
  CanonEdgeKey key;
  bool dummy_tag = false;
  std::vector<std::string> pathways;    // sorted, unique
  double score = 0;
  auto operator<=>(const CanonEdge&) const = default;
};

struct CanonGraph {
  std::vector<CanonNode> nodes;         // sorted by key (then by the other fields)
  std::vector<CanonEdge> edges;         // sorted by key (then by the other fields); a key may repeat when a
                                        // graph holds parallel edges whose subtypes differ only in order, or a
                                        // FuSION 1 file repeats a relation (S2.12)
  std::string text() const;             // one LF-terminated line per element
  std::string sha256() const;           // of text(), lower-case hex
  const CanonNode* node(const std::string& key) const;   // binary search; null if absent
  bool operator==(const CanonGraph&) const = default;
};

// Sorts every member, subtype and pathway list and both element lists, removing repeated list items
// and repeated identical nodes. Every producer of a CanonGraph calls it last.
void normalise(CanonGraph&);

struct CanonCorridor {
  CanonGraph graph;
  std::vector<std::string> sources, targets;   // CanonNode keys
  int bound = 0;
  bool operator==(const CanonCorridor&) const = default;
};

// The key of a node made of these members, and of a complex made of these components (canonical keys).
std::string node_key(std::vector<std::string> members);
std::string complex_key(std::vector<std::string> component_keys);
bool is_complex_key(const std::string& key);
std::vector<std::string> components_of(const std::string& complex_key);   // its direct components' keys
// The key FuSION 1 and 2.0 give the node: its smallest member in byte order, or for a complex "c_s_" + its
// components' keys, sorted and joined by '_', + "_c_e" (S2.07, S2.09).
std::string module_key(const CanonNode&);
std::string module_key_of(const std::string& canonical_key);   // the same, from a canonical key alone

struct GraphDiff {
  std::vector<std::string> only_a_nodes, only_b_nodes;          // node keys
  std::vector<std::pair<CanonNode, CanonNode>> changed_nodes;   // same key, other fields differ
  std::vector<CanonEdgeKey> only_a_edges, only_b_edges;
  std::vector<std::pair<CanonEdge, CanonEdge>> changed_edges;   // same key, other fields differ
  bool empty() const;
  std::string text(std::size_t limit = 20) const;               // the first differences, for a failing test
};
GraphDiff diff(const CanonGraph& a, const CanonGraph& b);       // sorted merge on keys

struct Point {
  int n = 0, e = 0;
  auto operator<=>(const Point&) const = default;
};
enum class CheckOutcome { sat, unsat, unknown, not_checked };
struct Frontier {
  std::vector<Point> points;            // ascending n
  std::vector<bool> certified;          // one per point
  bool operator==(const Frontier&) const = default;
};

}  // namespace fusion::testkit
