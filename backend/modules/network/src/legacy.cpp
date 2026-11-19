// fusion network: the legacy reader (02.C5; S2.27, S2.12, S2.08, S2.03, S4.13). Every unmarked file
// FuSION 1 wrote is read so as to reproduce what FuSION 1's later stages saw, its graph 2: read 1 by
// FuSION 1's tool-written rules, then the replay of wgx -> rgx y in memory, then the merge of exact
// duplicate edges, so edge identities stay unique.
#include <algorithm>
#include <map>
#include <set>

#include "detail.hpp"

namespace fusion::network {

namespace {

// A document as FuSION 1's reader sees it: entries by file id, relations in file order.
struct LDoc {
  struct Entry { std::vector<std::string> ids; std::string type; std::vector<std::uint32_t> comps; bool group = false; };
  struct Rel {
    std::uint32_t e1 = 0, e2 = 0;
    std::string type;
    std::vector<std::string> subtypes, pathways;
    std::optional<double> score;
  };
  std::map<std::uint32_t, Entry> entries;
  std::vector<Rel> relations;
};

// FuSION 1's graph, in its own creation order.
struct LGraph {
  struct Node {
    std::string key;                                         // rep_id
    std::vector<std::string> all_id;                         // in FuSION 1's order
    std::string type;
    std::vector<int> comps;                                  // node ids, ascending
    bool group = false, exception = false;
  };
  std::vector<Node> nodes;                                   // node id = position
  std::vector<LDoc::Rel> edges;                              // e1, e2 hold node ids
  std::map<std::string, std::string> id_to_rep;              // insert-only: the first claim wins
  std::map<std::string, int> rep_to_nid;                     // insert-only
  std::map<std::vector<int>, int> group_by_comps;
};

// One read by FuSION 1's tool-written rules: XMLParser.cc:621-722 (relations in file order, entry1's
// nodes, entry2's, then the fan-out in node-creation order), 875-1128 (an entry's nodes) and
// 1451-1545 (an edge: no duplicate test, no reverse edge, no tag, pathways from the attribute).
class Reader {
 public:
  Reader(const IdSet& exceptions, int threshold, RelationFilters filters)
      : exceptions_(exceptions), threshold_(threshold), filters_(filters) {}

  LGraph read(const LDoc& doc) {
    g_ = LGraph();
    for (const auto& r : doc.relations) {
      const std::set<int> sources = process(doc, r.e1);
      const std::set<int> targets = process(doc, r.e2);
      if (filtered(r)) continue;                             // after the nodes, as XMLParser.cc:1487
      for (int s : sources)                                  // std::set<int>: creation order (700-701)
        for (int t : targets) {
          LDoc::Rel e = r;
          e.e1 = static_cast<std::uint32_t>(s);
          e.e2 = static_cast<std::uint32_t>(t);
          g_.edges.push_back(std::move(e));
        }
    }
    return std::move(g_);
  }

 private:
  bool filtered(const LDoc::Rel& r) const {
    if (filters_.indirect == IndirectMode::discard &&
        std::find(r.subtypes.begin(), r.subtypes.end(), "indirect") != r.subtypes.end())
      return true;
    if (filters_.ecrel == EcrelMode::exclude && r.type == "ECrel") return true;
    if (filters_.maplink == MaplinkMode::skip && r.type == "maplink") return true;
    return false;
  }
  bool exception(const std::string& id) const { return exceptions_.contains(id); }
  void claim(const std::string& id, const std::string& rep) { g_.id_to_rep.emplace(id, rep); }   // add_id_rep_id
  int create(std::string key) {
    LGraph::Node node;
    node.key = std::move(key);
    g_.nodes.push_back(std::move(node));
    const int nid = static_cast<int>(g_.nodes.size()) - 1;
    g_.rep_to_nid.emplace(g_.nodes.back().key, nid);           // add_rep_id_for_node, insert-only
    return nid;
  }

  // XMLParser.cc:42-62, kept exactly, an empty first set included.
  static void cartesian(std::vector<std::set<int>>& combos, const std::set<int>& members) {
    if (combos.empty()) {
      for (int m : members) combos.push_back({m});
      return;
    }
    const std::vector<std::set<int>> before = std::move(combos);
    combos.clear();
    for (int m : members)
      for (const auto& c : before) {
        std::set<int> next = c;
        next.insert(m);
        combos.push_back(std::move(next));
      }
  }

