// exception_merge (oracles.hpp): S2.22's second oracle (12.C14), the merge with the exceptions written
// from S2.06 to S2.21 and section 03's union, with its own KGML reading. It shares no code with merge or
// network: it reads each document's entries and relations, splits each reached box into units under E
// (S2.07), unions the units' ids across all documents (S2.17), makes one complex per distinct set of
// component nodes (S2.09), fans each relation out over its entries' nodes (S2.10), merges edges of one
// identity (S2.20) and sets the tag once (S2.21).
#include <algorithm>
#include <map>
#include <set>
#include <tuple>

#include <pugixml.hpp>

#include "fusion/core/core.hpp"
#include "fusion/testkit/oracles.hpp"
#include "util.hpp"

namespace fusion::testkit {

namespace {

using detail::fail;
using detail::sort_unique;

struct Entry {
  std::string type;
  std::vector<std::string> ids;                 // canonical: ':' removed, sorted, unique
  std::vector<std::string> components;          // a group's component entry ids
};
struct Relation {
  std::string entry1, entry2, type;
  std::vector<std::string> subtypes;            // normalised (S2.10), in order
  std::optional<double> score;                  // empty: unscored
};
struct Doc {
  std::string pathway;                          // the root's name
  std::map<std::string, Entry> entries;
  std::vector<Relation> relations;
};

std::string normalise_subtype(std::string_view name) {      // S2.10
  if (name == "indirect effect") return "indirect";
  if (name == "missing interaction") return "missing";
  if (name == "state change") return "state";
  if (name == "binding/association") return "association";
  return std::string(name);
}

Doc parse(const std::string& text, const std::string& where) {
  pugi::xml_document xml;
  const auto parsed = xml.load_buffer(text.data(), text.size());
  if (!parsed) fail(where + ": " + parsed.description());
  const pugi::xml_node root = xml.child("pathway");
  if (!root) fail(where + ": no <pathway> root");
  Doc doc;
  doc.pathway = root.attribute("name").value();
  for (pugi::xml_node e : root.children("entry")) {
    const std::string id = e.attribute("id").value();
    Entry entry;
    entry.type = e.attribute("type").value();
    if (entry.type != "group")
      for (const std::string& raw : detail::words(e.attribute("name").value())) entry.ids.push_back(detail::strip_colons(raw));
    sort_unique(entry.ids);
    for (pugi::xml_node c : e.children("component")) entry.components.push_back(c.attribute("id").value());
    if (!doc.entries.emplace(id, std::move(entry)).second) fail(where + ": entry " + id + " repeats (S2.06)");
  }
  for (pugi::xml_node r : root.children("relation")) {
    Relation rel;
    rel.entry1 = r.attribute("entry1").value();
    rel.entry2 = r.attribute("entry2").value();
    rel.type = r.attribute("type").value();
    for (pugi::xml_node s : r.children("subtype")) rel.subtypes.push_back(normalise_subtype(s.attribute("name").value()));
    const pugi::xml_attribute score = r.attribute("experiment_score");
    if (score) {
      const core::Decimal d = core::parse_decimal(score.value());
      if (d.status == core::DecimalStatus::ok) rel.score = d.value;
    }
    for (const std::string& end : {rel.entry1, rel.entry2})
      if (!doc.entries.contains(end)) fail(where + ": a relation names a missing entry " + end + " (S2.06)");
    doc.relations.push_back(std::move(rel));
  }
  return doc;
}

bool has(const std::vector<std::string>& list, std::string_view name) {
  return std::find(list.begin(), list.end(), name) != list.end();
}

// Union-find over ids, with path halving.
class Union {
 public:
  std::size_t id(const std::string& s) {
    auto [it, fresh] = index_.try_emplace(s, parent_.size());
    if (fresh) parent_.push_back(parent_.size());
    return it->second;
  }
  std::size_t find(std::size_t x) {
    while (parent_[x] != x) x = parent_[x] = parent_[parent_[x]];
    return x;
  }
  void join(std::size_t a, std::size_t b) {
    a = find(a), b = find(b);
    if (a != b) parent_[std::max(a, b)] = std::min(a, b);
  }
  const std::map<std::string, std::size_t>& ids() const { return index_; }

 private:
  std::map<std::string, std::size_t> index_;
  std::vector<std::size_t> parent_;
};

class Merge {
 public:
  Merge(std::vector<Doc> docs, const std::set<std::string>& exceptions, const MergeOracleSettings& settings)
      : docs_(std::move(docs)), e_(exceptions), settings_(settings) {}

