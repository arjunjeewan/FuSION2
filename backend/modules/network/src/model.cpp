// fusion network: the graph model, its builder, the id index and views (section 02, "The graph model",
// "Stable keys and order", "Views"; 02.C8 to 02.C11; S2.02 to S2.04, S2.09, S2.10, S2.20, S2.21).
#include <algorithm>
#include <bit>
#include <cstring>
#include <deque>
#include <map>
#include <tuple>
#include <unordered_map>

#include "detail.hpp"

namespace fusion::network {

namespace {
constexpr std::uint16_t kComplex = bit(NodeFlag::complex);
constexpr std::uint16_t kException = bit(NodeFlag::exception);
constexpr std::uint16_t kFamily = bit(NodeFlag::family);
constexpr std::uint16_t kDummy = bit(EdgeFlag::dummy_u_to_d);
constexpr std::uint16_t kComponent = bit(EdgeFlag::component);
constexpr std::uint16_t kUndirected = bit(EdgeFlag::undirected);
constexpr std::uint16_t kEcrel = bit(EdgeFlag::ecrel);
constexpr std::uint16_t kIndirect = bit(EdgeFlag::indirect);
constexpr std::uint16_t kSelfLoop = bit(EdgeFlag::self_loop);
constexpr std::uint16_t kUnscored = bit(EdgeFlag::unscored);
constexpr std::uint16_t kIntrinsic = kEcrel | kIndirect | kSelfLoop | kUnscored;   // add_edge's own

[[noreturn]] void internal(const std::string& message) { throw core::Failure("internal", message); }

// A range of a CSR table whose offsets may or may not carry the final end.
std::pair<std::uint32_t, std::uint32_t> csr_range(const std::vector<std::uint32_t>& first, std::size_t refs,
                                                  std::size_t i) {
  const std::uint32_t begin = first[i];
  const std::uint32_t end = i + 1 < first.size() ? first[i + 1] : static_cast<std::uint32_t>(refs);
  return {begin, std::max(begin, end)};
}
}  // namespace

// ---------------------------------------------------------------- IdIndex

struct IdIndex::Impl {
  std::unordered_map<GeneId, std::pair<std::uint32_t, std::uint32_t>, core::HandleHash> ranges;   // lookups only
  std::vector<NodeIdx> holders;
};

IdIndex::IdIndex() : impl_(std::make_unique<Impl>()) {}
IdIndex::~IdIndex() = default;
IdIndex::IdIndex(IdIndex&&) noexcept = default;
IdIndex& IdIndex::operator=(IdIndex&&) noexcept = default;

std::span<const NodeIdx> IdIndex::holders(GeneId id) const noexcept {
  if (!impl_) return {};
  const auto it = impl_->ranges.find(id);
  if (it == impl_->ranges.end()) return {};
  return {impl_->holders.data() + it->second.first, it->second.second};
}

// ---------------------------------------------------------------- Network

struct Network::Impl {
  core::Interner interner;
  std::vector<Node> nodes;
  std::vector<Edge> edges;
  std::vector<GeneId> members;
  std::vector<NodeIdx> comps;
  std::vector<Sym> list_items;                 // subtype lists and pathway sets, by list id
  std::vector<std::uint32_t> list_first{0};
  std::vector<std::uint32_t> out_first, in_first;
  std::vector<EdgeIdx> out_list, in_list;
  std::unordered_map<GeneId, NodeIdx, core::HandleHash> by_key;   // lookups only
  IdIndex ids;
  NetworkInfo info;
  std::optional<Provenance> provenance;