  std::set<int> process(const LDoc& doc, std::uint32_t local) {
    const LDoc::Entry& entry = doc.entries.at(local);
    std::set<int> nodes;
    if (entry.group) {                                       // XMLParser.cc:1078-1125
      std::vector<std::set<int>> combos;
      for (std::uint32_t c : entry.comps) cartesian(combos, process(doc, c));
      for (const auto& combo : combos) {
        std::vector<int> comps(combo.begin(), combo.end());
        const auto found = g_.group_by_comps.find(comps);    // GraphManagerNew.cc:2421-2436
        if (found != g_.group_by_comps.end()) {
          nodes.insert(found->second);
          continue;
        }
        std::set<std::string> keys;                          // GraphManagerNew.cc:2438-2466
        for (int c : combo) keys.insert(g_.nodes[static_cast<std::size_t>(c)].key);
        std::string id = "c_s_";
        for (auto it = keys.begin(); it != keys.end(); ++it) id += (it == keys.begin() ? "" : "_") + *it;
        id += "_c_e";
        const int nid = create(id);
        auto& node = g_.nodes[static_cast<std::size_t>(nid)];
        node.all_id.push_back(id);
        node.type = entry.type;
        node.comps = comps;
        node.group = true;
        claim(id, id);
        g_.group_by_comps.emplace(std::move(comps), nid);
        nodes.insert(nid);
      }
      return nodes;
    }
    // XMLParser.cc:875-1077. all_ids is the sorted set of the entry's ids; the comparison with the
    // threshold is unsigned, so a negative threshold never collapses (899-900).
    const auto& all_ids = entry.ids;
    const bool split = threshold_ < 0 || all_ids.size() <= static_cast<std::size_t>(threshold_);
    std::vector<std::string> kept;                           // all_ids_vec: the ids outside E (922-932)
    for (const auto& id : all_ids)
      if (!exception(id)) kept.push_back(id);
    const std::string box = (!split && !kept.empty()) ? kept.front() : std::string();   // 934-940
    std::set<std::string> reps;
    for (const auto& id : all_ids) {                         // 954-987; all_id_map keys dropped (S2.14)
      if (exception(id)) {
        reps.insert(id);
        continue;
      }
      if (!split) {
        reps.insert(box);
        claim(id, box);
      } else {
        reps.insert(id);
        claim(id, id);
      }
    }
    for (const auto& rid : reps) {                           // 991-1076
      const auto at = g_.rep_to_nid.find(rid);
      if (at != g_.rep_to_nid.end()) {                       // an existing node gains no ids: S2.08's
        nodes.insert(at->second);                            // loss, which legacy reads keep (entry 163)
        continue;
      }
      const int nid = create(rid);
      auto& node = g_.nodes[static_cast<std::size_t>(nid)];
      node.type = entry.type;
      if (exception(rid)) {                                  // 1000-1008
        claim(rid, rid);
        node.all_id.push_back(rid);
        node.exception = true;
        nodes.insert(nid);
        continue;
      }
      for (const auto& id : kept) {                          // 1012-1045
        if (!split) {
          claim(id, box);
          claim(box, box);
          node.all_id.push_back(id);
          if (std::find(node.all_id.begin(), node.all_id.end(), box) == node.all_id.end()) node.all_id.push_back(box);
        } else {
          claim(id, rid);
          if (std::find(node.all_id.begin(), node.all_id.end(), rid) == node.all_id.end()) node.all_id.push_back(rid);
        }
      }
      nodes.insert(nid);
    }
    return nodes;
  }

