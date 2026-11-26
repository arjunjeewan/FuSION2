// Canonical forms and their diff (canon.hpp).
#include "fusion/testkit/canon.hpp"

#include <algorithm>
#include <map>
#include <sstream>

#include "fusion/core/core.hpp"
#include "util.hpp"

namespace fusion::testkit {

using detail::join;
using detail::sort_unique;

std::string CanonEdgeKey::text() const { return src + "|" + tgt + "|" + type + "|" + join(subtypes, ","); }

namespace {

std::string edge_line(const CanonEdge& e) {
  return "edge " + e.key.text() + "\ttag=" + (e.dummy_tag ? "1" : "0") + "\tpathways=" + join(e.pathways, ",") +
         "\tscore=" + core::format_number(e.score);
}

std::string node_line(const CanonNode& n) { return "node " + n.key + "\t" + n.type + "\t" + join(n.members, " "); }

}  // namespace

std::string CanonGraph::text() const {
  std::string out;
  for (const CanonNode& n : nodes) out += node_line(n) + "\n";
  for (const CanonEdge& e : edges) out += edge_line(e) + "\n";
  return out;
}

std::string CanonGraph::sha256() const { return core::Sha256::of(text()).hex(); }

const CanonNode* CanonGraph::node(const std::string& key) const {
  const auto it = std::lower_bound(nodes.begin(), nodes.end(), key,
                                   [](const CanonNode& n, const std::string& k) { return n.key < k; });
  return it != nodes.end() && it->key == key ? &*it : nullptr;
}

void normalise(CanonGraph& g) {
  for (CanonNode& n : g.nodes) sort_unique(n.members);
  for (CanonEdge& e : g.edges) {
    e.key.subtypes.erase(std::remove(e.key.subtypes.begin(), e.key.subtypes.end(), "dummy_u_to_d"),
                         e.key.subtypes.end());
    std::sort(e.key.subtypes.begin(), e.key.subtypes.end());
    sort_unique(e.pathways);
  }
  sort_unique(g.nodes);
  std::sort(g.edges.begin(), g.edges.end());
}

std::string node_key(std::vector<std::string> members) {
  sort_unique(members);
  return join(members, "+");
}

std::string complex_key(std::vector<std::string> component_keys) {
  sort_unique(component_keys);
  return "{" + join(component_keys, ",") + "}";
}

bool is_complex_key(const std::string& key) { return key.size() >= 2 && key.front() == '{' && key.back() == '}'; }

std::vector<std::string> components_of(const std::string& key) {
  std::vector<std::string> out;
  if (!is_complex_key(key)) return out;
  int depth = 0;
  std::string current;
  for (std::size_t i = 1; i + 1 < key.size(); ++i) {
    const char c = key[i];
    if (c == '{') ++depth;
    if (c == '}') --depth;
    if (c == ',' && depth == 0) {
      out.push_back(current);
      current.clear();
    } else {
      current += c;
    }
  }
  if (!current.empty()) out.push_back(current);
  return out;
}

std::string module_key_of(const std::string& key) {
  if (is_complex_key(key)) {
    std::vector<std::string> parts;
    for (const std::string& c : components_of(key)) parts.push_back(module_key_of(c));
    sort_unique(parts);
    return "c_s_" + join(parts, "_") + "_c_e";
  }
  const std::vector<std::string> members = detail::split(key, '+');
  return *std::min_element(members.begin(), members.end());
}

std::string module_key(const CanonNode& n) {
  if (is_complex_key(n.key)) return module_key_of(n.key);
  if (n.type == "group" || n.members.empty()) return n.key;
  return *std::min_element(n.members.begin(), n.members.end());
}

// ---------------------------------------------------------------- diff

bool GraphDiff::empty() const {
  return only_a_nodes.empty() && only_b_nodes.empty() && changed_nodes.empty() && only_a_edges.empty() &&
         only_b_edges.empty() && changed_edges.empty();
}

std::string GraphDiff::text(std::size_t limit) const {
  std::ostringstream out;
  std::size_t shown = 0;
  const auto line = [&](const std::string& s) {
    if (shown++ < limit) out << s << "\n";
  };
  for (const auto& k : only_a_nodes) line("only in a: node " + k);
  for (const auto& k : only_b_nodes) line("only in b: node " + k);
  for (const auto& [a, b] : changed_nodes) line("node " + a.key + ": " + node_line(a) + " | " + node_line(b));
  for (const auto& k : only_a_edges) line("only in a: edge " + k.text());
  for (const auto& k : only_b_edges) line("only in b: edge " + k.text());
  for (const auto& [a, b] : changed_edges) line("changed: " + edge_line(a) + " | " + edge_line(b));
  if (shown > limit) out << "... " << shown - limit << " more\n";
  return out.str();
}

namespace {

// One sorted merge on keys. Elements with equal keys form runs; within a pair of runs the identical
// elements match first, the rest pair up as changed, and what is left over is only on one side.
template <class T, class Key, class KeyOf, class OnlyA, class OnlyB, class Changed>
void merge_runs(const std::vector<T>& a, const std::vector<T>& b, KeyOf key_of, OnlyA only_a, OnlyB only_b,
                Changed changed) {
  std::size_t i = 0, j = 0;
  while (i < a.size() || j < b.size()) {
    if (j == b.size() || (i < a.size() && key_of(a[i]) < key_of(b[j]))) {
      only_a(a[i++]);
    } else if (i == a.size() || key_of(b[j]) < key_of(a[i])) {
      only_b(b[j++]);
    } else {
      const Key k = key_of(a[i]);
      std::vector<T> ra, rb;
      while (i < a.size() && key_of(a[i]) == k) ra.push_back(a[i++]);
      while (j < b.size() && key_of(b[j]) == k) rb.push_back(b[j++]);
      std::vector<bool> used(rb.size(), false);
      std::vector<const T*> left;
      for (const T& x : ra) {
        bool matched = false;
        for (std::size_t q = 0; q < rb.size() && !matched; ++q)
          if (!used[q] && rb[q] == x) used[q] = matched = true;
        if (!matched) left.push_back(&x);
      }
      std::size_t q = 0;
      for (const T* x : left) {
        while (q < rb.size() && used[q]) ++q;
        if (q < rb.size()) {
          used[q] = true;
          changed(*x, rb[q]);
        } else {
          only_a(*x);
        }
      }
      for (std::size_t r = 0; r < rb.size(); ++r)
        if (!used[r]) only_b(rb[r]);
    }
  }
}

}  // namespace

GraphDiff diff(const CanonGraph& a, const CanonGraph& b) {
  GraphDiff d;
  merge_runs<CanonNode, std::string>(
      a.nodes, b.nodes, [](const CanonNode& n) { return n.key; },
      [&](const CanonNode& n) { d.only_a_nodes.push_back(n.key); },
      [&](const CanonNode& n) { d.only_b_nodes.push_back(n.key); },
      [&](const CanonNode& x, const CanonNode& y) { d.changed_nodes.emplace_back(x, y); });
  merge_runs<CanonEdge, CanonEdgeKey>(
      a.edges, b.edges, [](const CanonEdge& e) { return e.key; },
      [&](const CanonEdge& e) { d.only_a_edges.push_back(e.key); },
      [&](const CanonEdge& e) { d.only_b_edges.push_back(e.key); },
      [&](const CanonEdge& x, const CanonEdge& y) { d.changed_edges.emplace_back(x, y); });
  return d;
}

}  // namespace fusion::testkit