  std::span<const Sym> list(std::uint32_t id) const noexcept {
    return {list_items.data() + list_first[id], list_first[id + 1] - list_first[id]};
  }
};

Network::Network(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
Network::Network(Network&&) noexcept = default;
Network& Network::operator=(Network&&) noexcept = default;
Network::~Network() = default;

std::span<const Node> Network::nodes() const noexcept { return impl_->nodes; }
std::span<const Edge> Network::edges() const noexcept { return impl_->edges; }

std::span<const GeneId> Network::members(NodeIdx n) const noexcept {
  const Node& node = impl_->nodes[n];
  return {impl_->members.data() + node.members_first, node.members_count};
}
std::span<const NodeIdx> Network::components(NodeIdx n) const noexcept {
  const Node& node = impl_->nodes[n];
  return {impl_->comps.data() + node.comps_first, node.comps_count};
}
std::span<const EdgeIdx> Network::out_edges(NodeIdx n) const noexcept {
  return {impl_->out_list.data() + impl_->out_first[n], impl_->out_first[n + 1] - impl_->out_first[n]};
}
std::span<const EdgeIdx> Network::in_edges(NodeIdx n) const noexcept {
  return {impl_->in_list.data() + impl_->in_first[n], impl_->in_first[n + 1] - impl_->in_first[n]};
}
std::span<const Sym> Network::subtypes(EdgeIdx e) const noexcept { return impl_->list(impl_->edges[e].subtypes); }
std::span<const Sym> Network::pathways(EdgeIdx e) const noexcept { return impl_->list(impl_->edges[e].pathways); }

bool Network::has_subtype(EdgeIdx e, std::string_view name) const noexcept {
  for (Sym s : subtypes(e))
    if (impl_->interner.str(s) == name) return true;
  return false;
}

NodeKey Network::node_key(NodeIdx n) const { return {std::string(impl_->interner.str(impl_->nodes[n].key))}; }

EdgeKey Network::edge_key(EdgeIdx e) const {
  const Edge& edge = impl_->edges[e];
  EdgeKey key{node_key(edge.src), node_key(edge.tgt), std::string(impl_->interner.str(edge.type)), {}};
  for (Sym s : subtypes(e)) key.subtypes.emplace_back(impl_->interner.str(s));
  return key;
}

NodeKind Network::kind(NodeIdx n) const noexcept { return kind_of(impl_->interner.str(impl_->nodes[n].type)); }

Effect Network::effect(EdgeIdx e) const noexcept {
  return has_subtype(e, "inhibition") || has_subtype(e, "repression") ? Effect::inhibition : Effect::activation;
}

std::optional<NodeIdx> Network::find_node(const NodeKey& key) const noexcept {
  const auto id = impl_->interner.find_id(key.value);
  if (!id) return std::nullopt;
  const auto it = impl_->by_key.find(*id);
  if (it == impl_->by_key.end()) return std::nullopt;
  return it->second;
}

namespace {
// Three-way comparison of an edge's (target, type, subtypes) with a key's, by content (02.C10).
int compare_tail(const Network& net, EdgeIdx e, NodeIdx tgt, std::string_view type,
                 const std::vector<std::string>& subtypes) {
  const Edge& edge = net.edges()[e];
  if (edge.tgt != tgt) return edge.tgt < tgt ? -1 : 1;
  const auto& in = net.interner();
  if (const int c = in.str(edge.type).compare(type)) return c < 0 ? -1 : 1;
  const auto subs = net.subtypes(e);
  for (std::size_t i = 0; i < subs.size() && i < subtypes.size(); ++i)
    if (const int c = in.str(subs[i]).compare(subtypes[i])) return c < 0 ? -1 : 1;
  if (subs.size() != subtypes.size()) return subs.size() < subtypes.size() ? -1 : 1;
  return 0;
}
}  // namespace

std::optional<EdgeIdx> Network::find_edge(const EdgeKey& key) const noexcept {
  const auto src = find_node(key.source);
  const auto tgt = find_node(key.target);
  if (!src || !tgt) return std::nullopt;
  const auto out = out_edges(*src);
  std::size_t lo = 0, hi = out.size();
  while (lo < hi) {                                          // binary search: O(log deg)
    const std::size_t mid = lo + (hi - lo) / 2;
    const int c = compare_tail(*this, out[mid], *tgt, key.type, key.subtypes);
    if (c == 0) return out[mid];
    if (c < 0) lo = mid + 1;
    else hi = mid;
  }
  return std::nullopt;
}

// S5.06: edge files name subtypes as a set, since a user cannot know the stored order.
std::vector<EdgeIdx> Network::find_edges(NodeIdx src, NodeIdx tgt,
                                         std::span<const std::string_view> subtype_set) const {
  std::vector<std::string_view> want;
  for (auto s : subtype_set)
    if (s != "dummy_u_to_d") want.push_back(s);
  std::sort(want.begin(), want.end());
  want.erase(std::unique(want.begin(), want.end()), want.end());
  std::vector<EdgeIdx> out;
  for (EdgeIdx e : out_edges(src)) {
    if (impl_->edges[e].tgt != tgt) continue;
    std::vector<std::string_view> have;
    for (Sym s : subtypes(e)) have.push_back(impl_->interner.str(s));
    std::sort(have.begin(), have.end());
    have.erase(std::unique(have.begin(), have.end()), have.end());
    if (have == want) out.push_back(e);
  }
  return out;
}

// 07.C10 and section 02's node labels: a single member shows its symbol or key; a family its
// representative's plus the count of the others ("RRAS2+5"); a complex its own symbol, else its
// components' labels in key order, "[MAX+MYC]", "[STAT1+6]". Derived, never stored (02.C16).
std::string Network::label(NodeIdx n, const Maps& maps) const {
  const Node& node = impl_->nodes[n];
  const std::string_view key = impl_->interner.str(node.key);
  if (node.flags & kComplex) {
    if (auto symbol = maps.symbols.find(key)) return std::string(*symbol);
    std::string out = "[";
    const auto comps = components(n);
    for (std::size_t i = 0; i < comps.size(); ++i) {
      if (i) out += '+';
      out += label(comps[i], maps);
    }
    return out + ']';
  }
  std::string out(maps.symbols.label_or_id(key));
  if (node.members_count > 1) out += '+' + std::to_string(node.members_count - 1);
  return out;
}

// Entry 205: a KGML's root title names its pathway; any other id takes the pathway-name map's entry,
// and an id with neither prints as itself (S1.19).
std::string Network::pathway_label(std::string_view pathway, const Maps& maps) const {
  for (const SourceRef& s : impl_->info.sources)
    if (s.pathway == pathway && !s.title.empty()) return s.title;
  if (auto name = maps.pathway_names.find(pathway)) return std::string(*name);
  return std::string(pathway);
}

const IdIndex& Network::ids() const noexcept { return impl_->ids; }
const NetworkInfo& Network::info() const noexcept { return impl_->info; }
const Provenance* Network::provenance() const noexcept {
  return impl_->provenance ? &*impl_->provenance : nullptr;
}
const core::Interner& Network::interner() const noexcept { return impl_->interner; }

// The model fields only: keys, types, members, components, flags, edges with their subtypes,
// pathways and scores, and the holder order of ids with several holders. Labels, titles, paths and
// the header are left out, so networks compare across runs and machines (section 02, Determinism).
core::Sha256 Network::content_hash() const {
  const auto& in = impl_->interner;
  core::Sha256Builder h;
  h.update(std::string_view("fusion-network-model/1\n"));
  for (NodeIdx n = 0; n < impl_->nodes.size(); ++n) {
    const Node& node = impl_->nodes[n];
    std::string line = "n\t";
    line += in.str(node.key);
    line += '\t';
    line += in.str(node.type);
    line += '\t' + std::to_string(node.flags) + '\t';
    for (GeneId m : members(n)) (line += in.str(m)) += ' ';
    line += '\t';
    for (NodeIdx c : components(n)) (line += in.str(impl_->nodes[c].key)) += ' ';
    line += '\n';
    h.update(line);
  }
  for (EdgeIdx e = 0; e < impl_->edges.size(); ++e) {
    const Edge& edge = impl_->edges[e];
    std::string line = "e\t";
    line += in.str(impl_->nodes[edge.src].key);
    line += '\t';
    line += in.str(impl_->nodes[edge.tgt].key);
    line += '\t';
    line += in.str(edge.type);
    line += '\t';
    for (Sym s : subtypes(e)) (line += in.str(s)) += ',';
    line += '\t';
    for (Sym p : pathways(e)) (line += in.str(p)) += ' ';
    line += '\t' + std::to_string(edge.flags) + '\t' + detail::format_score(edge.score) + '\n';
    h.update(line);
  }
  std::vector<std::string_view> shared;                     // ids with several holders (legacy)
  for (NodeIdx n = 0; n < impl_->nodes.size(); ++n)
    for (GeneId m : members(n))
      if (impl_->ids.holders(m).size() > 1) shared.push_back(in.str(m));
  std::sort(shared.begin(), shared.end());
  shared.erase(std::unique(shared.begin(), shared.end()), shared.end());
  for (auto id : shared) {
    std::string line = "c\t";
    line += id;
    for (NodeIdx holder : impl_->ids.holders(*in.find_id(id))) (line += '\t') += in.str(impl_->nodes[holder].key);
    h.update(line + '\n');
  }
  return h.finish();
}

// ---------------------------------------------------------------- the builder

struct NetworkBuilder::Impl {
  NetworkInfo info;
  core::Interner* caller = nullptr;

  // Strings, interned locally; std::deque keeps each string where it is.
  std::deque<std::string> strings;
  std::unordered_map<std::string_view, std::uint32_t, detail::TextHash, detail::TextEq> string_ids;
  std::unordered_map<GeneId, std::uint32_t, core::HandleHash> from_gene;   // caller handle -> string id
  std::unordered_map<Sym, std::uint32_t, core::HandleHash> from_sym;
  std::uint32_t s_group = 0, s_pprel = 0, s_component = 0, s_dummy = 0, s_ecrel = 0, s_indirect = 0;

  struct BNode {
    std::uint32_t type = 0;
    std::vector<std::uint32_t> members;          // string ids
    std::vector<std::uint32_t> comps;            // builder node indices, sorted and unique
    std::uint16_t flags = 0;
    bool complex = false;
  };
  std::vector<BNode> nodes;
  std::map<std::vector<std::uint32_t>, std::uint32_t> complex_by_comps;

  std::vector<std::vector<std::uint32_t>> lists;   // interned subtype lists (string ids, in order)
  std::map<std::vector<std::uint32_t>, std::uint32_t> list_ids;

  struct BEdge {
    std::uint32_t src = 0, tgt = 0, type = 0, subtypes = 0;
    std::vector<std::uint32_t> pathways;         // string ids, unique
    std::uint16_t flags = 0;
    double score = 0;
    std::vector<Provenance::Ref> prov;           // doc: index into docs
  };
  std::vector<BEdge> edges;
  struct Identity {
    std::uint32_t src, tgt, type, subtypes;
    bool operator==(const Identity&) const = default;
  };
  struct IdentityHash {
    std::size_t operator()(const Identity& k) const noexcept {
      std::uint64_t h = 1469598103934665603ULL;
      for (std::uint32_t v : {k.src, k.tgt, k.type, k.subtypes}) { h ^= v; h *= 1099511628211ULL; }
      return static_cast<std::size_t>(h);
    }
  };
  std::unordered_map<Identity, std::uint32_t, IdentityHash> edge_ids;   // dropped by finish()

  std::vector<std::shared_ptr<const PathwayDoc>> docs;
  std::map<const PathwayDoc*, std::uint32_t> doc_index;                 // lookups only
  std::vector<std::vector<Provenance::Ref>> node_prov;
  bool has_provenance = false;

  std::map<std::pair<const Network*, NodeIdx>, std::uint32_t> copied;   // lookups only
  std::vector<std::pair<std::uint32_t, std::uint32_t>> claims;          // (string id, node)

  std::uint32_t str(std::string_view s) {
    const auto it = string_ids.find(s);
    if (it != string_ids.end()) return it->second;
    strings.emplace_back(s);
    const auto id = static_cast<std::uint32_t>(strings.size() - 1);
    string_ids.emplace(std::string_view(strings.back()), id);
    return id;
  }
  std::uint32_t gene(GeneId g) {
    const auto it = from_gene.find(g);
    if (it != from_gene.end()) return it->second;
    return from_gene[g] = str(caller->str(g));
  }
  std::uint32_t sym(Sym s) {
    const auto it = from_sym.find(s);
    if (it != from_sym.end()) return it->second;
    return from_sym[s] = str(caller->str(s));
  }
  std::uint32_t list(const std::vector<std::uint32_t>& items) {
    const auto [it, inserted] = list_ids.try_emplace(items, static_cast<std::uint32_t>(lists.size()));
    if (inserted) lists.push_back(items);
    return it->second;
  }
  void check_node(NodeIdx n) const {
    if (n >= nodes.size()) internal("network builder: no node " + std::to_string(n));
  }
  std::uint32_t doc(const std::shared_ptr<const PathwayDoc>& d) {
    const auto [it, inserted] = doc_index.try_emplace(d.get(), static_cast<std::uint32_t>(docs.size()));
    if (inserted) docs.push_back(d);
    return it->second;
  }
  std::uint32_t new_node(BNode node) {
    nodes.push_back(std::move(node));
    node_prov.emplace_back();
    return static_cast<std::uint32_t>(nodes.size() - 1);
  }
  std::uint32_t complex(std::vector<std::uint32_t> comps) {
    std::sort(comps.begin(), comps.end());
    comps.erase(std::unique(comps.begin(), comps.end()), comps.end());
    if (comps.empty()) internal("network builder: a complex needs a component");
    for (auto c : comps) check_node(c);
    const auto it = complex_by_comps.find(comps);
    if (it != complex_by_comps.end()) return it->second;             // reuse (S2.09)
    BNode node;
    node.type = s_group;
    node.comps = comps;
    node.complex = true;
    const auto idx = new_node(std::move(node));
    complex_by_comps.emplace(std::move(comps), idx);
    return idx;
  }

  // S2.04, S2.10, S2.20: one edge per identity (source, target, type, ordered subtypes without the
  // tag); a repeat unions pathways, keeps the larger score and clears unscored when either is scored.
  // FuSION 1 tested identity by a linear scan of the source's out-list (GraphManagerNew.cc:676-694)
  // and lost a skipped duplicate's score (XMLParser.cc:1493-1501); S2.10's decision keeps the larger.
  std::uint32_t edge(std::uint32_t src, std::uint32_t tgt, std::uint32_t type, std::vector<std::uint32_t> subs,
                     std::vector<std::uint32_t> paths, std::optional<double> score, std::uint16_t flags) {
    check_node(src);
    check_node(tgt);
    flags = static_cast<std::uint16_t>(flags & ~kIntrinsic);
    std::vector<std::uint32_t> kept;
    kept.reserve(subs.size());
    for (auto s : subs) {
      if (s == s_dummy) flags |= kDummy;                          // the tag is a flag (S2.21)
      else kept.push_back(s);
    }
    if (type == s_ecrel) flags |= kEcrel;
    if (std::find(kept.begin(), kept.end(), s_indirect) != kept.end()) flags |= kIndirect;
    if (src == tgt) flags |= kSelfLoop;
    std::sort(paths.begin(), paths.end());
    paths.erase(std::unique(paths.begin(), paths.end()), paths.end());
    const std::uint32_t list_id = list(kept);
    const Identity key{src, tgt, type, list_id};
    const auto [it, inserted] = edge_ids.try_emplace(key, static_cast<std::uint32_t>(edges.size()));
    if (inserted) {
      BEdge e;
      e.src = src;
      e.tgt = tgt;
      e.type = type;
      e.subtypes = list_id;
      e.pathways = std::move(paths);
      e.flags = static_cast<std::uint16_t>(flags | (score ? 0 : kUnscored));
      e.score = score.value_or(0.0);
      edges.push_back(std::move(e));
      return it->second;
    }
    BEdge& e = edges[it->second];
    std::vector<std::uint32_t> merged;
    std::set_union(e.pathways.begin(), e.pathways.end(), paths.begin(), paths.end(), std::back_inserter(merged));
    e.pathways = std::move(merged);
    e.score = std::max(e.score, score.value_or(0.0));            // unscored counts as 0 (S2.10)
    if (score) e.flags = static_cast<std::uint16_t>(e.flags & ~kUnscored);
    e.flags |= flags;
    return it->second;
  }
};

NetworkBuilder::NetworkBuilder(NetworkInfo info, core::Interner& interner) : impl_(std::make_unique<Impl>()) {
  impl_->info = std::move(info);
  impl_->caller = &interner;
  impl_->s_group = impl_->str("group");
  impl_->s_pprel = impl_->str("PPrel");
  impl_->s_component = impl_->str("component");
  impl_->s_dummy = impl_->str("dummy_u_to_d");
  impl_->s_ecrel = impl_->str("ECrel");
  impl_->s_indirect = impl_->str("indirect");
}
NetworkBuilder::NetworkBuilder(NetworkBuilder&&) noexcept = default;
NetworkBuilder& NetworkBuilder::operator=(NetworkBuilder&&) noexcept = default;
NetworkBuilder::~NetworkBuilder() = default;

// The node's key is its smallest member in byte order (S2.07), worked out when the build finishes.
NodeIdx NetworkBuilder::add_node(Sym type, std::span<const GeneId> members) {
  if (members.empty()) internal("network builder: a node needs a member");
  Impl::BNode node;
  node.type = impl_->sym(type);
  for (GeneId m : members) node.members.push_back(impl_->gene(m));
  return impl_->new_node(std::move(node));
}

// S2.08's fix: every id of a box joins the node the box resolves to (FuSION 1 gave only a new node
// its ids, Linux/src/XMLParser.cc:991-1073).
void NetworkBuilder::add_members(NodeIdx n, std::span<const GeneId> ids) {
  impl_->check_node(n);
  auto& node = impl_->nodes[n];
  if (node.complex) internal("network builder: a complex's only member is its key");
  for (GeneId m : ids) node.members.push_back(impl_->gene(m));
}

// S2.09: one complex per distinct set of component nodes; a repeat returns the complex already made
// (FuSION 1 scanned every node for it, Linux/src/GraphManagerNew.cc:2421-2436).
NodeIdx NetworkBuilder::add_complex(std::span<const NodeIdx> components) {
  return impl_->complex(std::vector<std::uint32_t>(components.begin(), components.end()));
}

void NetworkBuilder::mark_exception(NodeIdx n) {
  impl_->check_node(n);
  impl_->nodes[n].flags |= kException;
}

EdgeIdx NetworkBuilder::add_edge(NodeIdx src, NodeIdx tgt, Sym type, std::span<const Sym> subtypes,
                                 std::span<const Sym> pathways, std::optional<double> score,
                                 std::uint16_t extra_flags) {
  std::vector<std::uint32_t> subs, paths;
  for (Sym s : subtypes) subs.push_back(impl_->sym(s));
  for (Sym p : pathways) paths.push_back(impl_->sym(p));
  return impl_->edge(src, tgt, impl_->sym(type), std::move(subs), std::move(paths), score, extra_flags);
}

// S2.09: two PPrel edges per (complex, component), component to complex and back, subtype
// `component`, with the document's own pathway id rather than the first edge's list
// (Linux/src/GraphManagerNew.cc:3981-4071, 3998). finish() tags both directions.
void NetworkBuilder::add_component_edges(NodeIdx complex, Sym pathway) {
  impl_->check_node(complex);
  if (!impl_->nodes[complex].complex) internal("network builder: component edges need a complex");
  const auto comps = impl_->nodes[complex].comps;
  const std::uint32_t p = impl_->sym(pathway);
  for (std::uint32_t c : comps) {
    impl_->edge(c, complex, impl_->s_pprel, {impl_->s_component}, {p}, std::nullopt, kComponent);
    impl_->edge(complex, c, impl_->s_pprel, {impl_->s_component}, {p}, std::nullopt, kComponent);
  }
}

// Item 2B.11: a node of another network with its members (a complex with its components, copied
// first), for 03's split. Copying one node twice gives the same node.
NodeIdx NetworkBuilder::copy_node(const Network& from, NodeIdx n) {
  const auto key = std::make_pair(&from, n);
  if (const auto it = impl_->copied.find(key); it != impl_->copied.end()) return it->second;
  const Node& node = from.nodes()[n];
  const auto& in = from.interner();
  std::uint32_t idx = 0;
  if (node.flags & kComplex) {
    std::vector<std::uint32_t> comps;
    for (NodeIdx c : from.components(n)) comps.push_back(copy_node(from, c));
    idx = impl_->complex(std::move(comps));
  } else {
    Impl::BNode copy;
    copy.type = impl_->str(in.str(node.type));
    for (GeneId m : from.members(n)) copy.members.push_back(impl_->str(in.str(m)));
    idx = impl_->new_node(std::move(copy));
  }
  impl_->nodes[idx].flags |= static_cast<std::uint16_t>(node.flags & kException);
  if (const Provenance* p = from.provenance(); p && n + 1 <= p->node_first.size()) {
    const auto [b, e] = csr_range(p->node_first, p->node_refs.size(), n);
    for (std::uint32_t r = b; r < e; ++r) {
      Provenance::Ref ref = p->node_refs[r];
      ref.doc = impl_->doc(p->docs[ref.doc]);
      impl_->node_prov[idx].push_back(ref);
      impl_->has_provenance = true;
    }
  }
  impl_->copied.emplace(key, idx);
  return idx;
}

// Item 2B.11: an edge of another network with its flags, pathways, score and provenance.
EdgeIdx NetworkBuilder::copy_edge(const Network& from, EdgeIdx e, NodeIdx src, NodeIdx tgt) {
  const Edge& edge = from.edges()[e];
  const auto& in = from.interner();
  std::vector<std::uint32_t> subs, paths;
  for (Sym s : from.subtypes(e)) subs.push_back(impl_->str(in.str(s)));
  for (Sym p : from.pathways(e)) paths.push_back(impl_->str(in.str(p)));
  std::optional<double> score;  // assigned, not built from a ?: , which GCC's -O3 flow analysis misreads
  if (!(edge.flags & kUnscored)) score = edge.score;
  const auto idx = impl_->edge(src, tgt, impl_->str(in.str(edge.type)), std::move(subs), std::move(paths), score,
                               static_cast<std::uint16_t>(edge.flags & ~kIntrinsic));
  if (const Provenance* p = from.provenance(); p && e + 1 <= p->edge_first.size()) {
    const auto [b, end] = csr_range(p->edge_first, p->edge_refs.size(), e);
    for (std::uint32_t r = b; r < end; ++r) {
      Provenance::Ref ref = p->edge_refs[r];
      ref.doc = impl_->doc(p->docs[ref.doc]);
      impl_->edges[idx].prov.push_back(ref);
      impl_->has_provenance = true;
    }
  }
  return idx;
}

// Legacy only (S2.03, 02.C5): each id's holders in FuSION 1's claim order, the first claim first.
void NetworkBuilder::set_claim_order(std::span<const std::pair<GeneId, NodeIdx>> claims) {
  impl_->claims.clear();
  for (const auto& [id, node] : claims) {
    impl_->check_node(node);
    impl_->claims.emplace_back(impl_->gene(id), node);
  }
}

void NetworkBuilder::attach(Provenance p) {
  auto& I = *impl_;
  I.has_provenance = true;
  std::vector<std::uint32_t> doc_map;
  for (const auto& d : p.docs) doc_map.push_back(I.doc(d));
  auto add = [&](const std::vector<std::uint32_t>& first, const std::vector<Provenance::Ref>& refs,
                 std::size_t count, auto&& sink) {
    for (std::size_t i = 0; i < first.size() && i < count; ++i) {
      const auto [b, e] = csr_range(first, refs.size(), i);
      for (std::uint32_t r = b; r < e; ++r) {
        Provenance::Ref ref = refs[r];
        if (ref.doc >= doc_map.size()) internal("provenance names a document it does not hold");
        ref.doc = doc_map[ref.doc];
        sink(i, ref);
      }
    }
  };
  if (p.edge_first.size() > I.edges.size() + 1 || p.node_first.size() > I.nodes.size() + 1)
    internal("provenance covers more edges or nodes than the builder holds");
  add(p.edge_first, p.edge_refs, I.edges.size(), [&](std::size_t i, Provenance::Ref r) { I.edges[i].prov.push_back(r); });
  add(p.node_first, p.node_refs, I.nodes.size(), [&](std::size_t i, Provenance::Ref r) { I.node_prov[i].push_back(r); });
}

// Sets the tag once (S2.21), sorts nodes and edges into canonical order (02.C10), builds the
// adjacency, the key index and the id index (02.C8), and drops the identity hash.
Network NetworkBuilder::finish(core::Context& ctx) && {
  ctx.stop.throw_if_stopped();
  Impl& I = *impl_;
  const auto& S = I.strings;
  const std::size_t N = I.nodes.size();
  auto by_text = [&](std::uint32_t a, std::uint32_t b) { return S[a] < S[b]; };

  // Members sorted by content; keys; complex keys from their components' keys (S2.07, S2.09;
  // Linux/src/GraphManagerNew.cc:2438-2466). Components are always made before their complex.
  std::vector<std::string> keys(N);
  for (std::size_t i = 0; i < N; ++i) {
    auto& node = I.nodes[i];
    if (node.complex) {
      std::vector<std::string> parts;
      for (auto c : node.comps) parts.push_back(keys[c]);
      std::sort(parts.begin(), parts.end());
      parts.erase(std::unique(parts.begin(), parts.end()), parts.end());
      keys[i] = "c_s_" + detail::join(parts, "_") + "_c_e";
    } else {
      std::sort(node.members.begin(), node.members.end(), by_text);
      node.members.erase(std::unique(node.members.begin(), node.members.end()), node.members.end());
      keys[i] = S[node.members.front()];
    }
  }
  std::vector<std::uint32_t> order(N);
  for (std::uint32_t i = 0; i < N; ++i) order[i] = i;
  std::sort(order.begin(), order.end(), [&](std::uint32_t a, std::uint32_t b) { return keys[a] < keys[b]; });
  for (std::size_t p = 1; p < N; ++p)
    if (keys[order[p]] == keys[order[p - 1]])
      throw core::Failure("network.duplicate_key", "two nodes have the key " + keys[order[p]]);
  std::vector<std::uint32_t> perm(N);
  for (std::uint32_t p = 0; p < N; ++p) perm[order[p]] = p;

  auto out = std::make_unique<Network::Impl>();
  auto& in = out->interner;
  out->nodes.resize(N);
  for (std::uint32_t p = 0; p < N; ++p) {
    const auto& b = I.nodes[order[p]];
    Node& node = out->nodes[p];
    node.key = in.id(keys[order[p]]);
    node.members_first = static_cast<std::uint32_t>(out->members.size());
    if (b.complex) {
      out->members.push_back(node.key);                      // a complex's only member is its key
    } else {
      for (auto m : b.members) out->members.push_back(in.id(S[m]));
    }
    node.members_count = static_cast<std::uint32_t>(out->members.size()) - node.members_first;
  }
  for (std::uint32_t p = 0; p < N; ++p) {
    const auto& b = I.nodes[order[p]];
    Node& node = out->nodes[p];
    node.type = in.sym(S[b.type]);
    node.comps_first = static_cast<std::uint32_t>(out->comps.size());
    std::vector<NodeIdx> comps;
    for (auto c : b.comps) comps.push_back(perm[c]);
    std::sort(comps.begin(), comps.end());                   // key order
    comps.erase(std::unique(comps.begin(), comps.end()), comps.end());
    out->comps.insert(out->comps.end(), comps.begin(), comps.end());
    node.comps_count = static_cast<std::uint32_t>(comps.size());
    node.flags = static_cast<std::uint16_t>(b.flags & kException);
    if (b.complex) node.flags |= kComplex;
    else if (node.members_count > 1) node.flags |= kFamily;
  }

  // Edges: endpoints renumbered, the tag set once (S2.09, S2.10, S2.21), canonical order (02.C10).
  struct Pending { std::uint32_t builder; NodeIdx src, tgt; std::uint16_t flags; };
  std::vector<Pending> pending;
  pending.reserve(I.edges.size());
  for (std::uint32_t e = 0; e < I.edges.size(); ++e) {
    const auto& b = I.edges[e];
    Pending x{e, perm[b.src], perm[b.tgt], b.flags};
    x.flags = static_cast<std::uint16_t>(x.flags & ~kSelfLoop);
    if (x.src == x.tgt) x.flags |= kSelfLoop;
    if (x.flags & kComponent) {
      x.flags |= kDummy;                                     // both directions (Linux/src/GraphManagerNew.cc:4023, 4057)
    } else if (x.flags & kUndirected) {
      x.flags = static_cast<std::uint16_t>(x.flags & ~kDummy);
      if (x.src != x.tgt && keys[b.src] > keys[b.tgt]) x.flags |= kDummy;   // XMLParser.cc:1528, 1585
    }
    pending.push_back(x);
  }
  auto list_less = [&](std::uint32_t a, std::uint32_t b) {
    return std::lexicographical_compare(I.lists[a].begin(), I.lists[a].end(), I.lists[b].begin(), I.lists[b].end(),
                                        by_text);
  };
  std::sort(pending.begin(), pending.end(), [&](const Pending& x, const Pending& y) {
    if (x.src != y.src) return x.src < y.src;
    if (x.tgt != y.tgt) return x.tgt < y.tgt;
    const auto& ex = I.edges[x.builder];
    const auto& ey = I.edges[y.builder];
    if (ex.type != ey.type) return S[ex.type] < S[ey.type];
    return list_less(ex.subtypes, ey.subtypes);
  });

  std::map<std::vector<Sym>, std::uint32_t> list_index;        // canonical list ids, first use first
  auto intern_list = [&](const std::vector<Sym>& items) {
    const auto [it, inserted] = list_index.try_emplace(items, static_cast<std::uint32_t>(list_index.size()));
    if (inserted) {
      out->list_items.insert(out->list_items.end(), items.begin(), items.end());
      out->list_first.push_back(static_cast<std::uint32_t>(out->list_items.size()));
    }
    return it->second;
  };
  std::vector<std::uint32_t> edge_perm(I.edges.size());
  out->edges.reserve(pending.size());
  for (const Pending& x : pending) {
    const auto& b = I.edges[x.builder];
    Edge edge;
    edge.src = x.src;
    edge.tgt = x.tgt;
    edge.type = in.sym(S[b.type]);
    std::vector<Sym> subs;
    for (auto s : I.lists[b.subtypes]) subs.push_back(in.sym(S[s]));
    std::vector<std::uint32_t> paths = b.pathways;
    std::sort(paths.begin(), paths.end(), by_text);
    std::vector<Sym> psyms;
    for (auto p : paths) psyms.push_back(in.sym(S[p]));
    edge.subtypes = intern_list(subs);
    edge.pathways = intern_list(psyms);
    edge.flags = x.flags;
    edge.score = b.score;
    edge_perm[x.builder] = static_cast<std::uint32_t>(out->edges.size());
    out->edges.push_back(edge);
  }

  // Adjacency (02.C8): offsets and edge indices, ascending EdgeIdx in both directions.
  const std::size_t E = out->edges.size();
  out->out_first.assign(N + 1, 0);
  out->in_first.assign(N + 1, 0);
  for (const Edge& e : out->edges) { ++out->out_first[e.src + 1]; ++out->in_first[e.tgt + 1]; }
  for (std::size_t n = 0; n < N; ++n) {
    out->out_first[n + 1] += out->out_first[n];
    out->in_first[n + 1] += out->in_first[n];
  }
  out->out_list.resize(E);
  out->in_list.resize(E);
  {
    std::vector<std::uint32_t> o(out->out_first.begin(), out->out_first.end() - 1);
    std::vector<std::uint32_t> i(out->in_first.begin(), out->in_first.end() - 1);
    for (EdgeIdx e = 0; e < E; ++e) {
      out->out_list[o[out->edges[e].src]++] = e;
      out->in_list[i[out->edges[e].tgt]++] = e;
    }
  }

  // Keys and the id index (S2.03): one holder per id, except in a legacy network, whose holders
  // follow FuSION 1's claim order, the first claim first.
  for (NodeIdx n = 0; n < N; ++n) out->by_key.emplace(out->nodes[n].key, n);
  std::map<std::uint32_t, std::vector<NodeIdx>> holders;     // by handle value: deterministic
  auto hold = [&](GeneId id, NodeIdx n) {
    auto& h = holders[id.value];
    if (std::find(h.begin(), h.end(), n) == h.end()) h.push_back(n);
  };
  for (const auto& [sid, node] : I.claims) hold(in.id(S[sid]), perm[node]);
  for (NodeIdx n = 0; n < N; ++n) {
    const Node& node = out->nodes[n];
    for (std::uint32_t m = 0; m < node.members_count; ++m) hold(out->members[node.members_first + m], n);
  }
  auto& index = *out->ids.impl_;
  for (const auto& [value, h] : holders) {
    index.ranges.emplace(GeneId{value}, std::make_pair(static_cast<std::uint32_t>(index.holders.size()),
                                                        static_cast<std::uint32_t>(h.size())));
    index.holders.insert(index.holders.end(), h.begin(), h.end());
  }

  // Provenance (2B.11): documents in canonical order (pathway id, then SHA-256), refs per canonical
  // edge and node, each list sorted and unique.
  if (I.has_provenance) {
    Provenance prov;
    std::vector<std::uint32_t> doc_order(I.docs.size());
    for (std::uint32_t d = 0; d < doc_order.size(); ++d) doc_order[d] = d;
    std::stable_sort(doc_order.begin(), doc_order.end(), [&](std::uint32_t a, std::uint32_t b) {
      const auto& sa = I.docs[a]->source();
      const auto& sb = I.docs[b]->source();
      if (sa.pathway != sb.pathway) return sa.pathway < sb.pathway;
      return sa.sha256 < sb.sha256;
    });
    std::vector<std::uint32_t> doc_perm(I.docs.size());
    for (std::uint32_t d = 0; d < doc_order.size(); ++d) {
      doc_perm[doc_order[d]] = d;
      prov.docs.push_back(I.docs[doc_order[d]]);
    }
    auto pack = [&](std::vector<Provenance::Ref> refs, std::vector<std::uint32_t>& first,
                    std::vector<Provenance::Ref>& table) {
      for (auto& r : refs) r.doc = doc_perm[r.doc];
      std::sort(refs.begin(), refs.end(), [](const Provenance::Ref& a, const Provenance::Ref& b) {
        return std::tie(a.doc, a.item, a.kind) < std::tie(b.doc, b.item, b.kind);
      });
      refs.erase(std::unique(refs.begin(), refs.end(),
                             [](const Provenance::Ref& a, const Provenance::Ref& b) {
                               return a.doc == b.doc && a.item == b.item && a.kind == b.kind;
                             }),
                 refs.end());
      table.insert(table.end(), refs.begin(), refs.end());
      first.push_back(static_cast<std::uint32_t>(table.size()));
    };
    std::vector<std::uint32_t> edge_order(E);
    for (std::uint32_t b = 0; b < I.edges.size(); ++b) edge_order[edge_perm[b]] = b;
    prov.edge_first.push_back(0);
    for (EdgeIdx e = 0; e < E; ++e) pack(I.edges[edge_order[e]].prov, prov.edge_first, prov.edge_refs);
    prov.node_first.push_back(0);
    for (NodeIdx n = 0; n < N; ++n) pack(I.node_prov[order[n]], prov.node_first, prov.node_refs);
    out->provenance = std::move(prov);
  }

  out->info = std::move(I.info);
  std::sort(out->info.sources.begin(), out->info.sources.end(), [](const SourceRef& a, const SourceRef& b) {
    if (a.pathway != b.pathway) return a.pathway < b.pathway;
    return a.sha256 < b.sha256;
  });
  std::sort(out->info.exceptions.begin(), out->info.exceptions.end());
  out->info.exceptions.erase(std::unique(out->info.exceptions.begin(), out->info.exceptions.end()),
                             out->info.exceptions.end());
  impl_.reset();                                              // the identity hash goes with it
  return Network(std::move(out));
}

// ---------------------------------------------------------------- views (02.C9)

std::size_t Bitset::count() const noexcept {
  std::size_t n = 0;
  for (std::uint64_t w : words_) n += static_cast<std::size_t>(std::popcount(w));
  return n;
}

GraphView::GraphView(std::shared_ptr<const Network> network) : network_(std::move(network)) {
  if (!network_) internal("a view needs a network");
}

GraphView::GraphView(std::shared_ptr<const Network> network, Bitset nodes, Bitset edges)
    : network_(std::move(network)), nodes_(std::move(nodes)), edges_(std::move(edges)) {
  if (!network_) internal("a view needs a network");
  if (nodes_->size() != network_->nodes().size() || edges_->size() != network_->edges().size())
    internal("a view's bitsets must match its network's sizes");
}

std::size_t GraphView::node_count() const noexcept {
  return nodes_ ? nodes_->count() : network_->nodes().size();
}
std::size_t GraphView::edge_count() const noexcept {
  return edges_ ? edges_->count() : network_->edges().size();
}

}  // namespace fusion::network