  CanonGraph run() {
    // 1. Kept relations (S2.11, S2.31, 02.C7) and the entries they reach, directly or as components.
    std::vector<std::vector<const Relation*>> kept(docs_.size());
    std::vector<std::set<std::string>> reached(docs_.size());
    for (std::size_t d = 0; d < docs_.size(); ++d) {
      for (const Relation& r : docs_[d].relations) {
        if (!settings_.keep_indirect && has(r.subtypes, "indirect")) continue;
        if (!settings_.keep_ecrel && r.type == "ECrel") continue;
        if (r.type == "maplink") continue;
        kept[d].push_back(&r);
        reach(d, r.entry1, reached[d], 0);
        reach(d, r.entry2, reached[d], 0);
      }
    }
    // 2. Units of each reached box under E (S2.07), the type table (S2.19), 3. the union (S2.17).
    std::map<std::string, std::string> type_of;
    for (std::size_t d = 0; d < docs_.size(); ++d)
      for (const std::string& id : reached[d]) {
        const Entry& entry = docs_[d].entries.at(id);
        if (entry.type == "group") continue;
        for (const std::string& gene : entry.ids) {
          const auto [it, fresh] = type_of.emplace(gene, entry.type);
          if (!fresh && it->second != entry.type)
            fail("node-type conflict: " + gene + " is a " + it->second + " and a " + entry.type + " (S2.19)");
          ids_.id(gene);
        }
        for (const auto& unit : units(entry))
          for (std::size_t k = 1; k < unit.size(); ++k) ids_.join(ids_.id(unit[0]), ids_.id(unit[k]));
      }
    // 4. Nodes: one per component of ids, key its smallest member.
    std::map<std::size_t, std::size_t> node_of_root;
    for (const auto& [gene, x] : ids_.ids()) {
      const std::size_t root = ids_.find(x);
      auto [it, fresh] = node_of_root.try_emplace(root, nodes_.size());
      if (fresh) nodes_.push_back({});
      nodes_[it->second].members.push_back(gene);
      nodes_[it->second].type = type_of.at(gene);
    }
    for (Node& n : nodes_) {
      sort_unique(n.members);
      n.key = n.members.front();
    }
    for (std::size_t v = 0; v < nodes_.size(); ++v)
      for (const std::string& gene : nodes_[v].members) node_by_id_[gene] = v;
    // 5. Complexes and 6. edges, document by document; the builder's identity merge is a map here.
    for (std::size_t d = 0; d < docs_.size(); ++d)
      for (const Relation* r : kept[d]) {
        const std::vector<std::size_t> from = nodes_of(d, r->entry1), to = nodes_of(d, r->entry2);
        const bool undirected = has(r->subtypes, "association") || has(r->subtypes, "dissociation") ||
                                has(r->subtypes, "mapping");
        for (std::size_t s : from)
          for (std::size_t t : to) {
            add_edge(s, t, r->type, r->subtypes, docs_[d].pathway, r->score, undirected, false);
            if (undirected && s != t) add_edge(t, s, r->type, r->subtypes, docs_[d].pathway, r->score, true, false);
          }
      }
    for (std::size_t c = 0; c < nodes_.size(); ++c)   // two-way component edges, with each making group's pathway
      for (const std::string& pathway : nodes_[c].made_by)
        for (std::size_t part : nodes_[c].components) {
          add_edge(part, c, "PPrel", {"component"}, pathway, std::nullopt, false, true);
          add_edge(c, part, "PPrel", {"component"}, pathway, std::nullopt, false, true);
        }
    return canonical();
  }

 private:
  struct Node {
    std::string key;                             // the module key (S2.07, S2.09)
    std::vector<std::string> members;
    std::string type;
    bool complex = false;
    std::vector<std::size_t> components;         // distinct component nodes, sorted
    std::set<std::string> made_by;               // pathways of the groups that made it
  };
  struct Edge {
    std::set<std::string> pathways;
    std::optional<double> score;
    bool undirected = false, component = false;
  };
  using Identity = std::tuple<std::size_t, std::size_t, std::string, std::vector<std::string>>;

  void reach(std::size_t d, const std::string& id, std::set<std::string>& out, int depth) {
    if (depth > 64) fail(docs_[d].pathway + ": a group contains itself (S2.06)");
    const auto it = docs_[d].entries.find(id);
    if (it == docs_[d].entries.end()) fail(docs_[d].pathway + ": a component names a missing entry " + id + " (S2.06)");
    if (!out.insert(id).second) return;
    for (const std::string& c : it->second.components) reach(d, c, out, depth + 1);
  }

  // S2.07: when a box holds more ids than the threshold, each exception id is a unit alone and the rest
  // form one unit; otherwise every id is a unit alone. A negative threshold never collapses; 0 acts as 1.
  std::vector<std::vector<std::string>> units(const Entry& box) const {
    std::vector<std::vector<std::string>> out;
    const bool collapse = settings_.threshold >= 0 && static_cast<long long>(box.ids.size()) > settings_.threshold;
    std::vector<std::string> rest;
    for (const std::string& id : box.ids) {
      if (!collapse || e_.contains(id)) out.push_back({id});
      else rest.push_back(id);
    }
    if (!rest.empty()) out.push_back(rest);
    return out;
  }