  const IdSet& exceptions_;
  int threshold_;
  RelationFilters filters_;
  LGraph g_;
};

// What FuSION 1's wgx writes and its rgx y reads back (GraphManagerNew.cc:7895-7938, 2892-2953):
// nodes numbered from 1 in creation order, edges in creation order.
LDoc written(const LGraph& g) {
  LDoc doc;
  for (std::size_t n = 0; n < g.nodes.size(); ++n) {
    const auto& node = g.nodes[n];
    LDoc::Entry e;
    e.ids = node.all_id;
    std::sort(e.ids.begin(), e.ids.end());                   // the reader splits name into a set
    e.ids.erase(std::unique(e.ids.begin(), e.ids.end()), e.ids.end());
    e.type = node.type;
    e.group = node.group;
    for (int c : node.comps) e.comps.push_back(static_cast<std::uint32_t>(c) + 1);
    doc.entries.emplace(static_cast<std::uint32_t>(n) + 1, std::move(e));
  }
  for (const auto& edge : g.edges) {
    LDoc::Rel r = edge;
    r.e1 = edge.e1 + 1;
    r.e2 = edge.e2 + 1;
    doc.relations.push_back(std::move(r));
  }
  return doc;
}

}  // namespace

std::shared_ptr<const Network> read_legacy(const std::filesystem::path& path, const IdMap& ids,
                                           const BuildSettings& settings, const IdSet& exceptions, core::Context& ctx) {
  const auto started = std::chrono::steady_clock::now();
  ctx.stop.throw_if_stopped();
  const std::string bytes = detail::read_input(path);
  detail::ReadCounts counts;
  const auto parsed = detail::parse_pathway_xml(bytes, path, ids, Format::legacy, ctx, counts);
  const auto& in = parsed->interner;

  LDoc doc;
  for (const Entry& e : parsed->entries) {
    LDoc::Entry x;
    for (std::uint32_t i = 0; i < e.ids_count; ++i) x.ids.emplace_back(in.str(parsed->ids[e.ids_first + i]));
    x.type = std::string(in.str(e.type));
    x.group = x.type == "group";
    for (std::uint32_t i = 0; i < e.comps_count; ++i) x.comps.push_back(parsed->comps[e.comps_first + i]);
    doc.entries.emplace(e.local_id, std::move(x));
  }
  auto list = [&](std::uint32_t id) {
    std::vector<std::string> out;
    for (std::uint32_t i = parsed->list_first[id]; i < parsed->list_first[id + 1]; ++i)
      out.emplace_back(in.str(parsed->list_items[i]));
    return out;
  };
  for (const Relation& r : parsed->relations)
    doc.relations.push_back({r.entry1, r.entry2, std::string(in.str(r.type)), list(r.subtypes), list(r.pathways), r.score});

  // Read 1, then the replay of wgx -> rgx y with the same settings and exceptions (02.C5).
  Reader reader(exceptions, settings.split_threshold, settings.filters);
  const LGraph first = reader.read(doc);
  const LGraph graph = reader.read(written(first));

  NetworkInfo info;
  info.reader = "legacy";
  info.legacy = true;
  info.settings = settings;
  info.exceptions.assign(exceptions.ids().begin(), exceptions.ids().end());
  info.id_map = ids.digest();
  info.sources.push_back(parsed->source);
  core::Interner local;
  NetworkBuilder builder(std::move(info), local);
  std::vector<NodeIdx> made(graph.nodes.size());
  for (std::size_t n = 0; n < graph.nodes.size(); ++n) {
    const auto& node = graph.nodes[n];
    if (node.group) {
      std::vector<NodeIdx> comps;
      for (int c : node.comps) comps.push_back(made[static_cast<std::size_t>(c)]);
      made[n] = builder.add_complex(comps);
    } else {
      std::vector<GeneId> members;
      for (const auto& id : node.all_id) members.push_back(local.id(id));
      made[n] = builder.add_node(local.sym(node.type), members);
      if (node.exception) builder.mark_exception(made[n]);
    }
  }
  // Exact duplicates merge here with S4.13's rule (pathways unioned, the larger score), which FuSION 1
  // applied at the corridor copy (GraphManagerNew.cc:9388-9503); the builder merges on identity.
  for (const auto& e : graph.edges) {
    std::vector<Sym> subs, paths;
    for (const auto& s : e.subtypes) subs.push_back(local.sym(s));
    for (const auto& p : e.pathways) paths.push_back(local.sym(p));
    builder.add_edge(made[e.e1], made[e.e2], local.sym(e.type), subs, paths, e.score, bit(EdgeFlag::legacy));
  }
  // Each id's holders: FuSION 1's binding first (its first claim, GraphManagerNew.cc:797-808,
  // 876-887), then the other nodes that hold it, in creation order.
  std::map<std::string, std::vector<int>> holders;
  for (std::size_t n = 0; n < graph.nodes.size(); ++n)
    for (const auto& id : graph.nodes[n].all_id) {
      auto& h = holders[id];
      if (h.empty() || h.back() != static_cast<int>(n)) h.push_back(static_cast<int>(n));
    }
  std::vector<std::pair<GeneId, NodeIdx>> claims;
  for (const auto& [id, rep] : graph.id_to_rep) {
    const GeneId g = local.id(id);
    const auto bound = graph.rep_to_nid.find(rep);
    if (bound != graph.rep_to_nid.end()) claims.emplace_back(g, made[static_cast<std::size_t>(bound->second)]);
    if (const auto h = holders.find(id); h != holders.end())
      for (int n : h->second) claims.emplace_back(g, made[static_cast<std::size_t>(n)]);
  }
  builder.set_claim_order(claims);
  auto net = std::make_shared<Network>(std::move(builder).finish(ctx));

  ctx.events.emit("network.read", core::Json{{"path", detail::display(path)}, {"format", "legacy"},
                                             {"pathway", parsed->source.pathway}, {"entries", parsed->entries.size()},
                                             {"relations", parsed->relations.size()}, {"nodes", net->nodes().size()},
                                             {"edges_before_merge", graph.edges.size()}, {"edges", net->edges().size()},
                                             {"unscored", counts.unscored}, {"ms", detail::elapsed_ms(started)}});
  return net;
}

}  // namespace fusion::network