  // The entry's node set: a box's units' nodes, a group's complexes (Cartesian product, S2.09).
  std::vector<std::size_t> nodes_of(std::size_t d, const std::string& id) {
    const auto memo = memo_.find({d, id});
    if (memo != memo_.end()) return memo->second;
    const Entry& entry = docs_[d].entries.at(id);
    std::vector<std::size_t> out;
    if (entry.type != "group") {
      for (const std::string& gene : entry.ids) out.push_back(node_by_id_.at(gene));
      sort_unique(out);
    } else {
      std::vector<std::vector<std::size_t>> combos{{}};
      for (const std::string& c : entry.components) {
        const std::vector<std::size_t> part = nodes_of(d, c);
        std::vector<std::vector<std::size_t>> next;
        for (const auto& combo : combos)
          for (std::size_t n : part) {
            auto x = combo;
            x.push_back(n);
            next.push_back(std::move(x));
          }
        combos = std::move(next);
      }
      for (auto combo : combos) {
        sort_unique(combo);
        if (combo.empty()) continue;
        auto [it, fresh] = complex_by_parts_.try_emplace(combo, nodes_.size());
        if (fresh) {
          Node n;
          n.complex = true;
          n.type = "group";
          n.components = combo;
          std::vector<std::string> keys;
          for (std::size_t k : combo) keys.push_back(nodes_[k].key);
          sort_unique(keys);
          n.key = "c_s_" + detail::join(keys, "_") + "_c_e";
          nodes_.push_back(std::move(n));
        }
        nodes_[it->second].made_by.insert(docs_[d].pathway);
        out.push_back(it->second);
      }
      sort_unique(out);
    }
    memo_[{d, id}] = out;
    return out;
  }

  void add_edge(std::size_t s, std::size_t t, const std::string& type, std::vector<std::string> subtypes,
                const std::string& pathway, std::optional<double> score, bool undirected, bool component) {
    subtypes.erase(std::remove(subtypes.begin(), subtypes.end(), "dummy_u_to_d"), subtypes.end());
    Edge& e = edges_[Identity{s, t, type, subtypes}];   // identity: ends, type, ordered subtypes (S2.04)
    e.pathways.insert(pathway);
    if (score) e.score = e.score ? std::max(*e.score, *score) : *score;   // the larger score (S2.10, S2.20)
    e.undirected = e.undirected || undirected;
    e.component = e.component || component;
  }

  CanonGraph canonical() {
    std::vector<std::string> keys(nodes_.size());
    std::vector<std::vector<std::string>> members(nodes_.size());
    // Components are made before their complexes, so one pass in creation order suffices.
    for (std::size_t v = 0; v < nodes_.size(); ++v) {
      if (!nodes_[v].complex) {
        keys[v] = node_key(nodes_[v].members);
        members[v] = nodes_[v].members;
        continue;
      }
      std::vector<std::string> parts, ids;
      for (std::size_t c : nodes_[v].components) {
        parts.push_back(keys[c]);
        ids.insert(ids.end(), members[c].begin(), members[c].end());
      }
      keys[v] = complex_key(parts);
      sort_unique(ids);
      members[v] = ids;
    }
    CanonGraph g;
    for (std::size_t v = 0; v < nodes_.size(); ++v) g.nodes.push_back({keys[v], members[v], nodes_[v].type});
    for (const auto& [id, e] : edges_) {
      const auto& [s, t, type, subtypes] = id;
      CanonEdge c;
      c.key = {keys[s], keys[t], type, subtypes};
      // The tag, once (S2.21): every component edge, and an undirected edge whose source key is larger.
      c.dummy_tag = e.component || (e.undirected && s != t && nodes_[t].key < nodes_[s].key);
      c.pathways.assign(e.pathways.begin(), e.pathways.end());
      c.score = e.score.value_or(0);
      g.edges.push_back(std::move(c));
    }
    normalise(g);
    return g;
  }

  std::vector<Doc> docs_;
  const std::set<std::string>& e_;
  MergeOracleSettings settings_;
  Union ids_;
  std::vector<Node> nodes_;
  std::map<std::string, std::size_t> node_by_id_;
  std::map<std::vector<std::size_t>, std::size_t> complex_by_parts_;
  std::map<std::pair<std::size_t, std::string>, std::vector<std::size_t>> memo_;
  std::map<Identity, Edge> edges_;
};

}  // namespace

CanonGraph exception_merge_texts(std::span<const std::string> texts, const std::set<std::string>& exceptions,
                                 const MergeOracleSettings& settings) {
  std::vector<Doc> docs;
  for (std::size_t k = 0; k < texts.size(); ++k) docs.push_back(parse(texts[k], "document " + std::to_string(k + 1)));
  return Merge(std::move(docs), exceptions, settings).run();
}

CanonGraph exception_merge(std::span<const std::filesystem::path> kgmls, const std::set<std::string>& exceptions,
                           const MergeOracleSettings& settings) {
  std::vector<Doc> docs;
  for (const auto& path : kgmls) docs.push_back(parse(detail::slurp(path), core::to_utf8(path)));
  return Merge(std::move(docs), exceptions, settings).run();
}

}  // namespace fusion::testkit
